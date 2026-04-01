#pragma once
#include <BWAPI.h>
#include <optional>
#include <vector>

class ProtoBotCommander;
class ScoutingManager;

class DarkTemplar
{
public:
    explicit DarkTemplar(ProtoBotCommander* commander, ScoutingManager* manager)
        : commanderRef(commander), manager(manager)
    {
    }

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);

    void setEnemyMain(const BWAPI::TilePosition& tp);
    void assign(BWAPI::Unit unit);


private:
    enum class State
    {
        Idle,
        WaitEnemyMain,
        MoveToEnemyMain,
        ApproachNatural,
        AttackEnemyMain,
        Done
    };

    ProtoBotCommander* commanderRef = nullptr;
    ScoutingManager* manager = nullptr;

    BWAPI::Unit darkTemplar = nullptr;
    int lockedTargetId = -1;

    std::optional<BWAPI::TilePosition> enemyMainTile;
    BWAPI::Position enemyMainPos = BWAPI::Positions::Invalid;

    State state = State::Idle;

    int lastMoveIssueFrame = 0;
    static constexpr int kMoveCooldownFrames = 8;
    static constexpr int kAttackRangePx = 20 * 32;
    static constexpr int kArriveDistPx = 160;
    static constexpr int kMainPriorityRadiusPx = 14 * 32;

    void issueMove(BWAPI::Unit unit, const BWAPI::Position& p, bool force = false);
    BWAPI::Unit pickTargetFor(BWAPI::Unit dt) const;
    BWAPI::Unit getLockedTarget() const;
    BWAPI::Position getNaturalApproachPosition() const;
    BWAPI::Unit findApproachTarget(BWAPI::Unit dt) const;

    static bool isDetectorBuildingType(BWAPI::UnitType type);
    static bool isDetectorBuilding(BWAPI::Unit unit);
    static bool isWorker(BWAPI::Unit unit);
    static bool isBuilding(BWAPI::Unit unit);
};