#pragma once
#include <vector>
#include <string>
#include <BWAPI.h>
#include <fstream>

#include "../src/starterbot/MapTools.h"
#include "../src/starterbot/Tools.h"
#include "../visualstudio/BWEB/Source/BWEB.h"
#include "A-StarPathfinding.h"
#include "BuildingPlacer.h"
#include "Builder.h"

class SpenderManager;
class ProtoBotCommander;
class NexusEconomy;

class BuildManager
{
public:
    ProtoBotCommander* commanderReference;
    SpenderManager* spenderManager;
    BuildingPlacer buildingPlacer;
    std::vector<Builder> builders;

    bool buildOrderCompleted = false;

    BWAPI::Unitset buildings; //Completed Buildings
    BWAPI::Unitset incompleteBuildings; //Buildings in the process of being warped/built/formed, not yet completed.

    BuildManager(ProtoBotCommander* commanderReference);
    //~BuildManager();

    //BWAPI Events
    void onStart();
    void onFrame();
    void onUnitCreate(BWAPI::Unit);
    void onUnitDestroy(BWAPI::Unit);
    void onUnitMorph(BWAPI::Unit);
    void onUnitComplete(BWAPI::Unit);
    void onUnitDiscover(BWAPI::Unit);

    //Spender Manager Request methods
    void buildBuilding(BWAPI::UnitType);
    void trainUnit(BWAPI::UnitType, BWAPI::Unit);
    void buildUpgadeType(BWAPI::Unit, BWAPI::UpgradeType);
    bool alreadySentRequest(int unitID);
    bool requestedBuilding(BWAPI::UnitType);
    bool upgradeAlreadyRequested(BWAPI::Unit);
    bool checkUnitIsPlanned(BWAPI::UnitType);
    bool checkWorkerIsConstructing(BWAPI::Unit);
    int checkAvailableSupply();

    bool isBuildOrderCompleted();
    bool checkUnitIsBeingWarpedIn(BWAPI::UnitType building);
    void buildingDoneWarping(BWAPI::Unit unit);

    BWAPI::Unit getUnitToBuild(BWAPI::Position);
    std::vector<NexusEconomy> getNexusEconomies();

    //Builder helper methods
    std::vector<Builder> getBuilders();
    void createBuilder(BWAPI::Unit unit, BWAPI::UnitType building, BWAPI::Position positionToBuild);
    void pumpUnit();
};