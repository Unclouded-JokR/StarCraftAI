#pragma once
#include <BWAPI.h>
#include <variant>
#include <limits.h>
#include "../starterbot/Tools.h"

struct ResourceRequest;

//Do we need this?
struct ProtoBotRequestCounter {
    int gateway_requests = 0;
    int nexus_requests = 0;
    int forge_requests = 0;
    int cybernetics_requests = 0;
    int robotics_requests = 0;
    int observatory_requests = 0;
    int citadel_requests = 0;
    int templarArchives_requests = 0;
};


class SpenderManager
{
public:
    SpenderManager();

    //Checks to make sure we can afford the unit and have avalible supply
    bool canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas);
    int availableMinerals(std::vector<ResourceRequest>&);
    int availableGas(std::vector<ResourceRequest>&);
    int getPlannedMinerals(std::vector<ResourceRequest>&);
    int getPlannedGas(std::vector<ResourceRequest>&);
    int plannedSupply(std::vector<ResourceRequest>&, BWAPI::Unitset);
    int availableSupply();

    void printQueue();

    //BWAPI Events
    void onStart();
    void OnFrame(std::vector<ResourceRequest>&);
};
