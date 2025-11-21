#pragma once

#include <BWAPI.h>
#include "../../visualstudio/BWEB/Source/BWEB.h"

namespace Tools
{
    BWAPI::Unit GetClosestUnitTo(BWAPI::Position p, const BWAPI::Unitset& units);
    BWAPI::Unit GetClosestUnitTo(BWAPI::Unit unit, const BWAPI::Unitset& units);

    int CountUnitsOfType(BWAPI::UnitType type, const BWAPI::Unitset& units);
    int getVisibleCount(BWAPI::UnitType type, const BWAPI::Unitset& units);
    BWAPI::Unit GetUnitOfType(BWAPI::UnitType type);
    BWAPI::Unit GetDepot();

    bool BuildBuilding(BWAPI::UnitType type);
    bool BuildBuilding(BWAPI::Unit unit, BWAPI::UnitType type);
    bool BuildBuilding(BWAPI::Unit unit, BWAPI::UnitType type, BWAPI::TilePosition tile);

    void DrawUnitBoundingBoxes();
    void DrawUnitCommands();

    void SmartRightClick(BWAPI::Unit unit, BWAPI::Unit target);

    int GetTotalSupply(bool inProgress = false);

    void DrawUnitHealthBars();
    void DrawHealthBar(BWAPI::Unit unit, double ratio, BWAPI::Color color, int yOffset);
    int getVisibleCount(BWAPI::UnitType);
    int getCompleteCount(BWAPI::UnitType);
    int getTotalCount(BWAPI::UnitType);
    int getDeadCount(BWAPI::UnitType);
    bool ExpansionBuild(BWAPI::Unit unit);
    const BWEB::Station * const getMyNatural();

    void updateCount();

    /// Returns the self owned total unit count of this UnitType
    static int vis(BWAPI::UnitType t) {
        return Tools::getVisibleCount(t);
    }

    /// Returns the self owned completed unit count of this UnitType
    static int com(BWAPI::UnitType t) {
        return Tools::getCompleteCount(t);
    }

    /// Returns the self total unit count of this UnitType
    static int total(BWAPI::UnitType t) {
        return Tools::getTotalCount(t);
    }
}