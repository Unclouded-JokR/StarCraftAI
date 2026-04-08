#pragma once
#include <BWAPI.h>
#include <bwem.h>
#include <vector>
#include <optional>
#include "A-StarPathfinding.h"

#include <chrono>
#include <string>

class ProtoBotCommander;
class ScoutingManager;

// Concrete behavior for a Probe scout (your current logic moved here)
class ScoutingProbe {
public:
    explicit ScoutingProbe(ProtoBotCommander* commander, ScoutingManager* manager)
        : commanderRef(commander), manager(manager) {}

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);

    void setEnemyMain(const BWAPI::TilePosition& tp);
    void assign(BWAPI::Unit unit);
    bool hasScout() const { return scout && scout->exists(); }

    void drawDebug() const;


private:
    enum class State { Search, GasSteal, Harass, Orbit, ReturningCargo, Done };

    // core refs
    ProtoBotCommander* commanderRef = nullptr;
    ScoutingManager* manager = nullptr;
    BWAPI::Unit scout = nullptr;

    // enemy info
    std::optional<BWAPI::TilePosition> enemyMainTile;
    BWAPI::Position enemyMainPos = BWAPI::Positions::Invalid;


    // targets
    std::vector<BWAPI::TilePosition> startTargets;
    std::size_t nextTarget = 0;
    State state = State::Done;

    // gas steal
    bool gasStealDone = false;
    BWAPI::Unit targetGeyser = nullptr;
    int nextGasStealRetryFrame = 0;
    bool gasStealRequested = false;
    bool gasStealApproved = false;
    int gasStealRequestFrame = 0;
    int nextCheesePollFrame = 0;
    bool gasStealHoldingForMinerals = false;
    int nextMineralCheckFrame = 0;

    // misc
    int lastMoveIssueFrame = 0;
    int lastHP = -1, lastShields = -1;
    int lastThreatFrame = -999999;
    BWAPI::Position lastPos = BWAPI::Positions::Invalid;
    int lastProgressFrame = 0;
    int lastProgressDist = INT_MAX;
    int sidestepDir = 1;
    int sidestepAttempts = 0;
    int nextReplanFrame = 0;
    BWAPI::Position lastPlannedGoal = BWAPI::Positions::Invalid;
    BWAPI::Position currentMoveGoal = BWAPI::Positions::Invalid;

    // A* escape mode (only when jammed)
    int aStarEscapeUntilFrame = 0;
    BWAPI::Position aStarEscapeGoal = BWAPI::Positions::Invalid;

    // stuck detector state
    BWAPI::Position lastStuckPos = BWAPI::Positions::Invalid;
    int lastStuckCheckFrame = 0;
    int stuckFrames = 0;


    // orbit
    std::vector<BWAPI::Position> orbitWaypoints;
    std::vector<BWAPI::Position> plannedPath;
    std::size_t orbitIdx = 0;
    int dwellUntilFrame = 0;

    // ---- tunables ----
    static constexpr int kCloseEnoughToTarget = 96;
    static constexpr int kMoveCooldownFrames = 8;
    static constexpr int kOrbitRadius = 350;
    static constexpr int kGasStealRetryCooldown = 24;
    static constexpr int kHarassRadiusFromMain = 320;
    static constexpr int kThreatRearmFrames = 8;
    static constexpr int kCalmFramesToResumeHarass = 72; // ~3 seconds at 24 FPS
    static constexpr int kReplanFrames = 24;          // 1s
    static constexpr int kGoalChangeReplanDist = 96;  // px

    // helpers
    void buildStartTargets();
    void issueMoveToward(const BWAPI::Position& p, int reissueDist = 32, bool force = false);
    bool seeAnyEnemyBuildingNear(const BWAPI::Position& p, int radius) const;
    bool anyRefineryOn(BWAPI::Unit geyser) const;

    bool tryGasSteal();
    bool tryHarassWorker();

    void ensureOrbitWaypoints();
    BWAPI::Position currentOrbitPoint() const;
    void advanceOrbitIfArrived();
    bool threatenedNow() const;
    BWAPI::Position getAvgPosition();
    bool planTerrainPathTo(const BWAPI::Position& goal);
    bool hasPlannedPath() const { return !plannedPath.empty(); }
    BWAPI::Position currentPlannedWaypoint() const;
    BWAPI::Position computeSidestepTarget(const BWAPI::Position& goal);
    bool isStuck(int now);
    BWAPI::Position computeEscapeGoal() const;
    BWAPI::Unit findAssimilatorOnTargetGeyser() const;
    bool tryConfirmEnemyMainByStartLocations();

    // returning cargo QoL
    void handleReturningCargoState();
    BWAPI::Unit findClosestDepot(const BWAPI::Position& from) const;

    // geometry & clamps
    BWAPI::Position computeOrbitCenter() const;
    static int angleDeg(const BWAPI::Position& from, const BWAPI::Position& to);
    static int normDeg(int d) { d %= 360; return d < 0 ? d + 360 : d; }
    static BWAPI::Position clampToMapPx(const BWAPI::Position& p, int marginPx = 32);
    static BWAPI::Position snapToNearestWalkable(BWAPI::Position p, int maxRadiusPx = 128);
    static BWAPI::Position snapToNearestWalkableClear(BWAPI::Position p, BWAPI::UnitType ut, int maxRadiusPx = 128);

    bool planAStarPathTo(const BWAPI::Position& goal, bool interactableEndpoint = false);
    void followPlannedPath(const BWAPI::Position& finalGoal, int reissueDist = 48);

    // Timer debug

    int dbgLastOrbitPrintFrame = 0;
    int dbgLastOrbitBuildMs = 0;
    int dbgLastOrbitReplanMs = 0;
    int dbgLastAStarMs = 0;
    int dbgLastSnapMs = 0;
};
