#pragma once
#include <vector>
#include <string>
#include <BWAPI.h>
#include <fstream>

#include "../src/starterbot/MapTools.h"
#include "../src/starterbot/Tools.h"
#include "../visualstudio/BWEB/Source/BWEB.h"
#include "Builder.h"

class SpenderManager;
class BuildingPlacer;
class ProtoBotCommander;
class NexusEconomy;

class BuildManager
{
public:
    ProtoBotCommander* commanderReference;
    SpenderManager* spenderManager;
    BuildingPlacer* buildingPlacer;
    std::vector<Builder> builders;

    bool buildOrderCompleted = false;

    BWAPI::Unitset buildings;
    BWAPI::Unitset buildingWarps;

    BuildManager(ProtoBotCommander* commanderReference);

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
    void getBuildOrder();
    void updateBuild();

    BWAPI::Unit getUnitToBuild(BWAPI::Position);
    std::vector<NexusEconomy> getNexusEconomies();

    //Builder helper methods
    std::vector<Builder> getBuilders();
    

    void PvP();
    void PvT();
    void PvZ();
    void PvT_2Gateway_Observer();
    void PvP_10_12_Gateway();
    void PvZ_10_12_Gateway();
    void runBuildQueue();
    void runUnitQueue();
    void pumpUnit();
    void createBuilder(BWAPI::Unit, BWAPI::UnitType, BWAPI::Position);

    bool zealotUnitPump = false;
    std::map<BWAPI::UnitType, int>& getBuildQueue();
    std::map<BWAPI::UnitType, int>& getUnitQueue();
    using BuildList = void (BuildManager::*)();
    std::vector<BuildList> getBuildOrders(BWAPI::Race race);
};

namespace All {
    inline std::map <BWAPI::UnitType, int> buildQueue;
    inline std::map <BWAPI::UnitType, int> unitQueue;
    inline std::map <BWAPI::TechType, int> techQueue;

    inline std::string currentBuild = "";
    inline std::string currentOpener = "";
    inline std::string currentTransition = "";

}