#include "ScoutingManager.h"
#include "ProtoBotCommander.h"
#include <algorithm>
#include <cmath>
#include <climits>

using namespace BWAPI;
using namespace BWEM;

namespace {
    inline double deg2rad(double d) { return d * 3.1415926535 / 180.0; }
    inline Position orbitPoint(const Position& c, int r, int aDeg) {
        return Position(c.x + int(r * std::cos(deg2rad(aDeg))),
                        c.y + int(r * std::sin(deg2rad(aDeg))));
    }

    static inline int mapWpx() { return BWAPI::Broodwar->mapWidth() * 32; }
    static inline int mapHpx() { return BWAPI::Broodwar->mapHeight() * 32; }
}



ScoutingManager::ScoutingManager(ProtoBotCommander* commander)
: commanderRef(commander) {}

void ScoutingManager::onStart() {
    if (!Map::Instance().Initialized()) {
        Broodwar->printf("ScoutingManager: BWEM not initialized.");
        return;
    }
    // build list of enemy-start candidates (excluding ours)
    buildStartTargets();
    enemyMainTile.reset();
    enemyMainPos = Positions::Invalid;
    gasStealDone = false;
    targetGeyser = nullptr;
    nextGasStealRetryFrame = 0;
    lastMoveIssueFrame = 0;
    orbitWaypoints.clear();
    orbitIdx = 0;
    dwellUntilFrame = 0;

    // We don't auto-steal a worker anymore. We wait until assignScout() is called.
    state = startTargets.empty() ? State::Done : State::Search;
}

void ScoutingManager::assignScout(BWAPI::Unit unit) {
    if (!unit || !unit->exists()) return;
    scout = unit;
    Broodwar->printf("[Scouting] Assigned scout: %s (id=%d)", scout->getType().c_str(), scout->getID());
    lastHP = scout->getHitPoints();
    lastShields = scout->getShields();
    // Reset state machine when we get a new scout
    state = startTargets.empty() ? State::Done : State::Search;
}

void ScoutingManager::onUnitDestroy(BWAPI::Unit unit) {
    if (unit && scout && unit == scout) {
        Broodwar->printf("[Scouting] Scout destroyed. Clearing assignment.");
        scout = nullptr;
        state = State::Done;
    }
}

void ScoutingManager::onFrame() {
    if (!scout || !scout->exists() || state == State::Done) return;

    const int now = BWAPI::Broodwar->getFrameCount();
    const int hp = scout->getHitPoints();
    const int sh = scout->getShields();
    const bool tookDamage = (lastHP >= 0 && lastShields >= 0) && (hp + sh) < (lastHP + lastShields);

    // "imminent" = under attack OR hostile in striking distance
    const bool imminent = scout->isUnderAttack() || threatenedNow();

    if ((tookDamage || imminent) && (now - lastThreatFrame >= kThreatRearmFrames)) {
        lastThreatFrame = now;
        ensureOrbitWaypoints();
        const auto goal = currentOrbitPoint();

        if (!hasPlannedPath() || scout->getDistance(currentPlannedWaypoint()) < 80) {
            if (hasPlannedPath()) plannedPath.erase(plannedPath.begin());
            // (re)plan if empty or we just reached a node
            if (!hasPlannedPath()) planTerrainPathTo(currentOrbitPoint());
        }
        const auto wp = currentPlannedWaypoint();
        if (wp.isValid()) issueMoveToward(wp, /*reissue*/48);

        state = State::Orbit;
        // update lasts right away so we don't retrigger on the same frame
        lastHP = hp; lastShields = sh;
        return;                      
    }

    switch (state) {
    case State::Search: {
        // go through start locations until we see a building
        if (nextTarget >= (int)startTargets.size()) { state = State::Done; break; }

        const auto tp = startTargets[nextTarget];
        const Position goal(tp);
        issueMoveToward(goal);

        if (scout->getDistance(goal) <= kCloseEnoughToTarget) {
            if (seeAnyEnemyBuildingNear(goal, 400)) {
                enemyMainTile = tp;
                enemyMainPos  = goal;
                Broodwar->printf("[Scouting] Enemy main at (%d,%d)", tp.x, tp.y);
                state = State::GasSteal;
                break;
            }
            ++nextTarget;
        }
        break;
    }

    case State::GasSteal: {
        // Try once per few frames; if a refinery already exists → done
        if (tryGasSteal()) break;

        // Gas steal considered complete once *any* refinery exists on the target geyser
        if (gasStealDone) {
            state = State::Harass;
        }
        break;
    }

    case State::Harass: {
        // After gas-steal, poke workers near enemy main. If no viable target, orbit.
        if (enemyMainPos.isValid() && tryHarassWorker()) break;

        state = State::Orbit;
        break;
    }

    case State::Orbit: {
        ensureOrbitWaypoints();
        issueMoveToward(currentOrbitPoint(), /*reissueDist*/64, false);
        advanceOrbitIfArrived();
        break;
    }

    case State::Done:
    default:
        break;
    }
}

void ScoutingManager::setEnemyMain(const TilePosition& tp) {
    enemyMainTile = tp;
    enemyMainPos = Position(tp);
    // If manually set, jump straight to gas steal priority
    state = State::GasSteal;
}

/* ---------- helpers ---------- */

void ScoutingManager::buildStartTargets() {
    startTargets.clear();
    const TilePosition myStart = Broodwar->self()->getStartLocation();
    for (const auto& area : Map::Instance().Areas()) {
        for (const auto& base : area.Bases()) {
            if (base.Starting() && base.Location() != myStart) {
                startTargets.push_back(base.Location());
            }
        }
    }
    // simple sort by air distance from us
    std::sort(startTargets.begin(), startTargets.end(),
        [&](const TilePosition& a, const TilePosition& b) {
            return Position(myStart).getDistance(Position(a)) <
                   Position(myStart).getDistance(Position(b));
        });
    Broodwar->printf("[Scouting] %d start targets.",
                    static_cast<int>(startTargets.size()));
}

void ScoutingManager::issueMoveToward(const BWAPI::Position& p, int reissueDist, bool force) {
    if (!scout || !scout->exists() || !p.isValid()) return;
    if (!force && BWAPI::Broodwar->getFrameCount() - lastMoveIssueFrame < kMoveCooldownFrames) return;

    if (force || scout->getDistance(p) > reissueDist || !scout->isMoving()) {
        scout->stop();      // cancel any attack/queue
        scout->move(p);
        lastMoveIssueFrame = BWAPI::Broodwar->getFrameCount();
    }
    BWAPI::Broodwar->drawLineMap(scout->getPosition(), p, BWAPI::Colors::Yellow);
}

bool ScoutingManager::seeAnyEnemyBuildingNear(const Position& p, int radius) const {
    for (auto& u : Broodwar->enemy()->getUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isBuilding()) continue;
        if (u->getPosition().isValid() && u->getDistance(p) <= radius) return true;
    }
    return false;
}

/* ---------- gas steal ---------- */

bool ScoutingManager::anyRefineryOn(BWAPI::Unit geyser) const {
    if (!geyser || !geyser->exists()) return false;
    const TilePosition tp = geyser->getTilePosition();
    for (auto& u : Broodwar->getAllUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isRefinery()) continue;
        if (u->getTilePosition() == tp) return true;
    }
    return false;
}

bool ScoutingManager::tryGasSteal() {
    if (gasStealDone) return false;
    if (!scout || !scout->exists() || !scout->getType().isWorker()) return false;
    if (!enemyMainPos.isValid()) return false;

    // cooldown
    const int now = Broodwar->getFrameCount();
    if (now < nextGasStealRetryFrame) return false;

    // find nearest geyser at enemy main (or reuse cached)
    if (!targetGeyser || !targetGeyser->exists()) {
        BWAPI::Unit best = nullptr; int bestDist = 1e9;
        for (auto g : Broodwar->getGeysers()) {
            if (!g || !g->exists()) continue;
            const int d = enemyMainPos.getDistance(g->getPosition());
            if (d > 400) continue; // must be "at" the main
            if (anyRefineryOn(g)) { gasStealDone = true; return false; }
            if (d < bestDist) { bestDist = d; best = g; }
        }
        targetGeyser = best;
        if (!targetGeyser) { gasStealDone = true; return false; }
    }

    // If any refinery now exists, we are finished
    if (anyRefineryOn(targetGeyser)) { gasStealDone = true; return false; }

    // Only Protoss can gas-steal in this simplified flow
    if (Broodwar->self()->getRace() != Races::Protoss) { gasStealDone = true; return false; }

    // Not enough minerals → retry soon
    if (Broodwar->self()->minerals() < UnitTypes::Protoss_Assimilator.mineralPrice()) {
        nextGasStealRetryFrame = now + kGasStealRetryCooldown;
        return false;
    }

    // Move into range, then attempt build
    if (scout->getDistance(targetGeyser) > 96 || !scout->isMoving()) {
        scout->move(targetGeyser->getPosition());
        nextGasStealRetryFrame = now + kGasStealRetryCooldown;
        return true; // consumed this frame
    }

    // Try to place the Assimilator
    const TilePosition gtp = targetGeyser->getTilePosition();
    if (scout->build(UnitTypes::Protoss_Assimilator, gtp)) {
        Broodwar->printf("[GasSteal] Assimilator started at (%d,%d).", gtp.x, gtp.y);
        gasStealDone = true;
        return true; // consumed this frame
    }

    // Placement blocked: step again & retry soon
    scout->move(targetGeyser->getPosition());
    nextGasStealRetryFrame = now + kGasStealRetryCooldown;
    return true; // consumed
}

/* ---------- harass / orbit ---------- */

bool ScoutingManager::tryHarassWorker() {
    if (!scout || !scout->exists() || !enemyMainPos.isValid()) return false;

    BWAPI::Unit best = nullptr; int bestDist = 999999;
    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists()) continue;
        if (!e->getType().isWorker()) continue;
        if (e->getDistance(enemyMainPos) > kHarassRadiusFromMain) continue;
        int d = e->getDistance(scout);
        if (d < bestDist) { bestDist = d; best = e; }
    }
    if (!best) return false;

    if (Broodwar->getFrameCount() - lastMoveIssueFrame >= kMoveCooldownFrames) {
        scout->attack(best);
        lastMoveIssueFrame = Broodwar->getFrameCount();
    }
    Broodwar->drawCircleMap(best->getPosition(), 10, Colors::Red, true);
    return true;
}

void ScoutingManager::ensureOrbitWaypoints() {
    if (!orbitWaypoints.empty()) return;

    orbitWaypoints.clear();
    orbitWaypoints.reserve(8);

    const BWAPI::Position center = clampToMapPx(computeOrbitCenter(), 64);

    const int radius = kOrbitRadius;

    for (int deg = 0; deg < 360; deg += 45) {
        BWAPI::Position raw = orbitPoint(center, radius, deg);
        raw = clampToMapPx(raw, 48);
        BWAPI::Position snapped = snapToNearestWalkable(raw, /*maxRadiusPx*/128);
        if (snapped.isValid())
            orbitWaypoints.push_back(snapped);
        // else skip this waypoint 
    }

    if (orbitWaypoints.empty()) {
        orbitWaypoints.push_back(center);
    }

    BWAPI::Position threat = getAvgPosition();
    int oppDeg = threat.isValid()
        ? angleDeg(center, threat) + 180
        : (scout && scout->exists() ? angleDeg(center, scout->getPosition()) + 180 : 0);
    oppDeg = normDeg(oppDeg);

    const int step = 45;
    int startIdx = (oppDeg + step / 2) / step;
    orbitIdx = static_cast<std::size_t>(startIdx % int(orbitWaypoints.size()));
    dwellUntilFrame = 0;
}

BWAPI::Position ScoutingManager::currentOrbitPoint() const {
    if (orbitWaypoints.empty()) return enemyMainPos;
    return orbitWaypoints[orbitIdx % orbitWaypoints.size()];
}

void ScoutingManager::advanceOrbitIfArrived() {
    const auto goal = currentOrbitPoint();
    if (!goal.isValid()) return;
    const int now = Broodwar->getFrameCount();

    if (scout->getDistance(goal) < 80) {
        if (dwellUntilFrame == 0) dwellUntilFrame = now + 12; // linger ~0.5s
        if (now >= dwellUntilFrame) {
            orbitIdx = (orbitIdx + 1) % orbitWaypoints.size();
            dwellUntilFrame = 0;
        }
    } else {
        dwellUntilFrame = 0;
    }
}

bool ScoutingManager::threatenedNow() const {
    if (!scout || !scout->exists()) return false;

    if (scout->isUnderAttack()) return true;

    // Combat units close enough to strike soon
    for (auto& e : BWAPI::Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists()) continue;
        if (e->getType().isWorker()) continue;
        if (!e->getType().canAttack()) continue;
        // rough “danger bubble”
        if (e->getDistance(scout) < 192) return true;
    }
    // Aggressive workers (mining pull)
    for (auto& e : BWAPI::Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists() || !e->getType().isWorker()) continue;
        if (e->getDistance(scout) <= 112 &&
            (e->isAttacking() || e->getOrder() == BWAPI::Orders::AttackUnit || e->getTarget() == scout))
            return true;
    }
    return false;
}

// Gets the Average position of the enemy units
BWAPI::Position ScoutingManager::getAvgPosition() {
    long long sumX = 0, sumY = 0;
    int numUnits = 0;

    const BWAPI::Unitset& group = BWAPI::Broodwar->enemy()->getUnits();
    for (auto u : group) {
        if (!u || !u->exists()) continue;
        const BWAPI::Position p = u->getPosition();
        if (!p.isValid()) continue;
        sumX += p.x;
        sumY += p.y;
        ++numUnits;
    }

    return (numUnits > 0)
        ? BWAPI::Position(int(sumX / numUnits), int(sumY / numUnits))
        : BWAPI::Positions::Invalid;
}

int ScoutingManager::angleDeg(const BWAPI::Position& from, const BWAPI::Position& to) {
    const double ang = std::atan2(double(to.y - from.y), double(to.x - from.x)) * 180.0 / 3.141592653589793;
    return normDeg(int(std::lround(ang)));
}

BWAPI::Position ScoutingManager::computeOrbitCenter() const {
    if (enemyMainPos.isValid()) return enemyMainPos;
    if (scout && scout->exists()) return scout->getPosition();
    return BWAPI::Position(0, 0); // worst-case fallback; won't be used long
}

bool ScoutingManager::planTerrainPathTo(const BWAPI::Position& goal) {
    plannedPath.clear();
    if (!scout || !scout->exists() || !goal.isValid()) return false;

    const auto& cpPath = BWEM::Map::Instance().GetPath(scout->getPosition(), goal, nullptr);

    if (cpPath.empty()) {
        // Same area or trivially reachable: go direct
        plannedPath.push_back(goal);
        return true;
    }

    // Route via chokepoint centers, then final goal
    for (const auto* cp : cpPath) {
        if (!cp) continue;
        plannedPath.push_back(BWAPI::Position(cp->Center()));
    }
    plannedPath.push_back(goal);
    return true;
}

BWAPI::Position ScoutingManager::currentPlannedWaypoint() const {
    if (plannedPath.empty()) return BWAPI::Positions::Invalid;
    return plannedPath.front();
}

BWAPI::Position ScoutingManager::clampToMapPx(const BWAPI::Position& p, int marginPx) {
    int x = std::max(marginPx, std::min(p.x, mapWpx() - 1 - marginPx));
    int y = std::max(marginPx, std::min(p.y, mapHpx() - 1 - marginPx));
    return BWAPI::Position(x, y);
}


// Returns the nearest walkable pixel position to p (center of a walk cell).
// maxRadiusPx controls how far we search (in pixels). 96–128 is usually enough.
BWAPI::Position ScoutingManager::snapToNearestWalkable(BWAPI::Position p, int maxRadiusPx) {
    using namespace BWAPI;

    p = clampToMapPx(p, 32);
    WalkPosition w0(p);
    const int rMax = std::max(1, maxRadiusPx / 8); // walk cells

    auto toPosCenter = [](const WalkPosition& w) {
        return BWAPI::Position(w.x * 8 + 4, w.y * 8 + 4);
        };

    // If already walkable, return as-is.
    if (w0.isValid() && Broodwar->isWalkable(w0))
        return toPosCenter(w0);

    int bestD2 = INT_MAX;
    WalkPosition best(-1, -1);

    // Expand ring by ring in walk space 
    for (int r = 1; r <= rMax; ++r) {
        bool foundThisRing = false;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                // Only check the ring (skip interior)
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;

                WalkPosition w(w0.x + dx, w0.y + dy);
                if (!w.isValid()) continue;
                if (!Broodwar->isWalkable(w)) continue;

                int d2 = dx * dx + dy * dy;
                if (d2 < bestD2) { bestD2 = d2; best = w; foundThisRing = true; }
            }
        }
        if (foundThisRing) break; // nearest ring found
    }

    if (best.x != -1) return toPosCenter(best);
    return BWAPI::Positions::Invalid; // none found within radius
}