#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "SpenderManager.h"
#include "BuildingPlacer.h"
#include "Builder.h"
#include <cmath>
#include <random>


// Returns true if tile is valid, in bounds, and BWAPI considers it buildable for the given type.
static bool isValidBuildTile(BWAPI::UnitType type, const BWAPI::TilePosition& t)
{
    if (!t.isValid()) return false;

    const int w = type.tileWidth();
    const int h = type.tileHeight();

    if (t.x < 0 || t.y < 0) return false;
    if ((t.x + w) > BWAPI::Broodwar->mapWidth())  return false;
    if ((t.y + h) > BWAPI::Broodwar->mapHeight()) return false;

    // Strong guard: prevents (0,0) / unreachable “placements” from breaking BO.
    if (!BWAPI::Broodwar->canBuildHere(t, type)) return false;

    return true;
}

// Returns true if the footprint is within bounds and all underlying tiles are buildable terrain.
// This intentionally ignores Protoss power (so we can plan walls before the pylon is finished).
static bool isTerrainBuildable(BWAPI::UnitType type, const BWAPI::TilePosition& t)
{
    if (!t.isValid()) return false;
    const int w = type.tileWidth();
    const int h = type.tileHeight();
    if (t.x < 0 || t.y < 0) return false;
    if ((t.x + w) > BWAPI::Broodwar->mapWidth())  return false;
    if ((t.y + h) > BWAPI::Broodwar->mapHeight()) return false;

    for (int dx = 0; dx < w; dx++) {
        for (int dy = 0; dy < h; dy++) {
            BWAPI::TilePosition tt(t.x + dx, t.y + dy);
            if (!BWAPI::Broodwar->isBuildable(tt))
                return false;
        }
    }
    return true;
}


BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

//BuildManager::~BuildManager()
//{
//    delete commanderReference;
//    delete spenderManager;
//    delete buildingPlacer;
//
//    builders.clear();
//    buildings.clear();
//    incompleteBuildings.clear();
//}


#pragma region BWAPI EVENTS
void BuildManager::onStart()
{
    std::cout << "Builder Manager Initialized" << "\n";
    // Reset per-game state
    buildOrders.clear();
    activeBuildOrderIndex = -1;
    activeBuildOrderStep = 0;
    //Make false at the start of a game.
    buildOrderActive = false;
    buildOrderCompleted = false;

    resetNaturalWallPlan();

    spenderManager.onStart();
    buildingPlacer.onStart();
    builders.clear();

    initBuildOrdersOnStart();
    selectRandomBuildOrder();
}


void BuildManager::onFrame() {
    for (std::vector<ResourceRequest>::iterator it = resourceRequests.begin(); it != resourceRequests.end();)
    {
        if (it->state == ResourceRequest::State::Accepted_Completed || it->attempts == MAX_ATTEMPTS)
        {
            if (it->state == ResourceRequest::State::Accepted_Completed) std::cout << "Completed Request\n";
            if (it->attempts == MAX_ATTEMPTS) std::cout << "Killing request to build " << it->unit << "\n";

            it = resourceRequests.erase(it);
        }
        else
        {
            it++;
        }
    }

    // Execute build order steps (adds requests gradually, try to avoid expensive queue building)
    runBuildOrderOnFrame();

    spenderManager.OnFrame(resourceRequests);
    //buildingPlacer.drawPoweredTiles();

    for (ResourceRequest& request : resourceRequests)
    {
        if (request.state != ResourceRequest::State::Approved_InProgress)
        {
            continue;
        }

        switch (request.type)
        {
            case ResourceRequest::Type::Unit:
            {
                if (request.requestedBuilding->canTrain(request.unit) &&
                    !request.requestedBuilding->isTraining() &&
                    request.requestedBuilding->isCompleted() &&
                    request.requestedBuilding->isPowered())
                {
                    request.requestedBuilding->train(request.unit);
                    request.state = ResourceRequest::State::Accepted_Completed;
                }

                break;
            }
            case ResourceRequest::Type::Building:
            {
                //Should change this to consider distance measure but is fine for now.
                if (request.isCheese)
                {
                    request.state = ResourceRequest::State::Approved_BeingBuilt;
                }
                else
                {  
                    const PlacementInfo placementInfo = buildingPlacer.getPositionToBuild(request.unit);

                    if (placementInfo.position == BWAPI::Positions::Invalid)
                    {
                        const PlacementInfo::PlacementFlag flag_info = placementInfo.flag;

                        switch (flag_info)
                        {
                        case PlacementInfo::NO_POWER:
                            //should create a new pylon request
                            std::cout << "FAILED: NO POWER\n";
                            break;
                        case PlacementInfo::NO_BLOCKS:
                            //Wait for a bit and kill the request if no blocks are added
                            std::cout << "FAILED: NO BLOCKS\n";
                            break;
                        case PlacementInfo::NO_GYSERS:
                            //Shouldnt happen but okay
                            std::cout << "FAILED: NO GYSERS\n";
                            break;
                        case PlacementInfo::NO_PLACEMENTS:
                            //Wait for a bit and kill the request if no placements are added.
                            std::cout << "FAILED: NO PLACEMENTS\n";
                            break;
                        case PlacementInfo::NO_EXPANSIONS:
                            //kill the expansion request.
                            std::cout << "FAILED: NO EXPANSION\n";
                            break;
                        }

                        request.attempts++;

                        continue;
                    }

                    const BWAPI::Unit workerAvalible = getUnitToBuild(placementInfo.position);

                    if (workerAvalible == nullptr) continue;


                    Path pathToLocation;
                    if (request.unit.isResourceDepot())
                    {
                        std::cout << "Trying to build Nexus\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), placementInfo.position);
                    }
                    else if(request.unit.isRefinery())
                    {
                        //std::cout << "Trying to build assimlator\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), placementInfo.position, true);
                    }
                    else
                    {
                        //std::cout << "Trying to build regular building\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), placementInfo.position);
                    }

                    Builder temp = Builder(workerAvalible, request.unit, placementInfo.position, pathToLocation);
                    builders.push_back(temp);

                    request.state = ResourceRequest::State::Approved_BeingBuilt;
                }
                break;
            }
            case ResourceRequest::Type::Upgrade:
            {
                if (request.requestedBuilding->canUpgrade(request.upgrade) &&
                    !request.requestedBuilding->isUpgrading() &&
                    request.requestedBuilding->isCompleted() &&
                    request.requestedBuilding->isPowered())
                {
                    request.requestedBuilding->upgrade(request.upgrade);
                    request.state = ResourceRequest::State::Accepted_Completed;
                }
                break;
            }
            case ResourceRequest::Type::Tech:
            {
                if (request.requestedBuilding->canResearch(request.upgrade) &&
                    !request.requestedBuilding->isResearching() &&
                    request.requestedBuilding->isCompleted() &&
                    request.requestedBuilding->isPowered())
                {
                    request.requestedBuilding->upgrade(request.upgrade);
                    request.state = ResourceRequest::State::Accepted_Completed;
                }
                break;
            }
        }
    }


    //build order check here

    for (Builder& builder : builders)
    {
        builder.onFrame();
    }

    //Debug
    //Will most likely need to add a building data class to make this easier to be able to keep track of buildings and what units they are creating.
    /*for (BWAPI::Unit building : buildings)
    {
        BWAPI::Broodwar->drawTextMap(building->getPosition(), std::to_string(building->getID()).c_str());
    }*/

    pumpUnit();

    ////Might need to add filter on units, economy buildings, and pylons having the "Warpping Building" text.
    //for (BWAPI::Unit building : buildingWarps)
    //{
    //    BWAPI::Broodwar->drawTextMap(building->getPosition(), "Warpping Building");
    //}
}

void BuildManager::onUnitCreate(BWAPI::Unit unit)
{
    if (unit == nullptr) return;

    buildingPlacer.onUnitCreate(unit);

    //Need to check this for tech and upgrades;
    for (ResourceRequest& request : resourceRequests)
    {
        if (request.state == ResourceRequest::State::Approved_BeingBuilt &&
            request.unit == unit->getType())
        {
            request.state = ResourceRequest::State::Accepted_Completed;
        }
        else if (request.state == ResourceRequest::State::Approved_BeingBuilt &&
            request.unit == unit->getType())
        {
            request.state = ResourceRequest::State::Accepted_Completed;
        }
    }

    //Remove worker once a building is being warped in.
    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
    {
        if (unit->getType() == it->buildingToConstruct)
        {
            it = builders.erase(it);
            break;
        }
    }

    if (unit->getType().isBuilding() && !unit->isCompleted()) buildings.insert(unit);
}


void BuildManager::onUnitDestroy(BWAPI::Unit unit)
{
    buildingPlacer.onUnitDestroy(unit);

    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end();)
    {
        if (it->getUnitReference()->getID() == unit->getID())
        {
            const BWAPI::Unit unitAvalible = getUnitToBuild(it->requestedPositionToBuild);
            if (unitAvalible != nullptr)
            {
                it->setUnitReference(unitAvalible);
                break;
            }
        }

        it++;
    }

    if (unit->getPlayer() != BWAPI::Broodwar->self())
        return;

    BWAPI::UnitType unitType = unit->getType();

    if (!unitType.isBuilding()) return;

    //Check if a building has been killed
    for (const BWAPI::Unit building : buildings)
    {
        if (building == unit)
        {
            buildings.erase(unit);
            return;
        }
    }
}

void BuildManager::onUnitMorph(BWAPI::Unit unit)
{
    buildingPlacer.onUnitMorph(unit);

    std::cout << "Created " << unit->getType() << " (On Morph)\n";

    //Need to check this for tech and upgrades;
    for (ResourceRequest& request : resourceRequests)
    {
        if (request.state == ResourceRequest::State::Approved_BeingBuilt &&
            request.unit == unit->getType())
        {
            request.state = ResourceRequest::State::Accepted_Completed;
        }
        else if (request.state == ResourceRequest::State::Approved_BeingBuilt &&
            request.unit == unit->getType())
        {
            request.state = ResourceRequest::State::Accepted_Completed;
        }
    }

    if (unit->getType() == BWAPI::UnitTypes::Protoss_Assimilator && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
        {
            if (unit->getType() == it->buildingToConstruct)
            {
                it = builders.erase(it);
                break;
            }
        }

        buildings.insert(unit);
    }
}

void BuildManager::onUnitComplete(BWAPI::Unit unit)
{
    buildingPlacer.onUnitComplete(unit);
}

void BuildManager::onUnitDiscover(BWAPI::Unit unit)
{
    buildingPlacer.onUnitDiscover(unit);
}
#pragma endregion

#pragma region Spender Manager Methods
/// <summary>
/// Using these methods for now to get this working but it should be refactored later.
/// </summary>
/// <param name="building"></param>
void BuildManager::buildBuilding(BWAPI::UnitType building)
{
    
    if (isBuildOrderActive() && isRestrictedTechBuilding(building))
    {
        return;
    }

    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = building;
    request.fromBuildOrder = false;

    resourceRequests.push_back(request);
}

void BuildManager::buildBuilding(BWAPI::UnitType building, BWAPI::Unit scout)
{

    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = building;
    request.scoutToPlaceBuilding = scout;
    request.isCheese = true;
    request.fromBuildOrder = false;

    resourceRequests.push_back(request);
}

void BuildManager::trainUnit(BWAPI::UnitType unitToTrain, BWAPI::Unit unit)
{
    ResourceRequest request;
    request.type = ResourceRequest::Type::Unit;
    request.unit = unitToTrain;
    request.requestedBuilding = unit;

    resourceRequests.push_back(request);
}

void BuildManager::buildUpgadeType(BWAPI::Unit unit, BWAPI::UpgradeType upgrade)
{
    ResourceRequest request;
    request.type = ResourceRequest::Type::Upgrade;
    request.upgrade = upgrade;
    request.requestedBuilding = unit;

    resourceRequests.push_back(request);
}

bool BuildManager::alreadySentRequest(int unitID)
{
    for (const ResourceRequest& request : resourceRequests)
    {
        if (request.requestedBuilding != nullptr)
        {
            if (unitID == request.requestedBuilding->getID()) return true;
        }
    }
    return false;
}

bool BuildManager::requestedBuilding(BWAPI::UnitType building)
{
    for (const ResourceRequest& request : resourceRequests)
    {
        if (building == request.unit && !request.isCheese) return true;
    }
    return false;
}

bool BuildManager::upgradeAlreadyRequested(BWAPI::Unit building)
{
    for (const ResourceRequest& request : resourceRequests)
    {
        if (request.requestedBuilding != nullptr)
        {
            if (building->getID() == request.requestedBuilding->getID()) return true;
        }
    }
    return false;
}

bool BuildManager::checkUnitIsPlanned(BWAPI::UnitType building)
{
    for (const ResourceRequest& request : resourceRequests)
    {
        if (building == request.unit && request.state == ResourceRequest::State::Approved_InProgress && !request.isCheese) return true;
    }
    return false;
}

bool BuildManager::checkWorkerIsConstructing(BWAPI::Unit unit)
{
    for (Builder& builder : builders)
    {
        if (builder.getUnitReference()->getID() == unit->getID()) return true;
    }

    return false;
}

int BuildManager::checkAvailableSupply()
{
    return spenderManager.plannedSupply(resourceRequests, buildings);
}
#pragma endregion

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
}

bool BuildManager::checkUnitIsBeingWarpedIn(BWAPI::UnitType unit)
{
    for (BWAPI::Unit building : buildings)
    {
        if (unit == building->getType() && !building->isCompleted())
        {
            return true;
        }
    }

    return false;
}

bool BuildManager::cheeseIsApproved(BWAPI::Unit unit)
{
    for (ResourceRequest& request : resourceRequests)
    {
        if (request.type != ResourceRequest::Type::Building && request.isCheese) continue;
        
        if (request.scoutToPlaceBuilding == unit && request.state == ResourceRequest::State::Approved_BeingBuilt) return true;
    }

    return false;
}

void BuildManager::pumpUnit()
{
    FriendlyUnitCounter ProtoBot_Units = commanderReference->informationManager.getFriendlyUnitCounter();
    FriendlyBuildingCounter ProtoBot_Buildings = commanderReference->informationManager.getFriendlyBuildingCounter();
    FriendlyUpgradeCounter ProtoBot_Upgrades = commanderReference->informationManager.getFriendlyUpgradeCounter();
    const int totalMinerals = BWAPI::Broodwar->self()->minerals();
    const int totalGas = BWAPI::Broodwar->self()->gas();

    for (BWAPI::Unit unit : buildings)
    {
        BWAPI::UnitType type = unit->getType();
        if (type == BWAPI::UnitTypes::Protoss_Gateway && !unit->isTraining() && !alreadySentRequest(unit->getID()))
        {
            if (ProtoBot_Buildings.cyberneticsCore >= 1)
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Dragoon, unit);
            }
            else
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Zealot, unit);
            }
        }
        else if (unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility && !unit->isTraining() && !alreadySentRequest(unit->getID()) && unit->canTrain(BWAPI::UnitTypes::Protoss_Observer))
        {
            if (ProtoBot_Units.observer < 4)
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Observer, unit);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Cybernetics_Core && !unit->isUpgrading() && totalMinerals >= 500)
        {
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Singularity_Charge) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Singularity_Charge);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun && !unit->isUpgrading())
        {
            /*if (unit->canUpgrade(BWAPI::UpgradeTypes::Leg_Enhancements) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Leg_Enhancements);
            }*/

        }
        else if (type == BWAPI::UnitTypes::Protoss_Forge && !unit->isUpgrading() && totalMinerals >= 500)
        {
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Armor);
            }
            else if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
            }
            else if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Plasma_Shields) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Templar_Archives && !unit->isUpgrading())
        {
            /*if (unit->canUpgrade(BWAPI::TechTypes::Psionic_Storm))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Armor);
                continue;
            }

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
                continue;
            }

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Plasma_Shields))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
                continue;
            }*/
        }
        else if (type == BWAPI::UnitTypes::Protoss_Observatory)
        {
            /*if (unit->canUpgrade(BWAPI::UpgradeTypes::Sensor_Array) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Sensor_Array);
            }
            else if (unit->canUpgrade(BWAPI::UpgradeTypes::Gravitic_Boosters) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Gravitic_Boosters);
            }*/
        }
    }
}

BWAPI::Unit BuildManager::getUnitToBuild(BWAPI::Position position)
{
    return commanderReference->getUnitToBuild(position);
}

std::vector<NexusEconomy> BuildManager::getNexusEconomies()
{
    return commanderReference->getNexusEconomies();
}


// ---------------------------
// Build order helpers / runner
// ---------------------------

bool BuildManager::isBuildOrderActive() const
{
    return buildOrderActive && !buildOrderCompleted && activeBuildOrderIndex >= 0 && activeBuildOrderIndex < (int)buildOrders.size();
}

bool BuildManager::isRestrictedTechBuilding(BWAPI::UnitType type) const
{
    // Hack: During an active build order, suspend most tech/production buildings calls outside the build order until build order is completed.
    if (type == BWAPI::UnitTypes::Protoss_Pylon) return false;
    if (type == BWAPI::UnitTypes::Protoss_Nexus) return false;
    if (type == BWAPI::UnitTypes::Protoss_Assimilator) return true;
    if (type == BWAPI::UnitTypes::Protoss_Gateway) return true;
    if (type == BWAPI::UnitTypes::Protoss_Forge) return true;
    if (type == BWAPI::UnitTypes::Protoss_Cybernetics_Core) return true;
    if (type == BWAPI::UnitTypes::Protoss_Robotics_Facility) return true;
    if (type == BWAPI::UnitTypes::Protoss_Observatory) return true;
    if (type == BWAPI::UnitTypes::Protoss_Stargate) return true;
    return false;
}

std::string BuildManager::buildOrderNameToString(int name) const
{
    switch (name)
    {
        case 1: return "2 Gateway Observer";
        case 2: return "3 Gate Robo";
        case 3: return "Corsair/Dragoon";
        default: return "Unknown";
    }
}

void BuildManager::initBuildOrdersOnStart()
{
    buildOrders.clear();

    // 1) 2 Gateway Observer vs Terran
    {
        BuildOrder bo;
        bo.name = 1;
        bo.id = 1;
        bo.vsRace = BWAPI::Races::Terran;

        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
        bo.steps.push_back({ BuildStepType::ScoutWorker,       BWAPI::UnitTypes::None,        1, {BuildTriggerType::AtSupply, 8} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 12} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 14} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 15} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 16} }); // 2nd gate
        bo.steps.push_back({ BuildStepType::SupplyRampNatural,             BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 21} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Robotics_Facility, 1, {BuildTriggerType::AtSupply, 25} });
        bo.steps.push_back({ BuildStepType::NaturalWall,             BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 27} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 29} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 31} });
        bo.steps.push_back({ BuildStepType::Build,             BWAPI::UnitTypes::Protoss_Observatory, 1, {BuildTriggerType::AtSupply, 38} });

        buildOrders.push_back(bo);
    }

    // 2) 3 Gate Robo vs Protoss
    {
        BuildOrder bo;
        bo.name = 2;
        bo.id = 2;
        bo.vsRace = BWAPI::Races::Protoss;

        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
        bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 9} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 12} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 14} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 16} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 20} }); // 2nd gate
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Robotics_Facility, 1, {BuildTriggerType::AtSupply, 24} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 26} }); // 3rd gate
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 30} });
        buildOrders.push_back(bo);
    }

    // 3) Corsair/Dragoon vs Zerg
    {
        BuildOrder bo;
        bo.name = 3;
        bo.id = 3;
        bo.vsRace = BWAPI::Races::Zerg;

        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
        bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 9} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 12} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 14} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 15} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Stargate, 1, {BuildTriggerType::AtSupply, 18} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 21} });
        bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 25} }); // add gates for dragoon follow-up
        buildOrders.push_back(bo);
    }
}

void BuildManager::selectBuildOrderAgainstRace(BWAPI::Race enemyRace)
{
    if (buildOrders.empty())
    {
        buildOrderActive = false;
        buildOrderCompleted = true;
        return;
    }

    std::vector<int> candidates;

    if (enemyRace != BWAPI::Races::Unknown)
    {
        for (int i = 0; i < (int)buildOrders.size(); i++)
        {
            if (buildOrders[i].vsRace == enemyRace)
                candidates.push_back(i);
        }
    }

    if (candidates.empty())
    {
        for (int i = 0; i < (int)buildOrders.size(); i++)
            candidates.push_back(i);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, (int)candidates.size() - 1);

    activeBuildOrderIndex = candidates[dis(gen)];
    activeBuildOrderStep = 0;
    buildOrderActive = true;
    buildOrderCompleted = false;

    std::cout << "Selected Build Order: " << buildOrderNameToString(buildOrders[activeBuildOrderIndex].name) << "\n";
}

void BuildManager::selectRandomBuildOrder()
{
    const auto enemyRace = (BWAPI::Broodwar->enemy() ? BWAPI::Broodwar->enemy()->getRace() : BWAPI::Races::Unknown);
    selectBuildOrderAgainstRace(enemyRace);
}

void BuildManager::clearBuildOrder(bool clearPendingRequests)
{
    activeBuildOrderIndex = -1;
    activeBuildOrderStep = 0;
    buildOrderActive = false;
    buildOrderCompleted = true;

    if (clearPendingRequests)
    {
        // Remove any pending build-order building requests that haven't started
        for (auto it = resourceRequests.begin(); it != resourceRequests.end();)
        {
            if (it->fromBuildOrder && it->state == ResourceRequest::State::PendingApproval)
                it = resourceRequests.erase(it);
            else
                ++it;
        }
    }
}

void BuildManager::overrideBuildOrder(int buildOrderId)
{
    // Current setup: clear current build order requests, then replace with another chosen build order ID
    for (auto it = resourceRequests.begin(); it != resourceRequests.end();)
    {
        if (it->fromBuildOrder && it->state == ResourceRequest::State::PendingApproval)
            it = resourceRequests.erase(it);
        else
            ++it;
    }

    for (int i = 0; i < (int)buildOrders.size(); i++)
    {
        if (buildOrders[i].id == buildOrderId)
        {
            activeBuildOrderIndex = i;
            activeBuildOrderStep = 0;
            buildOrderActive = true;
            buildOrderCompleted = false;
            std::cout << "Overriding Build Order: " << buildOrderNameToString(buildOrders[i].name) << "\n";
            return;
        }
    }

    // If not found, fall back to random
    selectRandomBuildOrder();
}

bool BuildManager::enqueueBuildOrderBuilding(BWAPI::UnitType type, int count)
{
    for (int i = 0; i < count; i++)
    {
        ResourceRequest req;
        req.type = ResourceRequest::Type::Building;
        req.unit = type;
        req.fromBuildOrder = true;
        req.priority = 0;
        resourceRequests.push_back(req);
    }
    return true;
}

void BuildManager::runBuildOrderOnFrame()
{
    if (!isBuildOrderActive())
        return;

    BuildOrder& bo = buildOrders[activeBuildOrderIndex];
    const int supply = BWAPI::Broodwar->self()->supplyUsed() / 2;

    // Issue steps in order; at most 1 step per frame to avoid spikes
    if (activeBuildOrderStep >= bo.steps.size())
    {
        buildOrderCompleted = true;
        buildOrderActive = false;
        std::cout << "Build Order Completed: " << buildOrderNameToString(bo.name) << "\n";
        return;
    }

    BuildOrderStep& step = bo.steps[activeBuildOrderStep];

    bool triggerMet = false;
    if (step.trigger.type == BuildTriggerType::Immediately) triggerMet = true;
    if (step.trigger.type == BuildTriggerType::AtSupply && supply >= step.trigger.value) triggerMet = true;

    if (!triggerMet)
        return;

    
bool issued = false;

switch (step.type)
{
    case BuildStepType::ScoutWorker:
        if (commanderReference) { commanderReference->getUnitToScout(); issued = true; }
        break;

    case BuildStepType::SupplyRampNatural:
        issued = enqueueSupplyAtNaturalRamp();
        break;

    case BuildStepType::NaturalWall:
        issued = enqueueNaturalWallAtChoke();
        break;

    case BuildStepType::Build:
    default:
        issued = enqueueBuildOrderBuilding(step.unit, step.count);
        break;
}

if (issued)
    activeBuildOrderStep++;

}

// BWEB placement helpers

BWAPI::TilePosition BuildManager::findNaturalRampPlacement(BWAPI::UnitType type) const
{
    const auto* choke = BWEB::Map::getNaturalChoke();
    if (!choke)
        return BWAPI::TilePositions::Invalid;

    // Anchor on the choke tile closest to our natural, then inch a few tiles toward the main
    const BWAPI::Position natPos = BWEB::Map::getNaturalPosition();
    const BWAPI::Position mainPos = BWEB::Map::getMainPosition();

    const BWAPI::TilePosition chokeTile(BWEB::Map::getClosestChokeTile(choke, natPos));
    const BWAPI::TilePosition mainTile(BWEB::Map::getMainTile());

    const int dirX = (mainTile.x > chokeTile.x) ? 1 : (mainTile.x < chokeTile.x ? -1 : 0);
    const int dirY = (mainTile.y > chokeTile.y) ? 1 : (mainTile.y < chokeTile.y ? -1 : 0);

    const BWAPI::TilePosition anchor = chokeTile + BWAPI::TilePosition(dirX, dirY) * 3;

    const int w = type.tileWidth();
    const int h = type.tileHeight();

    auto inBounds = [&](const BWAPI::TilePosition& t) {
        return t.isValid()
            && t.x >= 0 && t.y >= 0
            && (t.x + w) <= BWAPI::Broodwar->mapWidth()
            && (t.y + h) <= BWAPI::Broodwar->mapHeight();
    };

    const int maxR = 10;
    for (int r = 0; r <= maxR; r++)
    {
        for (int dx = -r; dx <= r; dx++)
        {
            for (int dy = -r; dy <= r; dy++)
            {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;

                const BWAPI::TilePosition t = anchor + BWAPI::TilePosition(dx, dy);
                if (!inBounds(t)) continue;
                if (!BWEB::Map::isPlaceable(type, t)) continue;
                if (BWEB::Map::isUsed(t, w, h) != BWAPI::UnitTypes::None) continue;

                // Try not to reject reserved here since BWEB blocks may reserve ramp-adjacent tiles on purpose
                return t;
            }
        }
    }

    return BWAPI::TilePositions::Invalid;
}



void BuildManager::resetNaturalWallPlan()
{
    naturalWallPlanned = false;
    naturalWallPylonEnqueued = false;
    naturalWallGatewaysEnqueued = false;
    naturalWallPylonTile = BWAPI::TilePositions::Invalid;
    naturalWallGatewayTiles.clear();
}

// Find a good pylon tile very near the natural choke for wall power.
BWAPI::TilePosition BuildManager::findNaturalChokePylonTile() const
{
    const auto* choke = BWEB::Map::getNaturalChoke();
    if (!choke) return BWAPI::TilePositions::Invalid;

    const BWAPI::Position mainPos = BWEB::Map::getMainPosition();
    const BWAPI::TilePosition anchor(BWEB::Map::getClosestChokeTile(choke, mainPos));

    const auto pylon = BWAPI::UnitTypes::Protoss_Pylon;
    const int w = pylon.tileWidth();
    const int h = pylon.tileHeight();

    auto inBounds = [&](const BWAPI::TilePosition& t) {
        return t.isValid()
            && t.x >= 0 && t.y >= 0
            && (t.x + w) <= BWAPI::Broodwar->mapWidth()
            && (t.y + h) <= BWAPI::Broodwar->mapHeight();
    };

    // Prefer main-side area if possible
    const BWEM::Area* mainArea = BWEB::Map::getMainArea();

    // Ring search around anchor
    for (int r = 0; r <= 10; r++) {
        for (int dx = -r; dx <= r; dx++) {
            for (int dy = -r; dy <= r; dy++) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                BWAPI::TilePosition t = anchor + BWAPI::TilePosition(dx, dy);
                if (!inBounds(t)) continue;

                if (mainArea && BWEB::Map::mapBWEM.GetArea(t) != mainArea)
                    continue;

                if (!BWAPI::Broodwar->canBuildHere(t, pylon))
                    continue;

                if (BWEB::Map::isUsed(t, w, h) != BWAPI::UnitTypes::None)
                    continue;

                return t;
            }
        }
    }

    // Fallback: allow non-main area
    for (int r = 0; r <= 10; r++) {
        for (int dx = -r; dx <= r; dx++) {
            for (int dy = -r; dy <= r; dy++) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                BWAPI::TilePosition t = anchor + BWAPI::TilePosition(dx, dy);
                if (!inBounds(t)) continue;

                if (!BWAPI::Broodwar->canBuildHere(t, pylon))
                    continue;

                if (BWEB::Map::isUsed(t, w, h) != BWAPI::UnitTypes::None)
                    continue;

                return t;
            }
        }
    }

    return BWAPI::TilePositions::Invalid;
}

bool BuildManager::enqueueSupplyAtNaturalRamp()
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Protoss)
        return false;

    const auto type = BWAPI::UnitTypes::Protoss_Pylon;

    const BWAPI::TilePosition t = findNaturalRampPlacement(type);
    if (!isValidBuildTile(type, t))
        return false;

    ResourceRequest req;
    req.type = ResourceRequest::Type::Building;
    req.unit = type;
    req.fromBuildOrder = true;
    req.priority = 0;

    req.useForcedTile = true;
    req.forcedTile = t;

    // Reserve immediately so other placement logic doesn't steal the spot
    BWEB::Map::addReserve(t, type.tileWidth(), type.tileHeight());

    resourceRequests.push_back(req);
    return true;
}



bool BuildManager::enqueueNaturalWallAtChoke()
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Protoss)
        return false;

    const auto* choke = BWEB::Map::getNaturalChoke();
    const auto* area  = BWEB::Map::getNaturalArea();
    if (!choke || !area)
        return false;

    // If we haven't planned the wall layout yet, generate it once (and cache tiles)
    if (!naturalWallPlanned)
    {
        std::vector<std::vector<BWAPI::UnitType>> candidates = {
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon },
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway },
            { BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway },
            // fallback smaller wall : 1 gate + pylon
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon }
        };

        BWEB::Wall* wall = nullptr;
        for (auto& b : candidates)
        {
            wall = BWEB::Walls::createWall(b, area, choke, BWAPI::UnitTypes::None, {}, true, false);
            if (wall) break;
        }

        if (!wall)
            return false;

        // Choose pylon tile (small tile closest to choke center)
        if (!wall->getSmallTiles().empty())
        {
            const auto& small = wall->getSmallTiles();
            if (small.empty()) return false;

            BWAPI::TilePosition anchor = BWAPI::TilePosition(BWEB::Map::getClosestChokeTile(BWEB::Map::getNaturalChoke(),
                BWEB::Map::getMainPosition()));

            
BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
            int bestDist = INT_MAX;

            const BWEM::Area* mainArea = BWEB::Map::getMainArea();

            for (const auto& t : small)
            {
                // Prefer tiles on the main-side area and reachable from main.
                if (mainArea && BWEB::Map::mapBWEM.GetArea(t) != mainArea)
                    continue;

                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                    continue;

                int d = std::abs(t.x - anchor.x) + std::abs(t.y - anchor.y);
                if (d < bestDist)
                {
                    bestDist = d;
                    best = t;
                }
            }

            // If BWEB's small tiles don't give us a good main-side reachable pylon spot, fall back to search
            if (!best.isValid())
                naturalWallPylonTile = BWAPI::TilePositions::Invalid;
            else
                naturalWallPylonTile = best;
        }

        // Get gateway tiles
        naturalWallGatewayTiles.clear();
        for (const auto& t : wall->getMediumTiles())
            naturalWallGatewayTiles.push_back(t);
        for (const auto& t : wall->getLargeTiles())
            naturalWallGatewayTiles.push_back(t);

        naturalWallPlanned = true;
        naturalWallPylonEnqueued = false;
        naturalWallGatewaysEnqueued = false;
    }

    if (!naturalWallPylonEnqueued)
    {
        // If BWEB didn't provide a good pylon tile, fall back to searching near choke
        BWAPI::TilePosition pylonTile = naturalWallPylonTile.isValid() ? naturalWallPylonTile : findNaturalChokePylonTile();
        if (!pylonTile.isValid() || !isValidBuildTile(BWAPI::UnitTypes::Protoss_Pylon, pylonTile))
            return false;

        ResourceRequest req;
        req.type = ResourceRequest::Type::Building;
        req.unit = BWAPI::UnitTypes::Protoss_Pylon;
        req.fromBuildOrder = true;
        req.useForcedTile = true;
        req.forcedTile = pylonTile;
        req.priority = 0;

        // Then reserve
        BWEB::Map::addReserve(pylonTile, req.unit.tileWidth(), req.unit.tileHeight());

        resourceRequests.push_back(req);
        naturalWallPylonEnqueued = true;
        naturalWallPylonTile = pylonTile;
        return false;
    }

    // Wait for the pylon to be completed so gateways can be powered
    bool pylonComplete = false;
    for (auto u : BWAPI::Broodwar->self()->getUnits())
    {
        if (u->getType() == BWAPI::UnitTypes::Protoss_Pylon && u->isCompleted())
        {
            if (BWAPI::TilePosition(u->getPosition()).getDistance(naturalWallPylonTile) <= 3)
            {
                pylonComplete = true;
                break;
            }
        }
    }
    if (!pylonComplete)
        return false;

    if (!naturalWallGatewaysEnqueued)
    {
        bool enqueuedAny = false;

        
auto enqueueForced = [&](BWAPI::UnitType ut, const BWAPI::TilePosition& t)
        {
            if (!t.isValid()) return;

            // Check if reachable
            if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                return;
            if (!isTerrainBuildable(ut, t))
                return;

            if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss && ut.requiresPsi())
            {
                // nudge placement around the choke a bit if invalid or unpowered
                if (!BWAPI::Broodwar->hasPower(t, ut))
                {
                    bool found = false;
                    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                    int bestD = INT_MAX;

                    for (int r = 1; r <= 5 && !found; r++)
                    {
                        for (int dx = -r; dx <= r; dx++)
                        {
                            for (int dy = -r; dy <= r; dy++)
                            {
                                if (std::abs(dx) != r && std::abs(dy) != r) continue;

                                BWAPI::TilePosition tt = t + BWAPI::TilePosition(dx, dy);
                                if (!tt.isValid()) continue;

                                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(tt) + BWAPI::Position(16, 16)))
                                    continue;

                                if (!isTerrainBuildable(ut, tt))
                                    continue;

                                if (!BWAPI::Broodwar->hasPower(tt, ut))
                                    continue;

                                int d = std::abs(tt.x - naturalWallPylonTile.x) + std::abs(tt.y - naturalWallPylonTile.y);
                                if (d < bestD)
                                {
                                    bestD = d;
                                    best = tt;
                                }
                                found = true;
                            }
                        }
                    }

                    if (best.isValid())
                    {
                        ResourceRequest req;
                        req.type = ResourceRequest::Type::Building;
                        req.unit = ut;
                        req.fromBuildOrder = true;
                        req.useForcedTile = true;
                        req.forcedTile = best;
                        req.priority = 0;

                        resourceRequests.push_back(req);
                        enqueuedAny = true;
                    }
                    return;
                }
            }

            ResourceRequest req;
            req.type = ResourceRequest::Type::Building;
            req.unit = ut;
            req.fromBuildOrder = true;
            req.useForcedTile = true;
            req.forcedTile = t;
            req.priority = 0;

            resourceRequests.push_back(req);
            enqueuedAny = true;
        };

        if (naturalWallGatewayTiles.empty())
            return false;

        int placed = 0;
        for (const auto& t : naturalWallGatewayTiles)
        {
            enqueueForced(BWAPI::UnitTypes::Protoss_Gateway, t);
            placed++;
            if (placed >= 2) break;
        }

        if (!enqueuedAny)
            return false;

        naturalWallGatewaysEnqueued = true;
        return true;
    }

    return true;
}
