#pragma once
#include <memory>
#include <variant>
#include <optional>
#include <BWAPI.h>
#include <bwem.h>

class ProtoBotCommander;

// include concrete behaviors
#include "ScoutingProbe.h"
// #include "ScoutingZealot.h"
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
    void onUnitDestroy(BWAPI::Unit unit);

private:
    ProtoBotCommander* commanderRef = nullptr;

    using BehaviorVariant = std::variant<std::monostate, ScoutingProbe /*Once added -> , ScoutingZealot, ScoutingDragoon, ScoutingObserver*/>;
    BehaviorVariant behavior;

    // helpers
    void constructBehaviorFor(BWAPI::Unit unit);
};
