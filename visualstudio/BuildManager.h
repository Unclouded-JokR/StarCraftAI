#pragma once
#include <BWAPI.h>
#include <variant>
#include "SpenderManager.h"
#include "../src/starterbot/Tools.h"

class ProtoBotCommander;

struct BuildOrderInstruction
{
    int supply;
    BWAPI::UnitType buildingToConstruct = BWAPI::UnitTypes::None;
    BWAPI::UnitType unitToTrain = BWAPI::UnitTypes::None;
};

struct BuildOrder
{
    std::vector<BuildOrderInstruction> buildOrderInstructions;
};

class BuildManager
{
public:
    ProtoBotCommander* commanderReference;
    SpenderManager spenderManager;

    BuildOrder selectedBuildOrder;
    bool buildOrderCompleted = false;

    BWAPI::Unitset buildings;

    BuildManager(ProtoBotCommander* commanderReference);
    void onStart();
    void onFrame();
    void assignBuilding(BWAPI::Unit unit);
    bool isBuildOrderCompleted();
    bool alreadyBuildingSupply();
    void buildBuilding(BWAPI::UnitType building);
    void trainUnit(BWAPI::UnitType unitToTrain, BWAPI::Unit building);
    void buildUnitType(BWAPI::UnitType unit);
    void buildUpgadeType(BWAPI::UpgradeType upgradeToBuild);
};