#pragma once
#include <BWAPI.h>
#include <variant>
#include "../src/starterbot/Tools.h"

class ProtoBotCommander;

//Sould change this to be a smart reuqest but for now this works.
struct BuildRequest
{
    std::variant<BWAPI::UnitType, BWAPI::UpgradeType> unitRequest;
};

class SpenderManager
{
public:
    std::vector<BuildRequest> buildReuqests;
    ProtoBotCommander* commanderReference;

    SpenderManager(ProtoBotCommander* commanderReference);
    void addRequest(BWAPI::UnitType unitType);
    bool canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas);
    void OnFrame();
};

