#pragma once
#include <BWAPI.h>
#include <bwem.h>
#include <optional>
#include <vector>
#include <optional>
#include <limits>
#include <cmath>

class ProtoBotCommander;
class ScoutingManager;



class ScoutingObserver 
{
public:
    explicit ScoutingObserver(ProtoBotCommander* cmd = nullptr, ScoutingManager* mgr = nullptr)
        : commanderRef(cmd), manager(mgr) {
    }

    // lifecycle
    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit u);
    void assign(BWAPI::Unit u);
    void setEnemyMain(const BWAPI::TilePosition& tp);

    // manager provides this after assign()
    void setObserverSlot(int idx) { slotIndex = idx; }

private:
    enum class State { Idle, MoveToPost, Hold, AvoidDetection, Done };

    ProtoBotCommander* commanderRef = nullptr;
    ScoutingManager* manager = nullptr;

    BWAPI::Unit observer{ nullptr };
    State       state = State::Idle;

    // posts in priority order [0..3]
    std::vector<BWAPI::Position> posts;
    int slotIndex = -1;

    std::optional<BWAPI::TilePosition> enemyMainTile;
    BWAPI::Position enemyMainPos = BWAPI::Positions::Invalid;

    int lastMoveFrame = 0;
    int lastThreatFrame = -10000;

    // tuning
    static constexpr int kMoveCooldown = 6;          // frames
    static constexpr int kSafeFramesToReturn = 48;   // ~2 sec
    static constexpr int kEdgeMarginPx = 8;          // padding outside detection
    static constexpr int kHoldReissueDist = 16;
    static constexpr int kAirThreatThreshold = 40;   // Air threat "Danger" value
    static constexpr int kDetectReactBufferPx = 48;  // Stay a decent buffer distance from the detection radius

    static constexpr int kSlot3RebuildEveryFrames = 24 * 6;     // every 6s
    static constexpr int kSlot3MinBetweenMoves = 24;            // 1s
    static constexpr int kSlot3ArriveDist = 96;

    // --- slot 3 roam scout ---
    BWAPI::Position slot3Home = BWAPI::Positions::Invalid;
    bool slot3HomeSet = false;
    bool slot3ReturningHome = false;
    std::vector<BWAPI::Position> slot3Checkpoints;
    int slot3NextRebuildFrame = 0;
    bool slot3NeedsRebuild = false;
    int slot3NextIdx = 0;
    int slot3NextMoveFrame = 0;
    BWAPI::Position slot3CurTarget = BWAPI::Positions::Invalid;


    // helpers
    void computePosts(); // fills posts[0..3]
    BWAPI::Position postTarget() const;
    void issueMove(const BWAPI::Position& p, bool force = false, int reissueDist = 64);
    bool detectorThreat(BWAPI::Position& avoidTo) const; // returns true + sets a safe point
    static int detectionRadiusFor(const BWAPI::UnitType& t);
    static BWAPI::Position clampToMapPx(const BWAPI::Position& p, int margin = 16);
    static double groundPathLengthPx(const BWAPI::Position& from, const BWAPI::Position& to);
    bool isUnsafe(const BWAPI::Position& p) const;
    BWAPI::Position pickDetourToward(const BWAPI::Position& target) const;
    bool haveVisionAt(const BWAPI::Position& p, int radiusPx) const;
    void rebuildSlot3Checkpoints();

};