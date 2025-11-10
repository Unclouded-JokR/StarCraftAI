#pragma once
#include <BWAPI.h>
#include <variant>
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

struct ResearchUpgradeRequest
{
    BWAPI::UpgradeType upgradeType;
    BWAPI::Unit building;
};

struct BuildRequest
{
    std::variant<TrainUnitRequest, BuildStructureRequest, ResearchUpgradeRequest> request;
};

class SpenderManager
{
public:
    std::vector<BuildRequest> buildRequests;
    ProtoBotCommander* commanderReference;
    std::vector<BWAPI::UnitType> plannedBuildings;
    std::vector<BWAPI::UnitType> plannedUnits;
    std::vector<int> requestIdentifiers; //list of the buildings that have already sent train commands, can also do this for upgrades.

    SpenderManager(ProtoBotCommander* commanderReference);

    //Request to construct building
    void addRequest(BWAPI::UnitType unitType);

    //Request to research upgrade
    void addRequest(BWAPI::UpgradeType upgradeToResearch);

    //Request to train unit
    void addRequest(BWAPI::UnitType unitToTrain, BWAPI::Unit buildingRequesting);

    //Checks to make sure we can afford the unit and have avalible supply
    bool canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas);

    int availableMinerals();
    int availableGas();
    int availableSupply();
    bool requestedBuilding(BWAPI::UnitType building);
    bool buildingAlreadyMadeRequest(int unitID);
    bool checkUnitIsPlanned(BWAPI::UnitType building);

    void OnFrame();
    void onUnitCreate(BWAPI::Unit unit);
    void printQueue();
    void removeRequestID(int unitID);
};
