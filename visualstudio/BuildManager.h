#pragma once
#include <vector>
#include <string>
#include <BWAPI.h>
#include <variant>
#include "SpenderManager.h"
#include "../src/starterbot/Tools.h"
#include "../visualstudio/BWEB/Source/BWEB.h"



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
    void onUnitCreate(BWAPI::Unit unit);
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitMorph(BWAPI::Unit);
    void onUnitDiscover(BWAPI::Unit);

    void assignBuilding(BWAPI::Unit unit);
    bool isBuildOrderCompleted();
    bool requestedBuilding(BWAPI::UnitType building);
    bool alreadySentRequest(int unitID);
    bool checkUnitIsBeingWarpedIn(BWAPI::UnitType building);
    bool checkUnitIsPlanned(BWAPI::UnitType building);
    void buildingDoneWarping(BWAPI::Unit unit);

    void buildBuilding(BWAPI::UnitType building);
    void trainUnit(BWAPI::UnitType unitToTrain, BWAPI::Unit building);
    void buildUpgadeType(BWAPI::Unit, BWAPI::UpgradeType);

    void getBuildOrder();
    void updateBuild();

    void PvP();
    void PvT();
    void PvZ();
    void PvT_2Gateway_Observer();
    void PvP_10_12_Gateway();
    void PvZ_10_12_Gateway();
    void runBuildQueue();
    void runUnitQueue();
    void pumpUnit();

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