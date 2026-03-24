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

//Units strategy manager cares about, details the units that are being requested and have not been placed yet.

class ProtoBotCommander;
class NexusEconomy;
struct ResourceRequest;

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
    void overrideBuildOrder(int buildOrderId);
    void clearBuildOrder(bool clearPendingRequests = true);

    // Placement helpers
    BWAPI::TilePosition findNaturalRampPlacement(BWAPI::UnitType type) const;
    bool enqueueSupplyAtNaturalRamp();
    bool enqueueNaturalWallAtChoke();
    bool requestNaturalWallBuild(bool resetPlan = false);
    BWAPI::TilePosition findNaturalChokePylonTile() const;
    void resetNaturalWallPlan();
    bool checkWorkerIsConstructing(BWAPI::Unit);
    bool isBuildOrderCompleted();
    bool checkUnitIsBeingWarpedIn(BWAPI::UnitType building);

    BWAPI::Unit getUnitToBuild(BWAPI::Position);
    std::vector<NexusEconomy> getNexusEconomies();

    void pumpUnit();
private:
    std::vector<BuildOrder> buildOrders;
    int activeBuildOrderIndex = -1;
    size_t activeBuildOrderStep = 0;
    bool buildOrderActive = false;

    // Natural wall planning cache
    bool naturalWallPlanned = false;
    bool naturalWallPylonEnqueued = false;
    bool naturalWallGatewaysEnqueued = false;
    bool naturalWallCannonsEnqueued = false;
    BWAPI::TilePosition naturalWallPylonTile = BWAPI::TilePositions::Invalid;
    std::vector<BWAPI::TilePosition> naturalWallPylonTiles;
    BWAPI::TilePosition naturalWallOpeningTile = BWAPI::TilePositions::Invalid;
    std::vector<BWAPI::TilePosition> naturalWallGapTiles;
    BWAPI::TilePosition naturalWallChokeAnchorTile = BWAPI::TilePositions::Invalid;
    std::vector<BWAPI::TilePosition> naturalWallForgeTiles;
    std::vector<BWAPI::TilePosition> naturalWallGatewayTiles;
    std::vector<BWAPI::TilePosition> naturalWallCannonTiles;
    std::vector<BWAPI::TilePosition> naturalWallPathTiles;
    bool naturalWallStartLogged = false;

    bool isRestrictedTechBuilding(BWAPI::UnitType type) const;
    std::string buildOrderNameToString(int name) const;
    bool enqueueBuildOrderBuilding(BWAPI::UnitType type, int count);

};