#include "ScoutingManager.h"
#include "ProtoBotCommander.h"
#include <BWAPI.h>
#include <bwem.h>
#include <algorithm>
#include <cassert>
#include <cmath>

using namespace BWAPI;
using namespace BWEM;

ScoutingManager::ScoutingManager(ProtoBotCommander* commander)
    : commanderRef(commander) {
}

void ScoutingManager::onStart() {
    if (!Map::Instance().Initialized()) {
        Broodwar->printf("ScoutingManager: BWEM not initialized yet.");
        return;
    }

    startTargets.clear();
    buildStartTargets();
    nextTarget = 0;
    enemyMainTile.reset();
    enemyMainPos = Positions::Invalid;
    state = startTargets.empty() ? State::Done : State::Search;
    gasStealDone = false;
    targetGeyser = nullptr;
    nextGasStealRetryFrame = 0;


    scout = nullptr;
    assignScoutIfNeeded(); // try immediately
}

void ScoutingManager::onFrame() {
    assignScoutIfNeeded();
    if (!scout || !scout->exists()) return;

    // If we already know enemy main, go to that workflow
    if (enemyMainTile.has_value() && state != State::Done) {
        enemyMainPos = BWAPI::Position(*enemyMainTile);
        state = State::InEnemyBase;
    }

    switch (state)
    {
    case State::Search: {
        if (nextTarget >= (int)startTargets.size()) { state = State::ReturnHome; break; }
        state = State::TravelToTarget;
        break;
    }

    case State::TravelToTarget: {
        if (nextTarget >= (int)startTargets.size()) { state = State::ReturnHome; break; }

        const BWAPI::TilePosition tp = startTargets[nextTarget];
        const BWAPI::Position goal(tp);
        if (goal.isValid()) {
            BWAPI::Broodwar->drawLineMap(scout->getPosition(), goal, BWAPI::Colors::Yellow);
            issueMoveToward(goal);
        }

        // arrival check
        if (scout->getDistance(goal) < closeEnoughToTarget) {
            if (seeAnyEnemyBuildingNear(goal, 400)) {
                enemyMainTile = tp;
                enemyMainPos = goal;
                BWAPI::Broodwar->printf("Scouting: Enemy main located at %d,%d", tp.x, tp.y);
                state = State::InEnemyBase;
                break;
            }
            ++nextTarget;
            pickNextTarget();
        }

        if (threatenedByCombat()) { state = State::Retreat; break; }
        if (chasedByWorkers()) { state = State::Orbit;   break; }
        break;
    }

    case State::InEnemyBase: {
        if (!enemyMainPos.isValid()) { /* ... */ }

        // Retreat / Orbit decisions FIRST
        if (scout->getShields() <= lowHPShield || threatenedNow()) {
            const auto home = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
            // escape immediately (bypass cooldown; cancel attack)
            escapeMove(home);                   // or: issueMoveToward(home, 0, /*force=*/true);
            state = State::ReturnHome;
            break;
        }

        // gas steal (consumes the frame if it acts)
        if (enableGasSteal && BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss) {
            if (tryGasSteal()) { break; }
        }

        if (chasedByWorkers()) {
            // optional: orbit instead of hard retreat
            ensureOrbitWaypoints();
            const auto goal = currentOrbitPoint();
            escapeMove(goal);                   // snap to evasive path now
            state = State::Orbit;
            break;
        }

        

        // harass or orbit
        if (!tryHarassWorker()) {
            ensureOrbitWaypoints();
            const auto goal = currentOrbitPoint();
            issueMoveToward(goal, /*reissueDist*/64); // normal gated move is fine here
            advanceOrbitIfArrived();
        }
        break;
    }

    case State::Orbit: {
        if (scout->getShields() <= lowHPShield || threatenedNow()) {
            const auto home = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
            escapeMove(home);
            state = State::ReturnHome;
            break;
        }

        // same early retreat checks here
        if (scout->getShields() <= lowHPShield || threatenedNow()) {
            state = State::ReturnHome;
            break;
        }
        if (!chasedByWorkers()) {
            state = State::InEnemyBase;
            break;
        }

        ensureOrbitWaypoints();
        const auto goal = currentOrbitPoint();
        issueMoveToward(goal, /*reissueDist*/ 64);
        advanceOrbitIfArrived();
        break;
    }

    case State::Retreat:
    case State::ReturnHome: {
        const auto home = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
        // keep forcing the move so attacks never re-grab the order
        escapeMove(home);
        if (scout->getDistance(home) < 128) { state = State::Done; }
        break;
    }

    case State::Done:
    default: {
        // release worker to mine
        if (scout && scout->exists() && scout->getType().isWorker() && scout->isIdle()) {
            BWAPI::Unit mineral = nullptr;
            int best = 1e9;
            for (auto m : BWAPI::Broodwar->getMinerals()) {
                int d = scout->getDistance(m);
                if (d < best) { best = d; mineral = m; }
            }
            if (mineral) scout->rightClick(mineral);
        }
        break;
    }
    }
}

void ScoutingManager::setEnemyMain(const TilePosition& tp) {
    enemyMainTile = tp;
    enemyMainPos = Position(tp);
    state = State::InEnemyBase;
}

// ----------------- helpers -----------------

void ScoutingManager::assignScoutIfNeeded() {
    if (scout && scout->exists()) return;

    // Prefer idle/PlayerGuard worker
    for (auto& u : Broodwar->self()->getUnits()) {
        if (u->getType().isWorker() && u->isCompleted() &&
            (u->getOrder() == Orders::PlayerGuard || u->isIdle())) {
            scout = u;
            Broodwar->printf("Scout assigned (idle): %s", scout->getType().c_str());
            return;
        }
    }
    // Fallback: any worker not carrying
    for (auto& u : Broodwar->self()->getUnits()) {
        if (u->getType().isWorker() && u->isCompleted() &&
            !u->isCarryingMinerals() && !u->isCarryingGas()) {
            scout = u;
            Broodwar->printf("Scout assigned (pulled): %s", scout->getType().c_str());
            return;
        }
    }
}

void ScoutingManager::buildStartTargets() {
    TilePosition myStart = Broodwar->self()->getStartLocation();
    for (const auto& area : Map::Instance().Areas()) {
        for (const auto& base : area.Bases()) {
            if (base.Starting() && base.Location() != myStart) {
                startTargets.push_back(base.Location());
            }
        }
    }
    // Sort by ground path length, fallback to air distance
    std::sort(startTargets.begin(), startTargets.end(),
        [&](const TilePosition& a, const TilePosition& b) {
            const auto pa = Map::Instance().GetPath(Position(myStart), Position(a));
            const auto pb = Map::Instance().GetPath(Position(myStart), Position(b));
            if (pa.empty() != pb.empty()) return !pa.empty();
            if (!pa.empty() && !pb.empty()) return pa.size() < pb.size();
            return Position(myStart).getDistance(Position(a)) <
                Position(myStart).getDistance(Position(b));
        });

    Broodwar->printf("Scouting: %u start targets.", (unsigned)startTargets.size());
}

void ScoutingManager::pickNextTarget() {
    if (nextTarget < (int)startTargets.size()) {
        const auto tp = startTargets[nextTarget];
        Broodwar->printf("Scouting: heading to start pos %d at (%d,%d)", nextTarget, tp.x, tp.y);
    }
}

void ScoutingManager::issueMoveToward(const BWAPI::Position& p, int reissueDist) {
    if (!p.isValid() || !scout || !scout->exists()) return;

    // cool-down gate
    if (BWAPI::Broodwar->getFrameCount() - lastMoveIssueFrame < moveCooldownFrames) {
        return;
    }

    if (scout->getDistance(p) > reissueDist || !scout->isMoving()) {
        scout->move(p);
        lastMoveIssueFrame = BWAPI::Broodwar->getFrameCount();
    }

    BWAPI::Broodwar->drawLineMap(scout->getPosition(), p, BWAPI::Colors::Yellow);
}

bool ScoutingManager::tryHarassWorker() {
    if (!scout || !scout->exists()) return false;

    // Bail if risky
    if (scout->getShields() <= lowHPShield) return false;
    if (threatenedNow()) return false;     

    BWAPI::Unit best = nullptr; int bestDist = 999999;
    for (auto& e : BWAPI::Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists() || !e->getType().isWorker()) continue;
        if (enemyMainPos.isValid() && e->getDistance(enemyMainPos) > 320) continue;
        int d = e->getDistance(scout);
        if (d < bestDist) { bestDist = d; best = e; }
    }
    if (!best) return false;

    if (bestDist > 96) return false;

    if (BWAPI::Broodwar->getFrameCount() - lastMoveIssueFrame >= moveCooldownFrames) {
        scout->attack(best);
        lastMoveIssueFrame = BWAPI::Broodwar->getFrameCount();
    }
    BWAPI::Broodwar->drawCircleMap(best->getPosition(), 10, BWAPI::Colors::Red, true);
    return true;
}


bool ScoutingManager::threatenedNow() const {
    if (!scout || !scout->exists()) return false;

    // If under attack, always count as threatened
    if (scout->isUnderAttack()) return true;

    // Non-worker combat nearby?
    if (threatenedByCombat()) return true;

    // Workers actively attacking us nearby
    for (auto& e : BWAPI::Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists()) continue;
        if (!e->getType().isWorker()) continue;
        if (e->getDistance(scout) > 112) continue;  // ~range + a bit
        if (e->isAttacking() || e->getOrder() == BWAPI::Orders::AttackUnit) {
            return true;
        }
    }
    return false;
}

void ScoutingManager::escapeMove(const BWAPI::Position& p) {
    if (!scout || !scout->exists() || !p.isValid()) return;
    scout->stop();                    // cancel any current attack/queued cmd
    scout->move(p);                   // issue the flee move immediately
    lastMoveIssueFrame = BWAPI::Broodwar->getFrameCount(); // update so we don't spam
}

void ScoutingManager::ensureOrbitWaypoints() {
    if (!enemyMainPos.isValid() || !orbitWaypoints.empty()) return;
    orbitWaypoints.reserve(8);
    for (int a = 0; a < 360; a += 45) {
        orbitWaypoints.push_back(orbitPoint(enemyMainPos, orbitRadius, a));
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
    const int now = BWAPI::Broodwar->getFrameCount();

    // only advance when we actually reached it and lingered a bit
    if (scout->getDistance(goal) < 80) {
        if (dwellUntilFrame == 0) dwellUntilFrame = now + dwellFrames;
        if (now >= dwellUntilFrame) {
            orbitIdx = (orbitIdx + 1) % (int)orbitWaypoints.size();
            dwellUntilFrame = 0;
        }
    }
    else {
        // not there yet — keep moving to the same goal
        dwellUntilFrame = 0;
    }
}


bool ScoutingManager::seeAnyEnemyBuildingNear(const Position& p, int radius) const {
    for (auto& u : Broodwar->enemy()->getUnits()) {
        if (!u->exists()) continue;
        if (!u->getType().isBuilding()) continue;
        if (u->getPosition().isValid() && u->getDistance(p) <= radius) return true;
    }
    return false;
}

// ----------------- gas steal -----------------

bool ScoutingManager::tryGasSteal() {
    if (gasStealDone) return false;
    if (!enemyMainPos.isValid()) return false;
    if (!scout || !scout->exists() || !scout->getType().isWorker()) return false;

    // cooldown gate
    if (BWAPI::Broodwar->getFrameCount() < nextGasStealRetryFrame) return false;

    // (re)select target geyser
    if (!targetGeyser || !targetGeyser->exists()) {
        BWAPI::Unit best = nullptr; int bestDist = 1e9;
        for (auto g : BWAPI::Broodwar->getGeysers()) {
            if (!g || !g->exists()) continue;
            const int d = enemyMainPos.getDistance(g->getPosition());
            if (d > 400) continue;
            Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Assimilator);
            static int nextActionFrame = 0;
            int now = BWAPI::Broodwar->getFrameCount();
            if (now >= nextActionFrame) {
                nextActionFrame = now + 48;
            }
            if (anyRefineryOn(g)) { gasStealDone = true; return false; }
            if (d < bestDist) { bestDist = d; best = g; }
        }
        targetGeyser = best;
        if (!targetGeyser) { gasStealDone = true; return false; }
    }

    if (anyRefineryOn(targetGeyser)) { gasStealDone = true; return false; }

    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Protoss) { gasStealDone = true; return false; }

    if (BWAPI::Broodwar->self()->minerals() < BWAPI::UnitTypes::Protoss_Assimilator.mineralPrice()) {
        nextGasStealRetryFrame = BWAPI::Broodwar->getFrameCount() + gasStealRetryCooldown;
        return false;
    }

    const auto gtp = targetGeyser->getTilePosition();

    // Move closer if not in range; CONSUME this frame so no other orders overwrite it
    if (scout->getDistance(targetGeyser) > 96 || !scout->isMoving()) {
        scout->move(targetGeyser->getPosition());
        nextGasStealRetryFrame = BWAPI::Broodwar->getFrameCount() + gasStealRetryCooldown;
        return true; // <-- consumed
    }

    // Try to build
    if (scout->build(BWAPI::UnitTypes::Protoss_Assimilator, gtp)) {
        BWAPI::Broodwar->printf("[GasSteal] Starting Assimilator at (%d,%d).", gtp.x, gtp.y);
        gasStealDone = true;
        return true; // consumed
    }
    else {
        // Could be blocked; step closer and retry soon
        scout->move(targetGeyser->getPosition());
        nextGasStealRetryFrame = BWAPI::Broodwar->getFrameCount() + gasStealRetryCooldown;
        return true; // consumed
    }
}

bool ScoutingManager::anyRefineryOn(const BWAPI::Unit geyser) const {
    if (!geyser || !geyser->exists()) return false;
    const BWAPI::TilePosition tp = geyser->getTilePosition();
    for (auto& u : BWAPI::Broodwar->getAllUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isRefinery()) continue;
        if (u->getTilePosition() == tp) return true;
    }
    return false;
}



// ----------------- micro -----------------

BWAPI::Position ScoutingManager::orbitPoint(const Position& center, int radius, int angleDeg) {
    const double rad = angleDeg * 3.1415926535 / 180.0;
    int ox = center.x + int(radius * std::cos(rad));
    int oy = center.y + int(radius * std::sin(rad));
    return Position(ox, oy);
}

bool ScoutingManager::chasedByWorkers() const {
    if (!scout || !scout->exists()) return false;
    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e->exists()) continue;
        if (!e->getType().isWorker()) continue;
        if (e->getDistance(scout) < 96) return true;
    }
    return false;
}

bool ScoutingManager::threatenedByCombat() const {
    if (!scout || !scout->exists()) return false;
    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e->exists()) continue;
        if (e->getType().isWorker()) continue;
        if (e->getType().canAttack() && e->getDistance(scout) < 224) return true;
    }
    return false;
}