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
#include "SpenderManager.h"

#define FRAMES_BEFORE_TRYAGAIN 72
#define MAX_ATTEMPTS 3

class ProtoBotCommander;
class NexusEconomy;

struct ResourceRequest
{
    enum Type {Unit, Building, Upgrade, Tech};
    Type type;

    //Approved_InProgress only applies to Buildings since this requires a unit to take the time to place it.
    enum State {Accepted_Completed, Approved_BeingBuilt, Approved_InProgress, PendingApproval};
    State state = PendingApproval;

    BWAPI::UnitType unit = BWAPI::UnitTypes::None;
    BWAPI::UpgradeType upgrade = BWAPI::UpgradeTypes::None;
    BWAPI::TechType tech = BWAPI::TechTypes::None;

    BWAPI::Unit scoutToPlaceBuilding = nullptr; //Used if a scout requests a gas steal
    bool isCheese = false;

    //For now buildings will request to make units but we should remove this later
    //The strategy manager should request certain units and upgrades and the build manager should find open buildings that can trian them.
    BWAPI::Unit requestedBuilding = nullptr;

    //Use this to try requests again and see if we need to kill it.
    int framesSinceLastCheck = 0;
    int attempts = 0;
};

class BuildManager
{
public:
    ProtoBotCommander* commanderReference;
    SpenderManager spenderManager;
    BuildingPlacer buildingPlacer;

    std::vector<Builder> builders;
    std::vector<ResourceRequest> resourceRequests;

    bool buildOrderCompleted = false;

    BWAPI::Unitset buildings; //Complete and Incomplete Buildings

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
    void buildBuilding(BWAPI::UnitType, BWAPI::Unit scout);
    void trainUnit(BWAPI::UnitType, BWAPI::Unit);
    void buildUpgadeType(BWAPI::Unit, BWAPI::UpgradeType);

    bool cheeseIsApproved(BWAPI::Unit scout);
    bool alreadySentRequest(int unitID);
    bool requestedBuilding(BWAPI::UnitType);
    bool upgradeAlreadyRequested(BWAPI::Unit);
    bool checkUnitIsPlanned(BWAPI::UnitType);
    bool checkWorkerIsConstructing(BWAPI::Unit);
    int checkAvailableSupply();

    bool isBuildOrderCompleted();
    bool checkUnitIsBeingWarpedIn(BWAPI::UnitType building);

    BWAPI::Unit getUnitToBuild(BWAPI::Position);
    std::vector<NexusEconomy> getNexusEconomies();

    void pumpUnit();
};