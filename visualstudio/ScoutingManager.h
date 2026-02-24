#pragma once
#include <memory>
#include <variant>
#include <optional>
#include <type_traits>
#include <BWAPI.h>
#include <bwem.h>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <array>

class ProtoBotCommander;
class ScoutingProbe;
class ScoutingZealot;
class ScoutingObserver;

// include concrete behaviors
#include "ScoutingProbe.h"
#include "ScoutingZealot.h"
// #include "ScoutingDragoon.h"
#include "ScoutingObserver.h"

using BehaviorVariant = std::variant<
    std::monostate,
    ScoutingProbe,
    ScoutingZealot,
    ScoutingObserver
    /*Once added ->,
    ScoutingDragoon,
    */
>;

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

    void markScout(BWAPI::Unit u);          // call when a unit becomes a scout
    void unmarkScout(BWAPI::Unit u);        // call when it stops being a scout
    bool isScout(BWAPI::Unit u) const;      // handy for other modules (e.g., Squad draw)
    bool isCombatScout(BWAPI::Unit u) const;
    bool hasWorkerScout() const;
    void drawScoutTags() const;             // draw labels for all scouts

    void setCombatScoutingStarted(bool v) { combatScoutingStarted_ = v; }
    bool combatScoutingStarted() const { return combatScoutingStarted_; }

    bool canAcceptWorkerScout() const { return !combatScoutingStarted_ && !workerScout_; }

    bool canAcceptCombatScout(BWAPI::UnitType t) const 
    {
        if (t == BWAPI::UnitTypes::Protoss_Zealot)
            return int(combatZealots_.size()) < maxZealotScouts_;
        if (t == BWAPI::UnitTypes::Protoss_Dragoon)
            return int(combatDragoons_.size()) < maxDragoonScouts_;
        return false;
    }

    bool canAcceptObserverScout() const { return (int)observerScouts_.size() < maxObserverScouts_; }

    int numCombatScouts() const 
    {
        return int(combatZealots_.size() + combatDragoons_.size());
    }

    BWAPI::Unit getAvaliableDetectors();

private:
    ProtoBotCommander* commanderRef = nullptr;

    std::unordered_map<int, BehaviorVariant> behaviors_;

    // helpers
    BehaviorVariant constructBehaviorFor(BWAPI::Unit unit);
    static BWAPI::Unit findUnitById(int id);

    std::vector<BWAPI::Unit> scouts_;
    BWAPI::Unit workerScout_{ nullptr };
    bool combatScoutingStarted_{ false };
    std::optional<BWAPI::TilePosition> enemyMainCache_;
    std::optional<BWAPI::TilePosition> enemyNaturalCache_;

   // Number of scouts
    int  maxZealotScouts_{ 2 };
    int  maxDragoonScouts_{ 3 };
    int  maxObserverScouts_{ 4 };
    int proxyPatrolZealotId_ = -1;

    std::vector<BWAPI::Unit> combatZealots_;
    std::vector<BWAPI::Unit> combatDragoons_;

    // assigned observer units + slot ownership (slot 0..3)
    std::vector<BWAPI::Unit> observerScouts_;
    int observerSlotOwner_[4] = { -1, -1, -1, -1 }; // unitID or -1

    // helpers for observer slots
    int  reserveObserverSlot(int unitId);   // returns slot [0..3] or -1
    void releaseObserverSlot(int unitId);



};
