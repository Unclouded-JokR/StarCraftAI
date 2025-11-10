#pragma once
#include <BWAPI.h>
#include <bwem.h>
#include <vector>
#include <optional>

class ProtoBotCommander; // forward (optional)

class ScoutingManager
{
public:
    explicit ScoutingManager(ProtoBotCommander* commander = nullptr);

    void onStart();
    void onFrame();

    // Call when your bot positively knows enemy main (e.g., other systems)
    void setEnemyMain(const BWAPI::TilePosition& tp);

    // Toggle features
    void setGasStealEnabled(bool v) { enableGasSteal = v; }

private:
    enum class State {
        Search,         // visit starting locations
        TravelToTarget, // moving to next start location
        InEnemyBase,    // enemy main located (do intel/optional gas steal)
        Orbit,          // circle around enemy base when chased by workers
        Retreat,        // run away when in danger
        ReturnHome,     // go home after finished/danger
        Done            // no further scouting
    };

    // ---- helpers ----
    void assignScoutIfNeeded();
    void buildStartTargets();
    void pickNextTarget();
    void issueMoveToward(const BWAPI::Position& p, int reissueDist = 96);
    bool tryHarassWorker();
    bool threatenedNow() const;
    void escapeMove(const BWAPI::Position& p);
    void ensureOrbitWaypoints();
    BWAPI::Position currentOrbitPoint() const;
    void advanceOrbitIfArrived();
    bool enemyBaseLikelyAt(const BWAPI::TilePosition& startTile) const;
    bool seeAnyEnemyBuildingNear(const BWAPI::Position& p, int radius) const;

    // Gas steal (Protoss only)
    bool tryGasSteal();
    bool anyRefineryOn(const BWAPI::Unit geyser) const;
    bool gasStealDone = false;
    BWAPI::Unit targetGeyser = nullptr;
    int nextGasStealRetryFrame = 0;

    // Orbit behavior
    BWAPI::Position orbitPoint(const BWAPI::Position& center, int radius, int angleDeg);

    // Danger heuristics
    bool chasedByWorkers() const;
    bool threatenedByCombat() const;

    // ---- data ----
    ProtoBotCommander* commanderRef = nullptr;

    BWAPI::Unit scout = nullptr;
    std::vector<BWAPI::TilePosition> startTargets;
    int nextTarget = 0;

    std::optional<BWAPI::TilePosition> enemyMainTile;
    BWAPI::Position enemyMainPos = BWAPI::Positions::Invalid;

    State state = State::Search;

    // knobs
    bool enableGasSteal = true;            // switch off if you don’t want it
    int  closeEnoughToTarget = 200;        // px
    int  orbitRadius = 192;                // px around enemy main
    int  lowHPShield = 10;                  // probe dangerous when shields <= this
    int  orbitAngleStep = 35;              // degrees
    int  lastOrbitAngle = 0;

    static constexpr int gasStealRetryCooldown = 36;    // ~1.5 sec at 24 fps

    // movement throttling
    int lastMoveIssueFrame = -99999;
    static constexpr int moveCooldownFrames = 6; // ~0.25s @ 25 fps

    // orbit / waypointing
    std::vector<BWAPI::Position> orbitWaypoints;
    int orbitIdx = 0;
    int dwellUntilFrame = 0;
    static constexpr int dwellFrames = 12; // ~0.5s at each point
};