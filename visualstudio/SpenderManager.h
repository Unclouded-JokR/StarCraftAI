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

    SpenderManager(ProtoBotCommander* commanderReference);
    void addRequest(BWAPI::UnitType unitType);
    void addRequest(BWAPI::UpgradeType upgradeToResearch);
    void addRequest(BWAPI::UnitType unitToTrain, BWAPI::Unit buildingRequesting);
    bool canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas);
    bool isAlreadyBuildingSupply();
    void OnFrame();
};

