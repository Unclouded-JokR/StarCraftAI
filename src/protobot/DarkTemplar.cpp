#include "DarkTemplar.h"
#include "ProtoBotCommander.h"
#include <algorithm>

using namespace BWAPI;

/// <summary>
/// Initializes the dark templar scouting state.
/// 
/// Resets movement timing and sets the initial state to Idle.
/// </summary>

void DarkTemplar::onStart()
{
    state = State::Idle;
    lastMoveIssueFrame = 0;
}

/// <summary>
/// Sets the enemy main base location and updates behavior state.
/// 
/// If the dark templar is already assigned, transitions into
/// movement toward the enemy main. Otherwise waits for assignment.
/// </summary>
/// <param name="tp">Enemy main tile position</param>

void DarkTemplar::setEnemyMain(const BWAPI::TilePosition& tp)
{
    enemyMainTile = tp;
    enemyMainPos = BWAPI::Position(tp);

    if (darkTemplar && darkTemplar->exists())
    {
        state = State::MoveToEnemyMain;
    }
    else
    {
        state = State::WaitEnemyMain;
    }
}

/// <summary>
/// Assigns a unit as the dark templar scout.
/// 
/// Verifies the unit type and sets the initial movement state
/// based on whether the enemy main is already known.
/// </summary>
/// <param name="unit">Unit to assign as scout</param>

void DarkTemplar::assign(BWAPI::Unit unit)
{
    if (!unit || !unit->exists())
    {
        return;
    }

    if (unit->getType() != BWAPI::UnitTypes::Protoss_Dark_Templar)
    {
        return;
    }

    darkTemplar = unit;

    if (enemyMainPos.isValid())
    {
        state = State::MoveToEnemyMain;
    }
    else
    {
        state = State::WaitEnemyMain;
    }
}

/// <summary>
/// Handles cleanup when the dark templar is destroyed.
/// 
/// Clears the tracked unit, resets locked target state,
/// and marks the behavior as complete.
/// </summary>
/// <param name="unit">Destroyed unit</param>

void DarkTemplar::onUnitDestroy(BWAPI::Unit unit)
{
    if (!unit)
    {
        return;
    }

    if (darkTemplar == unit)
    {
        darkTemplar = nullptr;
        lockedTargetId = -1;
        state = State::Done;
    }
}

static bool isValidDTTarget(BWAPI::Unit dt, BWAPI::Unit enemy)
{
    if (!dt || !dt->exists() || !enemy || !enemy->exists())
    {
        return false;
    }

    if (enemy->isFlying())
    {
        return false;
    }

    if (!dt->canAttackUnit(enemy))
    {
        return false;
    }

    return true;
}

/// <summary>
/// Main update loop executed every frame.
/// 
/// Controls state transitions and behavior including:
/// - Waiting for enemy main information
/// - Moving toward the enemy main
/// - Approaching through the enemy natural
/// - Attacking targets inside the enemy base
/// </summary>

void DarkTemplar::onFrame()
{
    BWAPI::Unit dt = darkTemplar;

    if (state == State::Done || !dt || !dt->exists())
    {
        return;
    }

    if (!enemyMainPos.isValid() && commanderRef)
    {
        const auto& e = commanderRef->enemy();
        if (e.main.has_value())
        {
            setEnemyMain(*e.main);
        }
    }

    switch (state)
    {
        case State::Idle:
        {
            state = enemyMainPos.isValid() ? State::MoveToEnemyMain : State::WaitEnemyMain;
            break;
        }

        case State::WaitEnemyMain:
        {
            if (enemyMainPos.isValid())
            {
                state = State::MoveToEnemyMain;
            }
            break;
        }

        case State::MoveToEnemyMain:
        {
            if (enemyMainPos.isValid() && dt->getDistance(enemyMainPos) <= kMainPriorityRadiusPx)
            {
                lockedTargetId = -1;
                state = State::AttackEnemyMain;
                break;
            }

            if (manager)
            {
                auto nat = manager->getEnemyNatural();
                if (nat.has_value())
                {
                    state = State::ApproachNatural;
                    break;
                }
            }

            BWAPI::Unit target = getLockedTarget();

            if (!target || !target->exists())
            {
                target = findApproachTarget(dt);

                if (target && target->exists())
                {
                    lockedTargetId = target->getID();
                }
                else
                {
                    lockedTargetId = -1;
                }
            }

            if (target && target->exists())
            {
                if (dt->getOrderTarget() != target)
                {
                    dt->attack(target);
                }
            }
            else if (dt->getDistance(enemyMainPos) > kArriveDistPx)
            {
                if (dt->getLastCommandFrame() < BWAPI::Broodwar->getFrameCount() - kMoveCooldownFrames)
                {
                    dt->attack(enemyMainPos);
                }
            }
            else
            {
                state = State::ApproachNatural;
            }

            break;
        }

        case State::ApproachNatural:
        {
            BWAPI::Position approachPos = getNaturalApproachPosition();

            if (!approachPos.isValid())
            {
                if (enemyMainPos.isValid())
                {
                    approachPos = enemyMainPos;
                }
                else
                {
                    break;
                }
            }

            const int dist = dt->getDistance(approachPos);

            const int mainPriorityRadius = 10 * 32;

            if (enemyMainPos.isValid() && dt->getDistance(enemyMainPos) <= mainPriorityRadius)
            {
                lockedTargetId = -1;
                state = State::AttackEnemyMain;
                break;
            }

            BWAPI::Unit target = getLockedTarget();

            if (!target || !target->exists())
            {
                target = findApproachTarget(dt);

                if (target && target->exists())
                {
                    lockedTargetId = target->getID();
                }
                else
                {
                    lockedTargetId = -1;
                }
            }

            if (target && target->exists())
            {
                if (dt->getOrderTarget() != target)
                {
                    dt->attack(target);
                }
            }
            else if (dt->getLastCommandFrame() < BWAPI::Broodwar->getFrameCount() - kMoveCooldownFrames)
            {
                dt->attack(approachPos);
            }

            break;
        }

        case State::AttackEnemyMain:
        {
            BWAPI::Unit target = getLockedTarget();

            if (!target || !target->exists())
            {
                target = pickTargetFor(dt);

                if (target && target->exists())
                {
                    lockedTargetId = target->getID();
                }
                else
                {
                    lockedTargetId = -1;
                }
            }

            if (target && target->exists())
            {
                if (dt->getOrderTarget() != target)
                {
                    dt->attack(target);
                }
            }
            else
            {
                issueMove(dt, enemyMainPos);
            }
            break;
        }

        case State::Done:
        default:
        {
            break;
        }
    }
}

/// <summary>
/// Issues a movement command with cooldown control.
/// 
/// Prevents command spam by throttling repeated move orders.
/// </summary>
/// <param name="unit">Unit receiving the command</param>
/// <param name="p">Target position</param>
/// <param name="force">Force movement regardless of cooldown</param>

void DarkTemplar::issueMove(BWAPI::Unit unit, const BWAPI::Position& p, bool force)
{
    if (!unit || !unit->exists() || !p.isValid())
    {
        return;
    }

    const int now = BWAPI::Broodwar->getFrameCount();

    if (!force && (now - lastMoveIssueFrame) < kMoveCooldownFrames)
    {
        return;
    }

    if (force || !unit->isMoving() || unit->getDistance(p) > 48)
    {
        unit->move(p);
        lastMoveIssueFrame = now;
    }
}

/// <summary>
/// Returns the currently locked target, if it still exists.
/// </summary>
/// <returns>Locked enemy unit, or nullptr if unavailable</returns>

BWAPI::Unit DarkTemplar::getLockedTarget() const
{
    if (lockedTargetId < 0)
    {
        return nullptr;
    }

    for (BWAPI::Unit enemy : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (!enemy || !enemy->exists())
        {
            continue;
        }

        if (enemy->getID() == lockedTargetId)
        {
            return enemy;
        }
    }

    return nullptr;
}

/// <summary>
/// Returns the preferred approach position near the enemy natural.
/// 
/// Falls back to the enemy main if no natural location is known.
/// </summary>
/// <returns>Approach position for the dark templar</returns>

BWAPI::Position DarkTemplar::getNaturalApproachPosition() const
{
    if (manager)
    {
        auto nat = manager->getEnemyNatural();
        if (nat.has_value())
        {
            return BWAPI::Position(nat.value());
        }
    }

    if (enemyMainPos.isValid())
    {
        return enemyMainPos;
    }

    return BWAPI::Positions::Invalid;
}

/// <summary>
/// Finds a nearby target to attack while approaching the enemy base.
/// 
/// Used during movement states to opportunistically attack
/// visible nearby targets without fully switching behavior.
/// </summary>
/// <param name="dt">Dark templar unit</param>
/// <returns>Best nearby target, or nullptr if none found</returns>

BWAPI::Unit DarkTemplar::findApproachTarget(BWAPI::Unit dt) const
{
    if (!dt || !dt->exists())
    {
        return nullptr;
    }

    BWAPI::Unit best = nullptr;
    int bestDist = INT_MAX;

    for (BWAPI::Unit enemy : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (!enemy || !enemy->exists())
        {
            continue;
        }

        if (!enemy->isVisible())
        {
            continue;
        }

        const int d = dt->getDistance(enemy);

        // only grab things reasonably close during the walk-in
        if (d > 8 * 32)
        {
            continue;
        }

        if (!isValidDTTarget(dt, enemy))
        {
            continue;
        }

        if (d < bestDist)
        {
            bestDist = d;
            best = enemy;
        }
    }

    return best;
}

/// <summary>
/// Checks whether a unit type is considered a detector building.
/// </summary>
/// <param name="type">Unit type to check</param>
/// <returns>True if the type is a detector building</returns>

bool DarkTemplar::isDetectorBuildingType(BWAPI::UnitType type)
{
    return
        type == BWAPI::UnitTypes::Terran_Missile_Turret ||
        type == BWAPI::UnitTypes::Terran_Comsat_Station ||
        type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
        type == BWAPI::UnitTypes::Protoss_Observatory ||
        type == BWAPI::UnitTypes::Zerg_Spore_Colony;
}

bool DarkTemplar::isDetectorBuilding(BWAPI::Unit unit)
{
    if (!unit || !unit->exists())
    {
        return false;
    }

    const BWAPI::UnitType type = unit->getType();

    if (isDetectorBuildingType(type))
    {
        return true;
    }

    return false;
}

/// <summary>
/// Checks whether a unit is a worker.
/// </summary>
/// <param name="unit">Unit to check</param>
/// <returns>True if the unit is a worker</returns>

bool DarkTemplar::isWorker(BWAPI::Unit unit)
{
    return unit && unit->exists() && unit->getType().isWorker();
}

/// <summary>
/// Checks whether a unit is a building that is not specifically a detector.
/// </summary>
/// <param name="unit">Unit to check</param>
/// <returns>True if the unit is a building</returns>

bool DarkTemplar::isBuilding(BWAPI::Unit unit)
{
    return unit && unit->exists() && unit->getType().isBuilding();
}

/// <summary>
/// Selects the highest priority target inside the enemy base.
/// 
/// Priority order:
/// - Detector buildings
/// - Workers
/// - Other buildings
/// </summary>
/// <param name="dt">Dark templar unit</param>
/// <returns>Best target to attack, or nullptr if none found</returns>

BWAPI::Unit DarkTemplar::pickTargetFor(BWAPI::Unit dt) const
{
    if (!dt || !dt->exists())
    {
        return nullptr;
    }

    BWAPI::Unit bestDetector = nullptr;
    int bestDetectorDist = INT_MAX;

    BWAPI::Unit bestWorker = nullptr;
    int bestWorkerDist = INT_MAX;

    BWAPI::Unit bestBuilding = nullptr;
    int bestBuildingDist = INT_MAX;

    for (BWAPI::Unit enemy : BWAPI::Broodwar->enemy()->getUnits())
    {

        if (!enemy || !enemy->exists())
        {
            continue;
        }

        if (!isValidDTTarget(dt, enemy))
        {
            continue;
        }

        const int d = dt->getDistance(enemy);

        if (d > kAttackRangePx)
        {
            continue;
        }

        if (isDetectorBuilding(enemy))
        {
            if (d < bestDetectorDist)
            {
                bestDetectorDist = d;
                bestDetector = enemy;
            }
            continue;
        }

        if (isWorker(enemy))
        {
            if (d < bestWorkerDist)
            {
                bestWorkerDist = d;
                bestWorker = enemy;
            }
            continue;
        }

        if (isBuilding(enemy))
        {
            if (d < bestBuildingDist)
            {
                bestBuildingDist = d;
                bestBuilding = enemy;
            }
        }
    }

    if (bestDetector)
    {
        return bestDetector;
    }

    if (bestWorker)
    {
        return bestWorker;
    }

    if (bestBuilding)
    {
        return bestBuilding;
    }

    return nullptr;
}

/// <summary>
/// Draws debug information for the dark templar scout.
/// 
/// Displays:
/// - Current state
/// - Locked target
/// - Enemy main direction
/// - Enemy natural direction
/// </summary>

void DarkTemplar::drawDebug() const
{
    if (!darkTemplar || !darkTemplar->exists())
    {
        return;
    }
    const char* stateName = "Unknown";

    switch (state)
    {
    case State::Idle:
        stateName = "Idle";
        break;
    case State::WaitEnemyMain:
        stateName = "WaitEnemyMain";
        break;
    case State::MoveToEnemyMain:
        stateName = "MoveToEnemyMain";
        break;
    case State::ApproachNatural:
        stateName = "ApproachNatural";
        break;
    case State::AttackEnemyMain:
        stateName = "AttackEnemyMain";
        break;
    case State::Done:
        stateName = "Done";
        break;
    default:
        stateName = "Unknown";
        break;
    }
   

    BWAPI::Position p = darkTemplar->getPosition();

    BWAPI::Broodwar->drawCircleMap(p, 22, BWAPI::Colors::Purple, false);
    BWAPI::Broodwar->drawTextMap(p.x - 30, p.y - 46, "\x05DT Scout");
    BWAPI::Broodwar->drawTextMap(p.x - 30, p.y - 34, "\x11State: %s", stateName);

    BWAPI::Unit target = getLockedTarget();
    if (target && target->exists())
    {
        BWAPI::Broodwar->drawLineMap(p, target->getPosition(), BWAPI::Colors::Red);
        BWAPI::Broodwar->drawCircleMap(target->getPosition(), 14, BWAPI::Colors::Red, false);
        BWAPI::Broodwar->drawTextMap(
            p.x - 30,
            p.y - 22,
            "\x08Target: %s",
            target->getType().getName().c_str()
        );
    }
    else
    {
        BWAPI::Broodwar->drawTextMap(p.x - 30, p.y - 22, "\x08Target: None");
    }

    if (enemyMainPos.isValid())
    {
        BWAPI::Broodwar->drawLineMap(p, enemyMainPos, BWAPI::Colors::Green);
    }

    if (manager)
    {
        auto nat = manager->getEnemyNatural();
        if (nat.has_value())
        {
            BWAPI::Position natPos(nat.value());
            BWAPI::Broodwar->drawLineMap(p, natPos, BWAPI::Colors::Orange);
        }
    }

   
}
