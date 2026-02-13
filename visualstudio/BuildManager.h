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
#include "SpenderManager.h"

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

    // Optional: force a building to be placed at a specific TilePosition (eg. ramp pylon / wall pieces)
    BWAPI::TilePosition forcedTile = BWAPI::TilePositions::Invalid;
    bool useForcedTile = false;

    BWAPI::Unit scoutToPlaceBuilding = nullptr; //Used if a scout requests a gas steal
    bool isCheese = false;

    //For now buildings will request to make units but we should remove this later
    //The strategy manager should request certain units and upgrades and the build manager should find open buildings that can trian them.
    BWAPI::Unit requestedBuilding = nullptr;
    int priority;
};

/// Build Order data structures
enum class BuildStepType
{
    Build,
    ScoutWorker,
    SupplyRampNatural,   // place a supply building on the natural ramp using BWEB
    NaturalWall          // create/enqueue a natural wall using BWEB
};

enum class BuildTriggerType
{
    Immediately,
    AtSupply
};

struct BuildTrigger
{
    BuildTriggerType type = BuildTriggerType::Immediately;
    int supply = 0; // only used for AtSupply
};

struct BuildOrderStep
{
    BuildStepType stepType = BuildStepType::Build;
    BuildTrigger trigger;

    BWAPI::UnitType unit = BWAPI::UnitTypes::None; // for Build steps
};

struct BuildOrder
{
    // Note: user requested these fields to exist in the struct.
    int name = 0;
    int id = 0;

    // Extra metadata for usability
    BWAPI::Race vsRace = BWAPI::Races::Unknown;
    std::string debugName;

    std::vector<BuildOrderStep> steps;
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
    void buildBuilding(BWAPI::UnitType);
    void buildBuilding(BWAPI::UnitType, BWAPI::Unit scout);
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

    // Build Orders
    void initBuildOrdersOnStart();
    void selectRandomBuildOrder();
    void overrideBuildOrder(int buildOrderId); // clear remaining steps + switch
    void clearBuildOrderAndQueue(bool keepActiveBuilders = true);
    std::string getActiveBuildOrderName() const { return activeBuildOrderName; }

    void pumpUnit();

private:
    // Build order state
    std::vector<BuildOrder> buildOrders;
    const BuildOrder* activeBuildOrder = nullptr;
    size_t activeStepIndex = 0;
    std::string activeBuildOrderName;

    // Helpers
    bool isTriggerMet(const BuildTrigger& t) const;
    void processBuildOrderSteps();
    void enqueueStep(const BuildOrderStep& s);
    int countMyUnits(BWAPI::UnitType type) const;
    int countPlanned(BWAPI::UnitType type) const;
    BWAPI::TilePosition findSupplyRampBlockSlotMain() const;
    void enqueueSupplyRamp(BWAPI::UnitType supplyType);
    void enqueueNaturalWallProtoss();
    bool enqueueWallFromLayout(BWEB::Wall* wall, const std::vector<BWAPI::UnitType>& buildings);
};