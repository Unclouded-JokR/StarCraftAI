#pragma once
#include <BWAPI.h>
#include <variant>
#include <limits.h>
#include "../src/starterbot/Tools.h"

struct ResourceRequest;

class SpenderManager
{
public:
    SpenderManager();

    //Checks to make sure we can afford the unit and have avalible supply
    bool canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas);
    int availableMinerals(std::vector<ResourceRequest>&);
    int availableGas(std::vector<ResourceRequest>&);
    int plannedSupply(std::vector<ResourceRequest>&, BWAPI::Unitset);
    int availableSupply();

    void printQueue();

    //BWAPI Events
    void onStart();
    void OnFrame(std::vector<ResourceRequest>&);
};
