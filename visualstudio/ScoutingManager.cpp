#include "ScoutingManager.h"
#include "ProtoBotCommander.h"
#include <algorithm>
#include <cmath>

using namespace BWAPI;
using namespace BWEM;

namespace {
    inline double deg2rad(double d) { return d * 3.1415926535 / 180.0; }
    inline Position orbitPoint(const Position& c, int r, int aDeg) {
        return Position(c.x + int(r * std::cos(deg2rad(aDeg))),
                        c.y + int(r * std::sin(deg2rad(aDeg))));
    }
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

    // If attacked at any point, immediately go to Orbit
    if (threatenedNow()) {
        state = State::Orbit;
    }

    switch (state) {
    case State::Search: {
        // go through start locations until we see a building
        if (nextTarget >= (int)startTargets.size()) { state = State::Done; break; }

        const TilePosition tp = startTargets[nextTarget];
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
        issueMoveToward(currentOrbitPoint(), /*reissueDist*/64);
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
    Broodwar->printf("[Scouting] %u start targets.", unsigned(startTargets.size()));
}

void ScoutingManager::issueMoveToward(const Position& p, int reissueDist) {
    if (!scout || !scout->exists() || !p.isValid()) return;
    if (Broodwar->getFrameCount() - lastMoveIssueFrame < kMoveCooldownFrames) return;

    if (scout->getDistance(p) > reissueDist || !scout->isMoving()) {
        scout->move(p);
        lastMoveIssueFrame = Broodwar->getFrameCount();
    }
    Broodwar->drawLineMap(scout->getPosition(), p, Colors::Yellow);
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
    if (!enemyMainPos.isValid() || !orbitWaypoints.empty()) return;
    orbitWaypoints.reserve(8);
    for (int a = 0; a < 360; a += 45) {
        orbitWaypoints.push_back(orbitPoint(enemyMainPos, kOrbitRadius, a));
    }
    orbitIdx = 0;
    dwellUntilFrame = 0;
}

BWAPI::Position ScoutingManager::currentOrbitPoint() const {
    if (orbitWaypoints.empty()) return enemyMainPos;
    return orbitWaypoints[orbitIdx % (int)orbitWaypoints.size()];
}

void ScoutingManager::advanceOrbitIfArrived() {
    const auto goal = currentOrbitPoint();
    if (!goal.isValid()) return;
    const int now = Broodwar->getFrameCount();

    if (scout->getDistance(goal) < 80) {
        if (dwellUntilFrame == 0) dwellUntilFrame = now + 12; // linger ~0.5s
        if (now >= dwellUntilFrame) {
            orbitIdx = (orbitIdx + 1) % (int)orbitWaypoints.size();
            dwellUntilFrame = 0;
        }
    } else {
        dwellUntilFrame = 0;
    }
}

bool ScoutingManager::threatenedNow() const {
    if (!scout || !scout->exists()) return false;
    if (scout->isUnderAttack()) return true;

    // nearby non-worker combat unit
    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists()) continue;
        if (!e->getType().isWorker() && e->getType().canAttack() &&
            e->getDistance(scout) < 224) return true;
    }
    // aggressive workers in melee
    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists() || !e->getType().isWorker()) continue;
        if (e->getDistance(scout) <= 112 &&
           (e->isAttacking() || e->getOrder() == Orders::AttackUnit)) return true;
    }
    return false;
}
