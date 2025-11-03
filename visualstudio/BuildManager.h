#pragma once
#include <BWAPI.h>
#include <variant>
#include "SpenderManager.h"
#include "../src/starterbot/Tools.h"

class ProtoBotCommander;

class BuildManager
{
public:
    ProtoBotCommander* commanderReference;
    SpenderManager spenderManager;
    BWAPI::Unitset buildings;
    bool buildOrderCompleted = false;

    BuildManager(ProtoBotCommander* commanderReference);
    void onStart();
    void onFrame();
    void assignBuilding(BWAPI::Unit unit);
    bool isBuildOrderCompleted();
    void buildBuilding(BWAPI::Unit unit, BWAPI::UnitType building);
    void buildUnitType(BWAPI::UnitType unit);
    void buildUpgadeType(BWAPI::UpgradeType upgradeToBuild);
};