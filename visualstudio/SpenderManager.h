#pragma once
#include <BWAPI.h>
#include <variant>
#include <limits.h>
#include "../src/starterbot/Tools.h"

struct TrainUnitRequest
{
    BWAPI::UnitType unitType;
    BWAPI::Unit building;
};

struct BuildStructureRequest
{
    BWAPI::UnitType buildingType;
};

struct UnitBuildStructureRequest
{
    BWAPI::Unit worker;
    BWAPI::UnitType buildingType;
    BWAPI::TilePosition locationToBuild;
};

struct ResearchUpgradeRequest
{
    BWAPI::UpgradeType upgradeType;
    BWAPI::Unit building;
};

struct BuildRequest
{
    std::variant<TrainUnitRequest, BuildStructureRequest, ResearchUpgradeRequest, UnitBuildStructureRequest> request;
};

class BuildManager;

class SpenderManager
{
public:
    BuildManager* buildManagerReference;
    std::vector<BuildRequest> buildRequests;

    std::vector<BWAPI::UnitType> plannedBuildings; //Building Requests that have been accepted and waiting for a unit to place.

    std::vector<BWAPI::UnitType> plannedUnits;
    std::vector<int> requestIdentifiers; //list of the buildings that have already sent train commands, can also do this for upgrades.

    SpenderManager(BuildManager*);

    //Requests
    void addRequest(BWAPI::UnitType);
    void addRequest(BWAPI::Unit, BWAPI::UpgradeType);
    void addRequest(BWAPI::UnitType, BWAPI::TilePosition, BWAPI::Unit);
    void addRequest(BWAPI::UnitType, BWAPI::Unit);

    //Checks to make sure we can afford the unit and have avalible supply
    bool canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas);
    int availableMinerals();
    int availableGas();
    int availableSupply();
    int plannedSupply();

    bool requestedBuilding(BWAPI::UnitType building);
    bool buildingAlreadyMadeRequest(int unitID);
    bool checkUnitIsPlanned(BWAPI::UnitType building);
    bool upgradeAlreadyRequested(BWAPI::Unit);
    void printQueue();
    void removeRequestID(int unitID);
    bool alreadyUsingTiles(BWAPI::TilePosition);

    //BWAPI Events
    void onStart();
    void OnFrame();
    void onUnitCreate(BWAPI::Unit);
    void onUnitDestroy(BWAPI::Unit);
    void onUnitComplete(BWAPI::Unit);
    void onUnitMorph(BWAPI::Unit);
    void onUnitDiscover(BWAPI::Unit);
};
