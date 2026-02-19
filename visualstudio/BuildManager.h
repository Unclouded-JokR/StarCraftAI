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
#include "BuildOrder.h"
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
    // Build order / placement helpers
    bool fromBuildOrder = false;
    BWAPI::TilePosition forcedTile = BWAPI::TilePositions::Invalid;
    bool useForcedTile = false;

    int priority;
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
    // External requests from StrategyManager / commander
    void buildBuilding(BWAPI::UnitType);
    void buildBuilding(BWAPI::UnitType, BWAPI::Unit scout);

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
    BWAPI::TilePosition findNaturalChokePylonTile() const;
    void resetNaturalWallPlan();
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
private:
    std::vector<BuildOrder> buildOrders;
    int activeBuildOrderIndex = -1;
    size_t activeBuildOrderStep = 0;
    bool buildOrderActive = false;

    // Natural wall planning cache
    bool naturalWallPlanned = false;
    bool naturalWallPylonEnqueued = false;
    bool naturalWallGatewaysEnqueued = false;
    BWAPI::TilePosition naturalWallPylonTile = BWAPI::TilePositions::Invalid;
    std::vector<BWAPI::TilePosition> naturalWallGatewayTiles;

    bool isRestrictedTechBuilding(BWAPI::UnitType type) const;
    std::string buildOrderNameToString(int name) const;
    bool enqueueBuildOrderBuilding(BWAPI::UnitType type, int count);

};