#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "SpenderManager.h"
#include "BuildingPlacer.h"
#include "Builder.h"
#include <random>
#include <ctime>
#include <algorithm>
#include <climits>
#include <cmath>

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
    //Make false at the start of a game.
    std::cout << "Builder Manager Initialized" << "\n";
    buildOrderCompleted = true;
    spenderManager.onStart();
    buildingPlacer.onStart();
    builders.clear();

    // Build orders: define and select one at game start
    initBuildOrdersOnStart();
    selectRandomBuildOrder();
}

void BuildManager::onFrame() {
    for (std::vector<ResourceRequest>::iterator it = resourceRequests.begin(); it != resourceRequests.end();)
    {
        (it->state == ResourceRequest::State::Accepted_Completed) ? it = resourceRequests.erase(it) : it++;
    }

    // Allow build order to enqueue new requests before budgeting/approval
    processBuildOrderSteps();

    spenderManager.OnFrame(resourceRequests);
    buildingPlacer.drawPoweredTiles();

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
                    request.requestedBuilding->isCompleted())
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
                    BWAPI::Position locationToPlace;
                    if (request.useForcedTile && request.forcedTile.isValid())
                        locationToPlace = BWAPI::Position(request.forcedTile);
                    else
                        locationToPlace = buildingPlacer.getPositionToBuild(request.unit);

                    if (locationToPlace == BWAPI::Positions::Invalid) continue;

                    const BWAPI::Unit workerAvalible = getUnitToBuild(locationToPlace);

                    if (workerAvalible == nullptr) continue;

                    //For now dont use Astar to get path to location
                    Path pathToLocation;
                    if (request.unit.isResourceDepot())
                    {
                        std::cout << "Trying to build Nexus\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), locationToPlace);
                    }
                    else if(request.unit.isRefinery())
                    {
                        //std::cout << "Trying to build assimlator\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), locationToPlace, true);
                    }
                    else
                    {
                        //std::cout << "Trying to build regular building\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), locationToPlace);
                    }

                    Builder temp = Builder(workerAvalible, request.unit, locationToPlace, pathToLocation);
                    builders.push_back(temp);

                    request.state = ResourceRequest::State::Approved_BeingBuilt;
                }
                break;
            }
            case ResourceRequest::Type::Upgrade:
            {
                if (request.requestedBuilding->canUpgrade(request.upgrade) &&
                    !request.requestedBuilding->isUpgrading() &&
                    request.requestedBuilding->isCompleted())
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
                    request.requestedBuilding->isCompleted())
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
    for (BWAPI::Unit building : buildings)
    {
        BWAPI::Broodwar->drawTextMap(building->getPosition(), std::to_string(building->getID()).c_str());
    }

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

    //std::cout << "Created " << unit->getType() << "\n";

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
            it->setUnitReference(unitAvalible);
            break;
        }
        else
        {
            it++;
        }
    }

    if (unit->getPlayer() != BWAPI::Broodwar->self())
        return;

    BWAPI::UnitType unitType = unit->getType();

    if (!unitType.isBuilding()) return;

    //Check if a building has been killed
    for (BWAPI::Unit building : buildings)
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

    std::cout << "Created " << unit->getType() << "\n";

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
    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = building;

    resourceRequests.push_back(request);
}

void BuildManager::buildBuilding(BWAPI::UnitType building, BWAPI::Unit scout)
{
    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = building;
    request.scoutToPlaceBuilding = scout;
    request.isCheese = true;

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
#pragma region BUILD ORDER SYSTEM

int BuildManager::countMyUnits(BWAPI::UnitType type) const
{
    int c = 0;
    for (auto u : BWAPI::Broodwar->self()->getUnits())
        if (u && u->exists() && u->getType() == type)
            c++;
    return c;
}

int BuildManager::countPlanned(BWAPI::UnitType type) const
{
    int c = 0;
    for (const auto& r : resourceRequests)
    {
        if ((r.type == ResourceRequest::Type::Building || r.type == ResourceRequest::Type::Unit) && r.unit == type)
        {
            if (r.state != ResourceRequest::State::Accepted_Completed)
                c++;
        }
    }
    return c;
}

bool BuildManager::isTriggerMet(const BuildTrigger& t) const
{
    switch (t.type)
    {
    case BuildTriggerType::Immediately:
        return true;
    case BuildTriggerType::AtSupply:
        return (BWAPI::Broodwar->self()->supplyUsed() / 2) >= t.supply;
    default:
        return true;
    }
}

void BuildManager::initBuildOrdersOnStart()
{
    buildOrders.clear();

    BWAPI::Race enemyRace = BWAPI::Broodwar->enemy() ? BWAPI::Broodwar->enemy()->getRace() : BWAPI::Races::Unknown;

    // --- 2 Gateway Observer vs Terran ---
    {
        BuildOrder bo;
        bo.name = 200;
        bo.id = 1;
        bo.vsRace = BWAPI::Races::Terran;
        bo.debugName = "2 Gateway Observer (vs Terran)";

        bo.steps = {
            { BuildStepType::SupplyRampNatural, { BuildTriggerType::AtSupply, 9  }, BWAPI::UnitTypes::Protoss_Pylon },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 12 }, BWAPI::UnitTypes::Protoss_Gateway },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 13 }, BWAPI::UnitTypes::Protoss_Assimilator },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 15 }, BWAPI::UnitTypes::Protoss_Gateway },
            { BuildStepType::ScoutWorker,       { BuildTriggerType::AtSupply, 15 }, BWAPI::UnitTypes::None },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 16 }, BWAPI::UnitTypes::Protoss_Cybernetics_Core },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 20 }, BWAPI::UnitTypes::Protoss_Robotics_Facility },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 22 }, BWAPI::UnitTypes::Protoss_Observatory },
            { BuildStepType::NaturalWall,             { BuildTriggerType::AtSupply, 23 }, BWAPI::UnitTypes::None } //Debug
        };
        buildOrders.push_back(std::move(bo));
    }

    // --- 3 Gate Robo vs Protoss ---
    {
        BuildOrder bo;
        bo.name = 100;
        bo.id = 2;
        bo.vsRace = BWAPI::Races::Protoss;
        bo.debugName = "3 Gate Robo (vs Protoss)";

        bo.steps = {
            { BuildStepType::SupplyRampNatural, { BuildTriggerType::AtSupply, 9  }, BWAPI::UnitTypes::Protoss_Pylon },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 12 }, BWAPI::UnitTypes::Protoss_Gateway },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 13 }, BWAPI::UnitTypes::Protoss_Assimilator },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 15 }, BWAPI::UnitTypes::Protoss_Gateway },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 16 }, BWAPI::UnitTypes::Protoss_Cybernetics_Core },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 20 }, BWAPI::UnitTypes::Protoss_Gateway },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 22 }, BWAPI::UnitTypes::Protoss_Robotics_Facility },
            { BuildStepType::ScoutWorker,       { BuildTriggerType::AtSupply, 18 }, BWAPI::UnitTypes::None }
        };
        buildOrders.push_back(std::move(bo));
    }

    // --- Corsair/Dragoon vs Zerg ---
    {
        BuildOrder bo;
        bo.name = 300;
        bo.id = 3;
        bo.vsRace = BWAPI::Races::Zerg;
        bo.debugName = "Corsair/Dragoon (vs Zerg)";

        bo.steps = {
            { BuildStepType::SupplyRampNatural, { BuildTriggerType::AtSupply, 9  }, BWAPI::UnitTypes::Protoss_Pylon },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 12 }, BWAPI::UnitTypes::Protoss_Gateway },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 13 }, BWAPI::UnitTypes::Protoss_Assimilator },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 16 }, BWAPI::UnitTypes::Protoss_Cybernetics_Core },
            { BuildStepType::Build,             { BuildTriggerType::AtSupply, 18 }, BWAPI::UnitTypes::Protoss_Stargate },
            { BuildStepType::ScoutWorker,       { BuildTriggerType::AtSupply, 15 }, BWAPI::UnitTypes::None }
        };
        buildOrders.push_back(std::move(bo));
    }

}

void BuildManager::selectRandomBuildOrder()
{
    activeBuildOrder = nullptr;
    activeStepIndex = 0;
    activeBuildOrderName.clear();

    if (buildOrders.empty())
        return;

    BWAPI::Race enemyRace = BWAPI::Broodwar->enemy() ? BWAPI::Broodwar->enemy()->getRace() : BWAPI::Races::Unknown;

    // Prefer build orders matching the enemy race; if none match, pick from all.
    std::vector<const BuildOrder*> candidates;
    for (const auto& bo : buildOrders)
        if (bo.vsRace == enemyRace)
            candidates.push_back(&bo);

    if (candidates.empty())
        for (const auto& bo : buildOrders)
            candidates.push_back(&bo);

    std::mt19937 rng((unsigned)BWAPI::Broodwar->getFrameCount() ^ (unsigned)time(nullptr));
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    activeBuildOrder = candidates[dist(rng)];
    activeStepIndex = 0;
    activeBuildOrderName = activeBuildOrder->debugName;

    if (commanderReference)
        commanderReference->buildOrderSelected = activeBuildOrderName;

    std::cout << "Selected Build Order: " << activeBuildOrderName << "\n";
}

void BuildManager::overrideBuildOrder(int buildOrderId)
{
    // Clear queued (not-yet-building) requests and switch active build order.
    clearBuildOrderAndQueue(true);

    for (const auto& bo : buildOrders)
    {
        if (bo.id == buildOrderId)
        {
            activeBuildOrder = &bo;
            activeStepIndex = 0;
            activeBuildOrderName = bo.debugName;

            if (commanderReference)
                commanderReference->buildOrderSelected = activeBuildOrderName;

            std::cout << "Overrode Build Order: " << activeBuildOrderName << "\n";
            return;
        }
    }

    // If not found, keep current and log
    std::cout << "overrideBuildOrder: buildOrderId not found: " << buildOrderId << "\n";
}

void BuildManager::clearBuildOrderAndQueue(bool keepActiveBuilders)
{
    // Reset the script pointer/index. Does not clear buildOrders definitions.
    activeBuildOrder = nullptr;
    activeStepIndex = 0;
    activeBuildOrderName.clear();

    // Clear pending requests that haven't started a worker assignment yet.
    for (auto it = resourceRequests.begin(); it != resourceRequests.end(); )
    {
        const bool isBuildingInProgress = (it->state == ResourceRequest::State::Approved_BeingBuilt);
        if (keepActiveBuilders && isBuildingInProgress) { ++it; continue; }

        it = resourceRequests.erase(it);
    }
}

void BuildManager::processBuildOrderSteps()
{
    if (!activeBuildOrder) return;
    if (activeStepIndex >= activeBuildOrder->steps.size()) return;

    while (activeStepIndex < activeBuildOrder->steps.size())
    {
        const auto& step = activeBuildOrder->steps[activeStepIndex];
        if (!isTriggerMet(step.trigger))
            break;

        enqueueStep(step);
        activeStepIndex++;
    }
}

void BuildManager::enqueueStep(const BuildOrderStep& s)
{
    switch (s.stepType)
    {
    case BuildStepType::Build:
        if (s.unit != BWAPI::UnitTypes::None)
            buildBuilding(s.unit);
        break;

    case BuildStepType::ScoutWorker:
        if (commanderReference)
            commanderReference->getUnitToScout();
        break;

    case BuildStepType::SupplyRampNatural:
        enqueueSupplyRamp(s.unit == BWAPI::UnitTypes::None ? BWAPI::UnitTypes::Protoss_Pylon : s.unit);
        break;

    case BuildStepType::NaturalWall:
        enqueueNaturalWallProtoss();
        break;

    default:
        break;
    }
}

/// Select a BWEB 2x2 block slot in MAIN (upper ramp) nearest the natural choke.
BWAPI::TilePosition BuildManager::findSupplyRampBlockSlotMain() const
{
    const auto* choke = BWEB::Map::getNaturalChoke();
    if (!choke) return BWAPI::TilePositions::Invalid;

    const BWAPI::Position mainPos = BWEB::Map::getMainPosition();
    const BWAPI::TilePosition chokeNearMainTile(BWEB::Map::getClosestChokeTile(choke, mainPos));

    const BWEM::Area* mainArea = BWEB::Map::getMainArea();
    if (!mainArea) return BWAPI::TilePositions::Invalid;

    BWEB::Block* bestBlock = nullptr;
    int bestBlockDist = INT_MAX;

    for (auto& block : BWEB::Blocks::getBlocks())
    {
        const BWAPI::TilePosition bt = block.getTilePosition();
        if (BWEB::Map::mapBWEM.GetArea(bt) != mainArea)
            continue;

        const int d = std::abs(bt.x - chokeNearMainTile.x) + std::abs(bt.y - chokeNearMainTile.y);

        if (d > 25)
            continue;

        if (d < bestBlockDist)
        {
            bestBlockDist = d;
            bestBlock = &block;
        }
    }

    if (!bestBlock) return BWAPI::TilePositions::Invalid;

    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
    int bestDist = INT_MAX;

    for (const auto& t : bestBlock->getSmallTiles())
    {
        if (BWEB::Map::mapBWEM.GetArea(t) != mainArea)
            continue;

        // IMPORTANT: block slots are expected to be reserved by BWEB; don't reject reserved tiles.
        if (!BWEB::Map::isPlaceable(BWAPI::UnitTypes::Protoss_Pylon, t))
            continue;

        if (BWEB::Map::isUsed(t, 2, 2) != BWAPI::UnitTypes::None)
            continue;

        const int d = std::abs(t.x - chokeNearMainTile.x) + std::abs(t.y - chokeNearMainTile.y);
        if (d < bestDist)
        {
            bestDist = d;
            best = t;
        }
    }

    return best;
}

void BuildManager::enqueueSupplyRamp(BWAPI::UnitType supplyType)
{
    // Only Protoss/Terran have building-based supply.
    if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
        supplyType = BWAPI::UnitTypes::Protoss_Pylon;
    else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
        supplyType = BWAPI::UnitTypes::Terran_Supply_Depot;
    else
        return;

    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = supplyType;

    // Force supply onto a BWEB block slot near the natural ramp on the MAIN side.
    if (supplyType == BWAPI::UnitTypes::Protoss_Pylon)
    {
        BWAPI::TilePosition t = findSupplyRampBlockSlotMain();
        if (t.isValid())
        {
            request.useForcedTile = true;
            request.forcedTile = t;
            // Block slot is usually already reserved by BWEB Blocks.
        }
    }

    resourceRequests.push_back(request);
}

bool BuildManager::enqueueWallFromLayout(BWEB::Wall* wall, const std::vector<BWAPI::UnitType>& buildings)
{
    if (!wall) return false;

    // Copy tile sets for assignment
    auto& small = wall->getSmallTiles();
    auto& medium = wall->getMediumTiles();
    auto& large = wall->getLargeTiles();

    std::set<BWAPI::TilePosition> smallAvail = small;
    std::set<BWAPI::TilePosition> medAvail = medium;
    std::set<BWAPI::TilePosition> largeAvail = large;

    auto popClosest = [](std::set<BWAPI::TilePosition>& avail, BWAPI::TilePosition anchor) -> BWAPI::TilePosition {
        if (avail.empty()) return BWAPI::TilePositions::Invalid;
        auto bestIt = avail.begin();
        int bestDist = INT_MAX;
        for (auto it = avail.begin(); it != avail.end(); ++it)
        {
            const int d = std::abs(it->x - anchor.x) + std::abs(it->y - anchor.y);
            if (d < bestDist) { bestDist = d; bestIt = it; }
        }
        BWAPI::TilePosition out = *bestIt;
        avail.erase(bestIt);
        return out;
    };

    BWAPI::TilePosition anchor((BWAPI::Position)wall->getCentroid());

    for (auto ut : buildings)
    {
        BWAPI::TilePosition t = BWAPI::TilePositions::Invalid;
        if (ut.tileWidth() >= 4) t = popClosest(largeAvail, anchor);
        else if (ut.tileWidth() >= 3) t = popClosest(medAvail, anchor);
        else t = popClosest(smallAvail, anchor);

        if (!t.isValid())
            continue;

        ResourceRequest req;
        req.type = ResourceRequest::Type::Building;
        req.unit = ut;
        req.useForcedTile = true;
        req.forcedTile = t;
        resourceRequests.push_back(req);
    }

    return true;
}

void BuildManager::enqueueNaturalWallProtoss()
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Protoss)
        return;

    const BWEM::Area* natArea = BWEB::Map::getNaturalArea();
    const BWEM::ChokePoint* natChoke = BWEB::Map::getNaturalChoke();
    if (!natArea || !natChoke) return;

    std::vector<BWAPI::UnitType> buildings = {
        BWAPI::UnitTypes::Protoss_Pylon,
        BWAPI::UnitTypes::Protoss_Gateway,
        BWAPI::UnitTypes::Protoss_Forge
    };

    BWEB::Wall* wall = BWEB::Walls::createWall(buildings, natArea, natChoke, BWAPI::UnitTypes::None, {}, true, false);
    if (!wall) return;

    enqueueWallFromLayout(wall, buildings);
}

#pragma endregion
