#pragma once
#include <memory>
#include <variant>
#include <optional>
#include <BWAPI.h>
#include <bwem.h>
#include <vector>
#include <algorithm>
class ProtoBotCommander;

// include concrete behaviors
#include "ScoutingProbe.h"
#include "ScoutingZealot.h"
// #include "ScoutingDragoon.h"
// #include "ScoutingObserver.h"

class ScoutingManager {
public:
    explicit ScoutingManager(ProtoBotCommander* commander = nullptr);

    void onStart();
    void onFrame();

    void assignScout(BWAPI::Unit unit);              // decides which concrete behavior to use
    bool hasScout() const;

    void setEnemyMain(const BWAPI::TilePosition& tp);
    void setEnemyNatural(const BWAPI::TilePosition& tp);
    void onUnitDestroy(BWAPI::Unit unit);

    std::optional<BWAPI::TilePosition> getEnemyMain() const { return enemyMainCache_; }
    std::optional<BWAPI::TilePosition> getEnemyNatural() const { return enemyNaturalCache_; }

    void markScout(BWAPI::Unit u);
    void markScout(BWAPI::Unit u, bool isCombatScout);          // call when a unit becomes a scout
    void unmarkScout(BWAPI::Unit u);        // call when it stops being a scout
    bool isScout(BWAPI::Unit u) const;      // handy for other modules (e.g., Squad draw)
    bool isCombatScout(BWAPI::Unit u) const;
    bool hasWorkerScout() const;
    int  numCombatScouts() const;
    void drawScoutTags() const;             // draw labels for all scouts

    void setCombatScoutingStarted(bool v) { combatScoutingStarted_ = v; }
    bool combatScoutingStarted() const { return combatScoutingStarted_; }
    void setMaxCombatScouts(int n) { maxCombatScouts_ = n; }
    int  maxCombatScouts() const { return maxCombatScouts_; }

    bool canAcceptWorkerScout() const { return !combatScoutingStarted_ && !workerScout_; }
    bool canAcceptCombatScout() const { return numCombatScouts() < maxCombatScouts_; }

private:
    ProtoBotCommander* commanderRef = nullptr;


    using BehaviorVariant = std::variant<
        std::monostate, 
        ScoutingProbe,
        ScoutingZealot
        /*Once added ->, 
        ScoutingDragoon, 
        ScoutingObserver*/
        >;
    BehaviorVariant behavior;

    // helpers
    void constructBehaviorFor(BWAPI::Unit unit);

    std::vector<BWAPI::Unit> scouts_;
    BWAPI::Unit workerScout_{ nullptr };
    std::vector<BWAPI::Unit> combatScouts_;
    bool combatScoutingStarted_{ false };
    int  maxCombatScouts_{ 3 }; // default, adjustable
    std::optional<BWAPI::TilePosition> enemyMainCache_;
    std::optional<BWAPI::TilePosition> enemyNaturalCache_;
};
