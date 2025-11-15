#pragma once
#include <BWAPI.h>
#include <bwem.h>
#include <vector>
#include <optional>
#include <cstddef>

class ProtoBotCommander;

class ScoutingManager {
public:
    explicit ScoutingManager(ProtoBotCommander* commander = nullptr);

    void onStart();
    void onFrame();

    // New: assign a specific unit to be the scout (called by ProtoBotCommander)
    void assignScout(BWAPI::Unit unit);
    bool hasScout() const { return scout && scout->exists(); }

    // Unchanged helper
    void setEnemyMain(const BWAPI::TilePosition& tp);

    // New: clean de-assignment when units die
    void onUnitDestroy(BWAPI::Unit unit);

private:
    enum class State { Search, GasSteal, Harass, Orbit, Done };

    // ---- core data ----
    ProtoBotCommander* commanderRef = nullptr;
    BWAPI::Unit scout = nullptr;

    std::optional<BWAPI::TilePosition> enemyMainTile;
    BWAPI::Position enemyMainPos = BWAPI::Positions::Invalid;

    std::vector<BWAPI::TilePosition> startTargets;
    std::size_t nextTarget = 0;
    State state = State::Done;

    // Gas steal bookkeeping
    bool gasStealDone = false;
    BWAPI::Unit targetGeyser = nullptr;
    int nextGasStealRetryFrame = 0;

    // Misc
    int lastMoveIssueFrame = 0;

    // ---- tunables (kept small/simple) ----
    static constexpr int kCloseEnoughToTarget   = 96;
    static constexpr int kMoveCooldownFrames    = 8;
    static constexpr int kOrbitRadius           = 350;
    static constexpr int kGasStealRetryCooldown = 24; // ~1s
    static constexpr int kHarassRadiusFromMain  = 320;

    // ---- helpers ----
    void buildStartTargets();
    void issueMoveToward(const BWAPI::Position& p, int reissueDist = 32, bool force = false);

    bool seeAnyEnemyBuildingNear(const BWAPI::Position& p, int radius) const;
    bool anyRefineryOn(BWAPI::Unit geyser) const;

    bool tryGasSteal();     // returns true if it issued a command this frame
    bool tryHarassWorker(); // returns true if it issued a command this frame

    void ensureOrbitWaypoints();
    BWAPI::Position currentOrbitPoint() const;
    void advanceOrbitIfArrived();
    bool threatenedNow() const;
    BWAPI::Position getAvgPosition();
    bool planTerrainPathTo(const BWAPI::Position& goal);
    bool hasPlannedPath() const { return !plannedPath.empty(); }
    BWAPI::Position currentPlannedWaypoint() const;
    static inline BWAPI::Position clampToMapPx(const BWAPI::Position& p, int marginPx = 32);
    static BWAPI::Position snapToNearestWalkable(BWAPI::Position p, int maxRadiusPx = 128);

    BWAPI::Position computeOrbitCenter() const;                 // enemy main if known, else scout pos
    static int normDeg(int d) { d %= 360; return d < 0 ? d + 360 : d; }
    static int angleDeg(const BWAPI::Position& from, const BWAPI::Position& to);

    // orbit data
    std::vector<BWAPI::Position> orbitWaypoints;
    std::vector<BWAPI::Position> plannedPath;
    std::size_t orbitIdx = 0;
    int dwellUntilFrame = 0;

    // ---- ScoutingManager members ----
    int lastHP = -1, lastShields = -1;
    int lastThreatFrame = -999999;
    static constexpr int kThreatRearmFrames = 8;
};
