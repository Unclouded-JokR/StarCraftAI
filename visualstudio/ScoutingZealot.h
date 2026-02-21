#pragma once
#include <BWAPI.h>
#include <bwem.h>
#include <optional>
#include <vector>

class ProtoBotCommander;
class ScoutingManager;

class ScoutingZealot
{
public:
    explicit ScoutingZealot(ProtoBotCommander* commander, ScoutingManager* manager)
        : commanderRef(commander), manager(manager)
    {
    }

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);

    void setEnemyMain(const BWAPI::TilePosition& tp);
    void assign(BWAPI::Unit unit);
    bool hasScout() const { return zealot && zealot->exists(); }

    void setProxyPatroller(bool v)
    {
        isProxyPatroller = v;
    }

private:
    enum class State
    {
        Idle,
        WaitEnemyMain,
        ProxyPatrol,
        MoveToNatural,
        HoldEdge,
        Reposition,
        Done
    };
    State returnStateAfterReposition = State::HoldEdge;
    ProtoBotCommander* commanderRef = nullptr;
    ScoutingManager* manager = nullptr;
    BWAPI::Unit zealot = nullptr;

    std::optional<BWAPI::TilePosition> enemyMainTile;
    BWAPI::Position enemyMainPos = BWAPI::Positions::Invalid;
    BWAPI::TilePosition enemyNaturalTile = BWAPI::TilePositions::Invalid;
    BWAPI::Position enemyNaturalPos = BWAPI::Positions::Invalid;

    State state = State::Idle;
    int lastMoveIssueFrame = 0;
    static constexpr int kMoveCooldownFrames = 8;
    static constexpr int kEdgeMarginPx = 24;
    static constexpr int kThreatRadiusPx = 256;
    static constexpr int kRepositionStepPx = 160;
    static constexpr int kCalmFramesToReturn = 72;
    int lastThreatFrame = -100000;
    static constexpr int kDangerClosePx = 96; // tune: 96-160
    int lastAttackCmdFrame = -100000;
    int lastTargetSelectFrame = -100000;
    static constexpr int kTargetStickFrames = 18;

    // --- proxy patrol tuning ---
    static constexpr int kProxyRebuildEveryFrames = 24 * 10; // 10s
    static constexpr int kProxyArriveDist = 96;
    static constexpr int kProxyMinBetweenMoves = 12;
    static constexpr double kMaxGroundDist = 180 * 32;

    std::vector<BWAPI::Position> proxyPoints;
    int proxyNextIdx = 0;
    int proxyNextRebuildFrame = 0;
    int proxyNextMoveFrame = 0;
    BWAPI::Position proxyCurTarget = BWAPI::Positions::Invalid;
    bool isProxyPatroller = false;

    // --- Scout Stuck fix --- 

    BWAPI::Position cachedPerch = BWAPI::Positions::Invalid;
    int cachedPerchFrame = -100000;

    BWAPI::Position lastIssuedGoal = BWAPI::Positions::Invalid;

    BWAPI::Position lastPos = BWAPI::Positions::Invalid;
    int stuckFrames = 0;

    static constexpr int kPerchRecalcFrames = 24;       // 1 sec
    static constexpr int kGoalChangeResetDist = 64;     // px

    // --- helpers ---
    BWAPI::Position homeRetreatPoint() const;
    void computeEnemyNatural();
    BWAPI::Position pickEdgeOfVisionSpot();
    BWAPI::Position findReachableNearby(const BWAPI::Position& desired) const;
    
    bool threatenedNow() const;
    void issueMove(const BWAPI::Position& p, bool force = false, int reissueDist = 32);

    void rebuildProxyPoints();
    bool isNear(const BWAPI::Position& a, const BWAPI::Position& b, int distPx) const;

    static double groundPathLengthPx(const BWAPI::Position& from, const BWAPI::Position& to);
    static BWAPI::Position clampToMapPx(const BWAPI::Position& p, int marginPx = 32);

    BWAPI::Unit findPrimaryThreat(int radiusPx) const;
    void retreatHomeMicro(BWAPI::Unit threat);
    BWAPI::Unit findInWeaponRangeTarget() const;
    
    BWAPI::Unit lastAttackTarget = nullptr;

    int attackCommitFrames() const;
    bool tryFireAndCommit(BWAPI::Unit target);
    bool isGoodKiteTarget(BWAPI::Unit u, int radiusPx) const;
};