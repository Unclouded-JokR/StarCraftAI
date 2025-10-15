#include "ScoutingManager.h"
#include <BWAPI.h>
#include <bwem.h>
#include <algorithm>
#include <cassert>

using namespace BWAPI;
using namespace BWEM;

void ScoutingManager::onStart() {
    if (!Map::Instance().Initialized()) {
        Broodwar->printf("ScoutingManager: BWEM not initialized yet.");
        return;
    }

    targets.clear();
    TilePosition myStart = Broodwar->self()->getStartLocation();

    for (const auto& area : Map::Instance().Areas()) {
        for (const auto& base : area.Bases()) {
            if (base.Starting() && base.Location() != myStart) {
                targets.push_back(base.Location());
            }
        }
    }
    Broodwar->printf("ScoutingManager: %u start targets.", (unsigned)targets.size());

    // Sort by ground-path length (fallback to air distance)
    std::sort(targets.begin(), targets.end(),
        [&](const TilePosition& a, const TilePosition& b) {
            const auto pa = Map::Instance().GetPath(Position(myStart), Position(a));
            const auto pb = Map::Instance().GetPath(Position(myStart), Position(b));
            if (pa.empty() != pb.empty()) return !pa.empty(); // prefer a path over no path
            if (!pa.empty() && !pb.empty()) return pa.size() < pb.size();
            // both empty: air distance fallback
            return Position(myStart).getDistance(Position(a)) <
                    Position(myStart).getDistance(Position(b));
        });

    nextTarget = 0;
    scout = nullptr;
    onFrame();   // Try to assign a scout immediately for testing

}

void ScoutingManager::onFrame() {
    // Assign a worker if we don't have one (prefer idle/guarding probe)
    if (!scout || !scout->exists()) {
        // Best: idle or PlayerGuard (standing around)
        for (auto& u : Broodwar->self()->getUnits()) {
            if (u->getType().isWorker() && u->isCompleted() &&
                (u->getOrder() == Orders::PlayerGuard || u->isIdle())) {
                scout = u;
                Broodwar->printf("Scout assigned (idle): %s", scout->getType().c_str());
                break;
            }
        }
        // Fallback: any worker not carrying (will pull from mining)
        if (!scout) {
            for (auto& u : Broodwar->self()->getUnits()) {
                if (u->getType().isWorker() && u->isCompleted() &&
                    !u->isCarryingMinerals() && !u->isCarryingGas()) {
                    scout = u;
                    Broodwar->printf("Scout assigned (pulled): %s", scout->getType().c_str());
                    break;
                }
            }
        }
    }

    if (!scout || !scout->exists() || targets.empty()) return;
    if (nextTarget >= (int)targets.size()) return;

    const TilePosition tp = targets[nextTarget];
    const Position     goal(tp);

    // IMPORTANT: Don’t require isIdle() — issue the move regardless so we pull it off mining.
    if (goal.isValid()) {
        Broodwar->drawLineMap(scout->getPosition(), goal, Colors::Yellow);
        // Only re-issue if far to avoid spamming
        if (scout->getDistance(goal) > 96) {
            scout->move(goal);
        }
    }

    // Advance when close
    if (scout->getDistance(goal) < 200) {
        Broodwar->printf("Scout reached target %d (%d,%d).", nextTarget, tp.x, tp.y);
        nextTarget++;
    }

    // Simple retreat rule
    if (scout->isUnderAttack()) {
        scout->move(Position(Broodwar->self()->getStartLocation()));
    }
}
