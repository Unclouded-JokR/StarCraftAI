#include "ThreatGrid.h"
#include <cmath>

/// <summary>
/// Initializes threat grids based on map dimensions.
/// 
/// Creates walk-tile resolution grids for:
/// - Ground threat
/// - Air threat
/// - Detection coverage
/// </summary>

void ThreatGrid::onStart()
{
    const int walkW = BWAPI::Broodwar->mapWidth() * 4;
    const int walkH = BWAPI::Broodwar->mapHeight() * 4;

    groundThreat_.init(walkW, walkH);
    detection_.init(walkW, walkH);

    stamps_.clear();
}

/// <summary>
/// Updates the current frame reference for the grid.
/// </summary>
/// <param name="frame">Current game frame</param>

void ThreatGrid::onFrameStart(int frame)
{
    currentFrame_ = frame;
}

/// <summary>
/// Returns the ground threat value at a given position.
/// </summary>
/// <param name="p">World position</param>
/// <returns>Accumulated ground threat value</returns>

int ThreatGrid::groundThreatAt(BWAPI::Position p) const
{
    return groundThreat_.get(toWalkX(p), toWalkY(p));
}

/// <summary>
/// Returns the detection threat value at a given position.
/// </summary>
/// <param name="p">World position</param>
/// <returns>Detection intensity value</returns>

int ThreatGrid::detectionAt(BWAPI::Position p) const
{
    return detection_.get(toWalkX(p), toWalkY(p));
}

/// <summary>
/// Returns the air threat value at a given position.
/// </summary>
/// <param name="p">World position</param>
/// <returns>Accumulated air threat value</returns>

int ThreatGrid::airThreatAt(BWAPI::Position p) const
{
    return airThreat_.get(toWalkX(p), toWalkY(p));
}

int ThreatGrid::getAirThreat(BWAPI::Position p) const
{
    return airThreatAt(p);
}

/// <summary>
/// Wrapper for retrieving detection threat at a position.
/// </summary>

int ThreatGrid::getDetection(BWAPI::Position p) const
{
    return detectionAt(p);
}

/// <summary>
/// Adds or updates an enemy unit's contribution to the threat grid.
/// 
/// If the unit state has changed, its previous contribution
/// is removed and recalculated.
/// </summary>
/// <param name="id">Unique unit ID</param>
/// <param name="type">Unit type</param>
/// <param name="pos">Unit position</param>
/// <param name="completed">Whether the unit is complete</param>
/// <param name="burrowed">Whether the unit is burrowed</param>
/// <param name="immobile">Whether the unit is immobile</param>

void ThreatGrid::addOrUpdateEnemy(int id, BWAPI::UnitType type, BWAPI::Position pos, bool completed, bool burrowed, bool immobile)
{
    EnemyStamp next;
    next.type = type;
    next.pos = pos;
    next.completed = completed;
    next.burrowed = burrowed;
    next.immobile = immobile;

    auto it = stamps_.find(id);
    if (it == stamps_.end())
    {
        stamps_.emplace(id, next);
        stampEnemy(next, +1);
        return;
    }

    EnemyStamp& prev = it->second;

    const bool changed =
        prev.type != next.type ||
        prev.pos != next.pos ||
        prev.completed != next.completed ||
        prev.burrowed != next.burrowed ||
        prev.immobile != next.immobile;

    if (!changed)
    {
        return;
    }

    stampEnemy(prev, -1);
    prev = next;
    stampEnemy(prev, +1);
}

/// <summary>
/// Removes an enemy unit from the threat grid.
/// 
/// Clears its previously stamped threat values.
/// </summary>
/// <param name="id">Unit ID</param>

void ThreatGrid::removeEnemy(int id)
{
    auto it = stamps_.find(id);
    if (it == stamps_.end())
    {
        return;
    }

    stampEnemy(it->second, -1);
    stamps_.erase(it);
}

void ThreatGrid::paintDisc(GridData& g, BWAPI::Position center, int rangePx, int delta)
{
    const int cx = toWalkX(center);
    const int cy = toWalkY(center);

    const int rWalk = (rangePx >> 3) + 1;
    const int r2 = rangePx * rangePx;

    for (int dx = -rWalk; dx <= rWalk; ++dx)
    {
        for (int dy = -rWalk; dy <= rWalk; ++dy)
        {
            const int wx = cx + dx;
            const int wy = cy + dy;

            const int px = (wx << 3) + 4;
            const int py = (wy << 3) + 4;

            const int ddx = px - center.x;
            const int ddy = py - center.y;

            if (ddx * ddx + ddy * ddy <= r2)
            {
                g.add(wx, wy, delta);
            }
        }
    }
}

int ThreatGrid::detectorRadiusPx(BWAPI::UnitType t) const
{
    if (t == BWAPI::UnitTypes::Terran_Missile_Turret) return 224;
    if (t == BWAPI::UnitTypes::Protoss_Photon_Cannon) return 224;
    if (t == BWAPI::UnitTypes::Zerg_Spore_Colony) return 224;
    if (t == BWAPI::UnitTypes::Terran_Science_Vessel) return 224;
    if (t == BWAPI::UnitTypes::Protoss_Observer) return 224;

    int r = t.sightRange();
    if (r <= 0) r = 224;
    return r;
}

int ThreatGrid::groundThreatRangePx(BWAPI::UnitType t) const
{
    BWAPI::UnitType weaponType = t;

    if (t == BWAPI::UnitTypes::Terran_Bunker)
    {
        weaponType = BWAPI::UnitTypes::Terran_Marine;
    }

    BWAPI::WeaponType w = weaponType.groundWeapon();
    if (w == BWAPI::WeaponTypes::None)
    {
        return 0;
    }

    int range = w.maxRange();
    if (t == BWAPI::UnitTypes::Terran_Bunker)
    {
        range += 48;
    }

    return range + rangeBufferPx_;
}

int ThreatGrid::groundThreatValue(BWAPI::UnitType t) const
{
    BWAPI::UnitType weaponType = t;

    if (t == BWAPI::UnitTypes::Terran_Bunker)
    {
        weaponType = BWAPI::UnitTypes::Terran_Marine;
    }

    BWAPI::WeaponType w = weaponType.groundWeapon();
    if (w == BWAPI::WeaponTypes::None)
    {
        return 0;
    }

    int dmg = w.damageAmount() * w.damageFactor();
    int hits = weaponType.maxGroundHits();

    int mult = 1;
    if (t == BWAPI::UnitTypes::Terran_Bunker)
    {
        mult = 4;
    }

    return dmg * hits * mult;
}

int ThreatGrid::airThreatRangePx(BWAPI::UnitType t) const
{
    BWAPI::UnitType weaponType = t;

    if (t == BWAPI::UnitTypes::Terran_Bunker)
    {
        weaponType = BWAPI::UnitTypes::Terran_Marine;
    }

    BWAPI::WeaponType w = weaponType.airWeapon();
    if (w == BWAPI::WeaponTypes::None)
    {
        return 0;
    }

    int range = w.maxRange();
    if (t == BWAPI::UnitTypes::Terran_Bunker)
    {
        range += 48;
    }

    return range + rangeBufferPx_;
}

int ThreatGrid::airThreatValue(BWAPI::UnitType t) const
{
    BWAPI::UnitType weaponType = t;

    if (t == BWAPI::UnitTypes::Terran_Bunker)
    {
        weaponType = BWAPI::UnitTypes::Terran_Marine;
    }

    BWAPI::WeaponType w = weaponType.airWeapon();
    if (w == BWAPI::WeaponTypes::None)
    {
        return 0;
    }

    int dmg = w.damageAmount() * w.damageFactor();
    int hits = weaponType.maxAirHits();

    int mult = 1;
    if (t == BWAPI::UnitTypes::Terran_Bunker)
    {
        mult = 4;
    }

    return dmg * hits * mult;
}


void ThreatGrid::stampEnemy(const EnemyStamp& s, int delta)
{
    if (!s.completed)
    {
        return;
    }

    const BWAPI::UnitType t = s.type;

    if (t.isDetector())
    {
        const int r = detectorRadiusPx(t) + rangeBufferPx_;
        paintDisc(detection_, s.pos, r, delta);
    }

    if (s.immobile)
    {
        return;
    }

    const bool isLurker = (t == BWAPI::UnitTypes::Zerg_Lurker);

    if (isLurker && !s.burrowed)
    {
        return;
    }

    if (!isLurker && s.burrowed)
    {
        return;
    }

    const int range = groundThreatRangePx(t);
    const int value = groundThreatValue(t);

    if (range > 0 && value > 0)
    {
        paintDisc(groundThreat_, s.pos, range, value * delta);
    }

    const int airRange = airThreatRangePx(t);
    const int airValue = airThreatValue(t);

    if (airRange > 0 && airValue > 0)
    {
        paintDisc(airThreat_, s.pos, airRange, airValue * delta);
    }
}
