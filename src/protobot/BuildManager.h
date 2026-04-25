#pragma once
#include <vector>
#include <algorithm>
#include <climits>
#include <string>
#include <BWAPI.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cmath>
#include <cstdlib>

#include "../starterbot/MapTools.h"
#include "../starterbot/Tools.h"
#include "BWEB/Source/BWEB.h"
#include "A-StarPathfinding.h"
#include "BuildingPlacer.h"
#include "Builder.h"
#include "BuildOrder.h"

#define FRAMES_BEFORE_TRYAGAIN 72
#define MAX_ATTEMPTS 3
#define LARGEST_GYSER_DIATNCE_TO_NEXUS 300

class ProtoBotCommander;
class NexusEconomy;
struct ResourceRequest;

/// <summary>
/// The BuildManagers responsibility is to be able to service the ResourceRequests passed to it and verify a unit is able to create the upgrade or combat unit. In the case of buildings more work is required to be able to find suitable locations to make sure that building can be constructed.\n
/// The class is also responsible for defining and executing the openings for each race that StarCraft has, allowing ProtoBot to have openers similar to chess at the beginning stages of a StarCraft game.
/// </summary>
class BuildManager
{
public:
    ProtoBotCommander* commanderReference;
    BuildingPlacer buildingPlacer;

    std::vector<Builder> builders;

    bool buildOrderCompleted = false;

    BWAPI::Unitset buildings; //Complete and Incomplete Buildings

    BuildManager(ProtoBotCommander* commanderReference);
    //~BuildManager();

    //BWAPI Events
    void onStart();
    void onFrame(std::vector<ResourceRequest> &resourceRequests);
    void onUnitCreate(BWAPI::Unit);
    void onUnitDestroy(BWAPI::Unit);
    void onUnitMorph(BWAPI::Unit);
    void onUnitComplete(BWAPI::Unit);
    void onUnitDiscover(BWAPI::Unit);

    BWAPI::Unitset getBuildings();

    // Build order lifecycle
    void initBuildOrdersOnStart();
    void selectBuildOrderAgainstRace(BWAPI::Race enemyRace);
    void selectRandomBuildOrder();

    void runBuildOrderOnFrame();
    bool isBuildOrderActive() const;

    // Placement helpers
    bool checkWorkerIsConstructing(BWAPI::Unit);
    bool isBuildOrderCompleted();
    bool checkUnitIsBeingWarpedIn(BWAPI::UnitType building, const BWEM::Base* nexus = nullptr);

    BWAPI::Unit getUnitToBuild(BWAPI::Position);
    std::vector<NexusEconomy> getNexusEconomies();

private:
    std::vector<BuildOrder> buildOrders;
    int activeBuildOrderIndex = -1;
    size_t activeBuildOrderStep = 0;
    bool buildOrderActive = false;

    std::string buildOrderNameToString(int name) const;
    bool enqueueBuildOrderBuilding(BWAPI::UnitType type, int count);
    bool enqueueBuildOrderUnit(BWAPI::UnitType type, int count);

};