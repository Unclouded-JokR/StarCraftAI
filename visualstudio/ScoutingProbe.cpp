#include "ScoutingProbe.h"
#include "ProtoBotCommander.h"
#include "ScoutingManager.h"
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

void ScoutingProbe::onStart() {
    if (!Map::Instance().Initialized()) {
        Broodwar->printf("ScoutingProbe: BWEM not initialized.");
        return;
    }
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
    state = startTargets.empty() ? State::Done : State::Search;
}

void ScoutingProbe::assign(BWAPI::Unit unit) {
    if (!unit || !unit->exists()) return;
    scout = unit;
    scout->stop();
    Broodwar->printf("[Scouting] Assigned scout: %s (id=%d)", scout->getType().c_str(), scout->getID());
    lastHP = scout->getHitPoints();
    lastShields = scout->getShields();
    if (scout->isCarryingMinerals() || scout->isCarryingGas())
        state = State::ReturningCargo;
    else
        state = startTargets.empty() ? State::Done : State::Search;
}

void ScoutingProbe::onUnitDestroy(BWAPI::Unit unit) {
    if (unit && scout && unit == scout) {
        Broodwar->printf("[Scouting] Scout destroyed. Clearing assignment.");
        scout = nullptr;
        state = State::Done;
    }
}

void ScoutingProbe::onFrame() {
    if (!scout || !scout->exists() || state == State::Done) return;

    const int now = Broodwar->getFrameCount();
    const int hp = scout->getHitPoints();
    const int sh = scout->getShields();
    const bool tookDamage = (lastHP >= 0 && lastShields >= 0) && (hp + sh) < (lastHP + lastShields);
    const bool imminent = scout->isUnderAttack() || threatenedNow();

    if ((tookDamage || imminent) && (now - lastThreatFrame >= kThreatRearmFrames)) {
        lastThreatFrame = now;
        ensureOrbitWaypoints();

        if (!hasPlannedPath() || scout->getDistance(currentPlannedWaypoint()) < 80) {
            if (hasPlannedPath()) plannedPath.erase(plannedPath.begin());
            if (!hasPlannedPath()) planTerrainPathTo(currentOrbitPoint());
        }
        const auto wp = currentPlannedWaypoint();
        if (wp.isValid()) issueMoveToward(wp, /*reissue*/48);
        state = State::Orbit;
        lastHP = hp; lastShields = sh;
        return;
    }

    switch (state) {
    case State::ReturningCargo:
        handleReturningCargoState();
        break;

    case State::Search: {
        if (nextTarget >= (int)startTargets.size()) { state = State::Done; break; }
        const auto tp = startTargets[nextTarget];
        const Position goal(tp);
        issueMoveToward(goal);

        if (scout->getDistance(goal) <= kCloseEnoughToTarget) {
            if (seeAnyEnemyBuildingNear(goal, 400)) {
                enemyMainTile = tp;
                enemyMainPos = goal;
                Broodwar->printf("[Scouting] Enemy main at (%d,%d)", tp.x, tp.y);
                state = State::GasSteal;
                break;
            }
            ++nextTarget;
        }
        break;
    }

    case State::GasSteal:
        if (tryGasSteal()) break;
        if (gasStealDone) state = State::Harass;
        break;

    case State::Harass:
        if (enemyMainPos.isValid() && tryHarassWorker()) break;
        state = State::Orbit;
        break;

    case State::Orbit:
        // If still threatened, keep clocking "lastThreatFrame" so the calm timer resets
        if (threatenedNow()) {
            lastThreatFrame = now;
        }

        // If we've been calm long enough and know their main, resume harassment
        if (enemyMainPos.isValid() && (now - lastThreatFrame) >= kCalmFramesToResumeHarass) {
            plannedPath.clear();        // drop orbit path so we don't keep following it
            state = State::Harass;
            break;
        }

        ensureOrbitWaypoints();
        issueMoveToward(currentOrbitPoint(), /*reissueDist*/64, false);
        advanceOrbitIfArrived();
        break;

    case State::Done:
    default:
        break;
    }
}

void ScoutingProbe::setEnemyMain(const TilePosition& tp) {
    enemyMainTile = tp;
    enemyMainPos = Position(tp);
    state = State::GasSteal;
}

/* ---------- returning cargo ---------- */
void ScoutingProbe::handleReturningCargoState() {
    if (!scout) return;
    if (!scout->isCarryingMinerals() && !scout->isCarryingGas()) {
        state = startTargets.empty() ? State::Done : State::Search;
        return;
    }
    if (scout->getOrder() != Orders::ReturnMinerals && scout->getOrder() != Orders::ReturnGas) {
        if (!scout->returnCargo()) {
            if (BWAPI::Unit depot = findClosestDepot(scout->getPosition())) {
                scout->rightClick(depot);
            }
        }
    }
    Broodwar->drawTextMap(scout->getPosition(), "Returning cargo...");
}

BWAPI::Unit ScoutingProbe::findClosestDepot(const BWAPI::Position& from) const {
    BWAPI::Unit best = nullptr; int bestDist = INT_MAX;
    for (auto& u : BWAPI::Broodwar->self()->getUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isResourceDepot()) continue;
        int d = u->getDistance(from);
        if (d < bestDist) { bestDist = d; best = u; }
    }
    return best;
}

/* ---------- helpers (your originals) ---------- */
void ScoutingProbe::buildStartTargets() {
    startTargets.clear();
    const TilePosition myStart = Broodwar->self()->getStartLocation();
    for (const auto& area : Map::Instance().Areas()) {
        for (const auto& base : area.Bases()) {
            if (base.Starting() && base.Location() != myStart) {
                startTargets.push_back(base.Location());
            }
        }
    }
    std::sort(startTargets.begin(), startTargets.end(),
        [&](const TilePosition& a, const TilePosition& b) {
            return Position(myStart).getDistance(Position(a)) <
                Position(myStart).getDistance(Position(b));
        });
    Broodwar->printf("[Scouting] %d start targets.", static_cast<int>(startTargets.size()));
}

void ScoutingProbe::issueMoveToward(const Position& p, int reissueDist, bool force) {
    if (!scout || !scout->exists() || !p.isValid()) return;
    if (!force && Broodwar->getFrameCount() - lastMoveIssueFrame < kMoveCooldownFrames) return;
    if (force || scout->getDistance(p) > reissueDist || !scout->isMoving()) {
        scout->stop(); scout->move(p);
        lastMoveIssueFrame = Broodwar->getFrameCount();
    }
    Broodwar->drawLineMap(scout->getPosition(), p, Colors::Yellow);
}

bool ScoutingProbe::seeAnyEnemyBuildingNear(const Position& p, int radius) const {
    for (auto& u : Broodwar->enemy()->getUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isBuilding()) continue;
        if (u->getPosition().isValid() && u->getDistance(p) <= radius) return true;
    }
    return false;
}

bool ScoutingProbe::anyRefineryOn(BWAPI::Unit geyser) const {
    if (!geyser || !geyser->exists()) return false;
    const TilePosition tp = geyser->getTilePosition();
    for (auto& u : Broodwar->getAllUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isRefinery()) continue;
        if (u->getTilePosition() == tp) return true;
    }
    return false;
}

bool ScoutingProbe::tryGasSteal() {
    if (gasStealDone) return false;
    if (!scout || !scout->exists() || !scout->getType().isWorker()) return false;
    if (!enemyMainPos.isValid()) return false;

    const int now = Broodwar->getFrameCount();
    if (now < nextGasStealRetryFrame) return false;

    if (!targetGeyser || !targetGeyser->exists()) {
        BWAPI::Unit best = nullptr; int bestDist = 1e9;
        for (auto g : Broodwar->getGeysers()) {
            if (!g || !g->exists()) continue;
            const int d = enemyMainPos.getDistance(g->getPosition());
            if (d > 400) continue;
            if (anyRefineryOn(g)) { gasStealDone = true; return false; }
            if (d < bestDist) { bestDist = d; best = g; }
        }
        targetGeyser = best;
        if (!targetGeyser) { gasStealDone = true; return false; }
    }
    if (anyRefineryOn(targetGeyser)) { gasStealDone = true; return false; }
    if (Broodwar->self()->getRace() != Races::Protoss) { gasStealDone = true; return false; }
    if (Broodwar->self()->minerals() < UnitTypes::Protoss_Assimilator.mineralPrice()) {
        nextGasStealRetryFrame = now + kGasStealRetryCooldown;
        return false;
    }
    if (scout->getDistance(targetGeyser) > 96 || !scout->isMoving()) {
        scout->move(targetGeyser->getPosition());
        nextGasStealRetryFrame = now + kGasStealRetryCooldown;
        return true;
    }
    const TilePosition gtp = targetGeyser->getTilePosition();
    if (scout->build(UnitTypes::Protoss_Assimilator, gtp)) {
        Broodwar->printf("[GasSteal] Assimilator started at (%d,%d).", gtp.x, gtp.y);
        gasStealDone = true;
        return true;
    }
    scout->move(targetGeyser->getPosition());
    nextGasStealRetryFrame = now + kGasStealRetryCooldown;
    return true;
}

bool ScoutingProbe::tryHarassWorker() {
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

void ScoutingProbe::ensureOrbitWaypoints() {
    if (!orbitWaypoints.empty()) return;
    orbitWaypoints.clear();
    orbitWaypoints.reserve(8);
    BWAPI::Broodwar->printf("Orbit");
    const Position center = clampToMapPx(computeOrbitCenter(), 64);
    const int radius = kOrbitRadius;

    for (int deg = 0; deg < 360; deg += 45) {
        Position raw = orbitPoint(center, radius, deg);
        raw = clampToMapPx(raw, 48);
        Position snapped = snapToNearestWalkable(raw, 128);
        if (snapped.isValid()) orbitWaypoints.push_back(snapped);
    }
    if (orbitWaypoints.empty()) orbitWaypoints.push_back(center);

    Position threat = getAvgPosition();
    int oppDeg = threat.isValid()
        ? angleDeg(center, threat) + 180
        : (scout && scout->exists() ? angleDeg(center, scout->getPosition()) + 180 : 0);
    oppDeg = normDeg(oppDeg);
    const int step = 45;
    int startIdx = (oppDeg + step / 2) / step;
    orbitIdx = static_cast<std::size_t>(startIdx % int(orbitWaypoints.size()));
    dwellUntilFrame = 0;
}

BWAPI::Position ScoutingProbe::currentOrbitPoint() const {
    if (orbitWaypoints.empty()) return enemyMainPos;
    return orbitWaypoints[orbitIdx % orbitWaypoints.size()];
}

void ScoutingProbe::advanceOrbitIfArrived() {
    const auto goal = currentOrbitPoint();
    if (!goal.isValid()) return;
    const int now = Broodwar->getFrameCount();

    if (scout->getDistance(goal) < 80) {
        if (dwellUntilFrame == 0) dwellUntilFrame = now + 12;
        if (now >= dwellUntilFrame) {
            orbitIdx = (orbitIdx + 1) % orbitWaypoints.size();
            dwellUntilFrame = 0;
        }
    }
    else {
        dwellUntilFrame = 0;
    }
}

bool ScoutingProbe::threatenedNow() const {
    if (!scout || !scout->exists()) return false;
    if (scout->isUnderAttack()) return true;

    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists()) continue;
        if (e->getType().isWorker()) continue;
        if (!e->getType().canAttack()) continue;
        if (e->getDistance(scout) < 192) return true;
    }
    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists() || !e->getType().isWorker()) continue;
        if (e->getDistance(scout) <= 112 &&
            (e->isAttacking() || e->getOrder() == Orders::AttackUnit || e->getTarget() == scout))
            return true;
    }
    return false;
}

BWAPI::Position ScoutingProbe::getAvgPosition() {
    long long sumX = 0, sumY = 0; int numUnits = 0;
    const Unitset& group = Broodwar->enemy()->getUnits();
    for (auto u : group) {
        if (!u || !u->exists()) continue;
        const Position p = u->getPosition();
        if (!p.isValid()) continue;
        sumX += p.x; sumY += p.y; ++numUnits;
    }
    return (numUnits > 0) ? Position(int(sumX / numUnits), int(sumY / numUnits)) : Positions::Invalid;
}

int ScoutingProbe::angleDeg(const Position& from, const Position& to) {
    const double ang = std::atan2(double(to.y - from.y), double(to.x - from.x)) * 180.0 / 3.141592653589793;
    int d = int(std::lround(ang)); d %= 360; if (d < 0) d += 360; return d;
}

BWAPI::Position ScoutingProbe::computeOrbitCenter() const {
    if (enemyMainPos.isValid()) 
    {
        if (manager) manager->setEnemyMain(BWAPI::TilePosition(enemyMainPos));
        return enemyMainPos;
    } 
    if (scout && scout->exists()) return scout->getPosition();
    return Position(0, 0);
}

bool ScoutingProbe::planTerrainPathTo(const Position& goal) {
    plannedPath.clear();
    if (!scout || !scout->exists() || !goal.isValid()) return false;
    const auto& cpPath = BWEM::Map::Instance().GetPath(scout->getPosition(), goal, nullptr);
    if (cpPath.empty()) { plannedPath.push_back(goal); return true; }
    for (const auto* cp : cpPath) {
        if (!cp) continue;
        plannedPath.push_back(Position(cp->Center()));
    }
    plannedPath.push_back(goal);
    return true;
}

BWAPI::Position ScoutingProbe::currentPlannedWaypoint() const {
    if (plannedPath.empty()) return Positions::Invalid;
    return plannedPath.front();
}

BWAPI::Position ScoutingProbe::clampToMapPx(const Position& p, int marginPx) {
    int x = std::max(marginPx, std::min(p.x, mapWpx() - 1 - marginPx));
    int y = std::max(marginPx, std::min(p.y, mapHpx() - 1 - marginPx));
    return Position(x, y);
}

BWAPI::Position ScoutingProbe::snapToNearestWalkable(Position p, int maxRadiusPx) {
    p = clampToMapPx(p, 32);
    WalkPosition w0(p);
    const int rMax = std::max(1, maxRadiusPx / 8);
    auto toPosCenter = [](const WalkPosition& w) { return Position(w.x * 8 + 4, w.y * 8 + 4); };
    if (w0.isValid() && Broodwar->isWalkable(w0)) return toPosCenter(w0);

    int bestD2 = INT_MAX; WalkPosition best(-1, -1);
    for (int r = 1; r <= rMax; ++r) {
        bool foundThisRing = false;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                WalkPosition w(w0.x + dx, w0.y + dy);
                if (!w.isValid()) continue;
                if (!Broodwar->isWalkable(w)) continue;
                int d2 = dx * dx + dy * dy;
                if (d2 < bestD2) { bestD2 = d2; best = w; foundThisRing = true; }
            }
        }
        if (foundThisRing) break;
    }
    if (best.x != -1) return toPosCenter(best);
    return Positions::Invalid;
}
