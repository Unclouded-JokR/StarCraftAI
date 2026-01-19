#pragma once
#include <BWAPI.h>
#include <variant>
#include <limits.h>
#include "../src/starterbot/Tools.h"

class ProtoBotCommander;

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

//Keep track of units that have been requested to build.
struct Builder
{
    BWAPI::Unit probe;
    BWAPI::UnitType building;
    BWAPI::Position positionToBuild;
};

class SpenderManager
{
public:
    std::vector<BuildRequest> buildRequests;
    ProtoBotCommander* commanderReference;
    std::vector<BWAPI::UnitType> plannedBuildings; //Building Requests that have been accepted and waiting for a unit to place.
    BWAPI::Unitset incompleteBuildings; //Buildings in the process of being warped/built/formed, not yet completed.
    std::vector<BWAPI::UnitType> plannedUnits;
    std::vector<int> requestIdentifiers; //list of the buildings that have already sent train commands, can also do this for upgrades.
    std::vector<Builder> builders;

    SpenderManager(ProtoBotCommander* commanderReference);

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
    bool checkWorkerIsConstructing(BWAPI::Unit);
    bool upgradeAlreadyRequested(BWAPI::Unit);
    BWAPI::Position getPositionToBuild(BWAPI::UnitType type);
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
