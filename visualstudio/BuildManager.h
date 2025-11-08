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

    bool buildOrderCompleted = false;
    BWAPI::Unitset buildings;
    BWAPI::Unitset buildingWarps;

    BuildManager(ProtoBotCommander* commanderReference);
    void onStart();
    void onFrame();
    void onCreate(BWAPI::Unit unit);
    void onUnitDestroy(BWAPI::Unit unit);

    void assignBuilding(BWAPI::Unit unit);
    bool isBuildOrderCompleted();
    bool requestedBuilding(BWAPI::UnitType building);
    bool alreadySentRequest(int unitID);
    bool checkUnitIsBeingWarpedIn(BWAPI::UnitType building);
    bool checkUnitIsPlanned(BWAPI::UnitType building);
    void buildingDoneWarping(BWAPI::Unit unit);


    void buildBuilding(BWAPI::UnitType building);
    void trainUnit(BWAPI::UnitType unitToTrain, BWAPI::Unit building);
    void buildUnitType(BWAPI::UnitType unit);
    void buildUpgadeType(BWAPI::UpgradeType upgradeToBuild);
};