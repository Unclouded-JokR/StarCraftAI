#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "SpenderManager.h"
#include "BuildingPlacer.h"
#include "Builder.h"

BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference), buildingPlacer(this)
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
    //std::cout << "Builder Manager Initialized" << "\n";

    // Reset per-game state
    buildOrders.clear();
    activeBuildOrderIndex = -1;
    activeBuildOrderStep = 0;
    //Make false at the start of a game.
    buildOrderActive = false;
    buildOrderCompleted = false;

    buildingPlacer.onStart();
    builders.clear();
    buildings.clear();

    initBuildOrdersOnStart();
    selectRandomBuildOrder();
}


void BuildManager::onFrame(std::vector<ResourceRequest>& resourceRequests) 
{
    runBuildOrderOnFrame();

    buildingPlacer.drawPoweredTiles();

    //Have to have loop that can check if a building can create a unit and is powered.
    //Might also need to have some data that shows what each building is doing but might be too complictaed for this approach.
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
                    if (request.requestedBuilding->train(request.unit))
                    {
                        request.state = ResourceRequest::State::Accepted_Completed;
                        request.frameRequestServiced = BWAPI::Broodwar->getFrameCount();
                    }
                    else
                    {
                        std::cout << "Failed to train unit for whatever reason\n";
                    }
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
                    if (request.placementPos == BWAPI::Positions::Invalid) request.placementInfo = buildingPlacer.getPositionToBuild(request.unit, request.base);

                    if (request.placementInfo.position == BWAPI::Positions::Invalid && request.gotPositionToBuild == false)
                    {
                        const PlacementInfo::PlacementFlag flag_info = request.placementInfo.flag;

                        switch (flag_info)
                        {
                        case PlacementInfo::NO_POWER:
                            //should create a new pylon request by inserting a pylon in the front of the queue.
                            //std::cout << "FAILED: NO POWER\n";
                            break;
                        case PlacementInfo::NO_BLOCKS:
                            //Wait for a bit and kill the request if no blocks are added
                            //std::cout << "FAILED: NO BLOCKS\n";
                            break;
                        case PlacementInfo::NO_GYSERS:
                            //Shouldnt happen but okay
                            //std::cout << "FAILED: NO GYSERS\n";
                            break;
                        case PlacementInfo::NO_PLACEMENTS:
                            //Wait for a bit and kill the request if no placements are added.
                            //std::cout << "FAILED: NO PLACEMENTS\n";
                            break;
                        case PlacementInfo::NO_EXPANSIONS:
                            //kill the expansion request.
                            //std::cout << "FAILED: NO EXPANSION\n";
                            break;
                        }

                        request.attempts++;
                    }
                    else
                    {
                        request.placementPos = request.placementInfo.position;
                        request.tileToPlace = request.placementInfo.topLeft;

                        const BWAPI::Unit workerAvalible = getUnitToBuild(request.placementPos);

                        if (workerAvalible == nullptr) continue;

                        Path pathToLocation;
                        if (request.unit.isResourceDepot())
                        {
                            //std::cout << "Trying to build Nexus\n";
                            pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), request.placementPos);
                        }
                        else if (request.unit.isRefinery())
                        {
                            //std::cout << "Trying to build assimlator\n";
                            pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), request.placementPos, true);
                        }
                        else
                        {
                            //std::cout << "Trying to build regular building\n";
                            pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), request.placementPos);
                        }

                        Builder temp = Builder(workerAvalible, request.unit, request.placementPos, pathToLocation);
                        builders.push_back(temp);

                        request.state = ResourceRequest::State::Approved_BeingBuilt;
                        request.frameRequestServiced = BWAPI::Broodwar->getFrameCount();

                        BWEB::Map::addUsed(request.placementInfo.topLeft, request.unit);
                    }
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
                    if (request.requestedBuilding->upgrade(request.upgrade))
                    {
                        request.state = ResourceRequest::State::Accepted_Completed;
                        request.frameRequestServiced = BWAPI::Broodwar->getFrameCount();
                    }
                    else
                    {
                        std::cout << "Failed to upgrade for whatever reason\n";
                    }
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
                    if (request.requestedBuilding->upgrade(request.upgrade))
                    {
                        request.state = ResourceRequest::State::Accepted_Completed;
                        request.frameRequestServiced = BWAPI::Broodwar->getFrameCount();
                    }
                    else
                    {
                        std::cout << "Failed to research tech for whatever reason\n";
                    }
                }
                break;
            }
        }
    }

    for (Builder& builder : builders)
    {
        builder.onFrame();
    }
}

void BuildManager::onUnitCreate(BWAPI::Unit unit)
{
    buildingPlacer.onUnitCreate(unit);

    if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

    //std::cout << unit->getType() << " placed down at tile position " << unit->getTilePosition() << "\n";

    //Remove worker once a building is being warped in.
    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
    {
        if (BWAPI::Position(unit->getTilePosition()) == it->requestedPositionToBuild && unit->getType() == it->buildingToConstruct)
        {
            //std::cout << "Builder placed down " << unit->getType() << ", removing from builders\n";
            it = builders.erase(it);
            break;
        }
    }

    if (unit->getType().isBuilding() && !unit->isCompleted()) buildings.insert(unit);
}


void BuildManager::onUnitDestroy(BWAPI::Unit unit)
{
    buildingPlacer.onUnitDestroy(unit);

    if (unit->getPlayer() != BWAPI::Broodwar->self())
        return;

    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end();)
    {
        if (it->getUnitReference()->getID() == unit->getID())
        {
            //std::cout << "Builder has died\n";

            const BWAPI::Unit unitAvalible = getUnitToBuild(it->requestedPositionToBuild);

            if (unitAvalible != nullptr)
            {
                Path pathToLocation;
                if (it->buildingToConstruct.isResourceDepot())
                {
                    //std::cout << "Trying to build Nexus\n";
                    pathToLocation = AStar::GeneratePath(unitAvalible->getPosition(), unitAvalible->getType(), it->requestedPositionToBuild);
                }
                else if (it->buildingToConstruct.isRefinery())
                {
                    //std::cout << "Trying to build assimlator\n";
                    pathToLocation = AStar::GeneratePath(unitAvalible->getPosition(), unitAvalible->getType(), it->requestedPositionToBuild, true);
                }
                else
                {
                    //std::cout << "Trying to build regular building\n";
                    pathToLocation = AStar::GeneratePath(unitAvalible->getPosition(), unitAvalible->getType(), it->requestedPositionToBuild);
                }

                it->setUnitReference(unitAvalible);
                it->updatePath(pathToLocation);
                //std::cout << "Replacement found and updated path\n";
            }
            else
            {
                //std::cout << "Replacement could not be found\n";
            }

            break;
        }

        it++;
    }

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

    //std::cout << "Created " << unit->getType() << " (On Morph)\n";

    if (unit->getType() == BWAPI::UnitTypes::Protoss_Assimilator && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
        {
            if (unit->getType() == it->buildingToConstruct && BWAPI::Position(unit->getTilePosition()) == it->requestedPositionToBuild)
            {
                //std::cout << "Assimilator built at location: " << BWAPI::Position(unit->getTilePosition()) << "\n";
                //std::cout << "Builder assigned to place at location: " << it->requestedPositionToBuild << "\n";
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

bool BuildManager::checkWorkerIsConstructing(BWAPI::Unit unit)
{
    for (Builder& builder : builders)
    {
        if (builder.getUnitReference()->getID() == unit->getID()) return true;
    }

    return false;
}

BWAPI::Unitset BuildManager::getBuildings()
{
    return buildings;
}

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
}

bool BuildManager::checkUnitIsBeingWarpedIn(BWAPI::UnitType unit, const BWEM::Base* nexus)
{
    if (nexus == nullptr)
    {
        for (const BWAPI::Unit building : buildings)
        {
            if (unit == building->getType() && !building->isCompleted())
            {
                return true;
            }
        }
    }
    else
    {
        for (const BWAPI::Unit building : buildings)
        {
            if (building->getType() != BWAPI::UnitTypes::Protoss_Assimilator) continue;

            if (unit == building->getType() && !building->isCompleted())
            {
                for (BWEM::Geyser* geyer : nexus->Geysers())
                {
                    if (building->getPosition() == geyer->Pos()) return true;
                }

                //if (nexusPosition != BWAPI::Positions::Invalid) std::cout << "Nexus at " << nexusPosition << " already has assimilator requested: " << "(Approx. distance " << nexusPosition.getApproxDistance(building->getPosition()) << ")\n";
            }
        }
    }

    return false;
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

std::string BuildManager::buildOrderNameToString(int name) const
{
    switch (name)
    {
        case 1: return "2 Gateway Dark Templar vs Terran";
        case 2: return "2 Gateway Dark Templar vs Protoss";
        case 3: return "2 Gateway Observer vs Zerg";
    }
}

void BuildManager::initBuildOrdersOnStart()
{
    buildOrders = BuildOrders::createAll();
}

void BuildManager::selectBuildOrderAgainstRace(BWAPI::Race enemyRace)
{
    /*if (buildOrders.empty())
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
    }*/

    std::vector<int> candidates;
    for (int i = 0; i < (int)buildOrders.size(); i++)
    {
        if (buildOrders[i].vsRace == enemyRace)
            candidates.push_back(i);
    }

    activeBuildOrderIndex = candidates[0];
    //activeBuildOrderIndex = 0;
    activeBuildOrderStep = 0;
    buildOrderActive = true;
    buildOrderCompleted = false;

    std::cout << "Selected Build Order: " << buildOrderNameToString(buildOrders[activeBuildOrderIndex].name) << "\n";
}

void BuildManager::selectRandomBuildOrder()
{
    const BWAPI::Race enemyRace = (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Unknown ? BWAPI::Broodwar->enemy()->getRace() : BWAPI::Races::Unknown);
    selectBuildOrderAgainstRace(enemyRace);
}

bool BuildManager::enqueueBuildOrderBuilding(BWAPI::UnitType type, int count)
{
    for (int i = 0; i < count; i++)
    {
        /*ResourceRequest req;
        req.type = ResourceRequest::Type::Building;
        req.unit = type;
        req.fromBuildOrder = true;
        resourceRequests.push_back(req);*/

        commanderReference->requestBuilding(type, true);
    }
    return true;
}

bool BuildManager::enqueueBuildOrderUnit(BWAPI::UnitType type, int count)
{
    if (!commanderReference)
        return false;

    const BWAPI::UnitType trainerType = type.whatBuilds().first;
    if (trainerType == BWAPI::UnitTypes::None)
        return false;

    int requestsIssued = 0;
    for (BWAPI::Unit building : buildings)
    {
        if (!building || building->getType() != trainerType)
            continue;
        if (!building->isCompleted() || !building->isPowered() || building->isTraining())
            continue;
        if (!building->canTrain(type) || commanderReference->alreadySentRequest(building->getID()))
            continue;

        commanderReference->requestUnit(type, building, true);
        requestsIssued++;

        if (requestsIssued >= count)
            return true;
    }

    return false;
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
        //std::cout << "Build Order Completed: " << buildOrderNameToString(bo.name) << "\n";
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
    case BuildStepType::Train:
        issued = enqueueBuildOrderUnit(step.unit, step.count);
        break;
    case BuildStepType::Build:
    default:
        issued = enqueueBuildOrderBuilding(step.unit, step.count);
        break;
}

if (issued)
    activeBuildOrderStep++;

}