#pragma once
#include <BWAPI.h>
#include <bwem.h>
#include <vector>
#include <optional>

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

    // misc
    int lastMoveIssueFrame = 0;
    int lastHP = -1, lastShields = -1;
    int lastThreatFrame = -999999;

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

    // helpers (your existing functions)
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

    // returning cargo QoL
    void handleReturningCargoState();
    BWAPI::Unit findClosestDepot(const BWAPI::Position& from) const;

    // geometry & clamps
    BWAPI::Position computeOrbitCenter() const;
    static int angleDeg(const BWAPI::Position& from, const BWAPI::Position& to);
    static int normDeg(int d) { d %= 360; return d < 0 ? d + 360 : d; }
    static BWAPI::Position clampToMapPx(const BWAPI::Position& p, int marginPx = 32);
    static BWAPI::Position snapToNearestWalkable(BWAPI::Position p, int maxRadiusPx = 128);
};
