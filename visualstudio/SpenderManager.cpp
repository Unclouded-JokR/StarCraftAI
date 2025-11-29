#include "ProtoBotCommander.h"
#include "SpenderManager.h"

SpenderManager::SpenderManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
}

void SpenderManager::onStart()
{
    std::cout << "Spender Manager Initialized" << "\n";
    builders.clear();
    buildRequests.clear();
    plannedBuildings.clear();
    plannedUnits.clear();
    requestIdentifiers.clear();
}

void SpenderManager::addRequest(BWAPI::UnitType building)
{
    BuildRequest requestToAdd;
    BuildStructureRequest temp;

    temp.buildingType = building;
    requestToAdd.request = temp;

    buildRequests.push_back(requestToAdd);
}

void SpenderManager::addRequest(BWAPI::Unit unit, BWAPI::UpgradeType upgradeToResearch)
{
    BuildRequest requestToAdd;
    ResearchUpgradeRequest temp;
    temp.upgradeType = upgradeToResearch;
    temp.building = unit;
    requestToAdd.request = temp;

    buildRequests.push_back(requestToAdd);
}

void SpenderManager::addRequest(BWAPI::UnitType unitToTrain, BWAPI::Unit buildingRequesting)
{
    BuildRequest requestToAdd;
    TrainUnitRequest temp;
    temp.unitType = unitToTrain;
    temp.building = buildingRequesting;

    //Add unit to already sent requests
    requestIdentifiers.push_back(buildingRequesting->getID());

    requestToAdd.request = temp;

    buildRequests.push_back(requestToAdd);
}

void SpenderManager::printQueue()
{
    std::cout << "Build Request Queue:" << "\n";

    int index = 1;
    for (BuildRequest buildRequest : buildRequests)
    {
        if (holds_alternative<BuildStructureRequest>(buildRequest.request))
        {
            const BuildStructureRequest buildingInQueue = get<BuildStructureRequest>(buildRequest.request);
            std::cout << "Index: " << index << " " << buildingInQueue.buildingType << "\n";
        }
        else if (holds_alternative<TrainUnitRequest>(buildRequest.request))
        {
            const TrainUnitRequest unitInQueue = get<TrainUnitRequest>(buildRequest.request);
            std::cout << "Index: " << index << " " << unitInQueue.unitType << "\n";
        }
        else if (holds_alternative<ResearchUpgradeRequest>(buildRequest.request))
        {
            const ResearchUpgradeRequest upgradeInQueue = get<ResearchUpgradeRequest>(buildRequest.request);
            std::cout << "Index: " << index << " " << upgradeInQueue.upgradeType << "\n";
        }
        index++;
    }
}

int SpenderManager::availableMinerals()
{
    int currentMineralCount = BWAPI::Broodwar->self()->minerals();

    for (BWAPI::UnitType building : plannedBuildings)
    {
        currentMineralCount -= building.mineralPrice();
    }

    return currentMineralCount;
}

int SpenderManager::availableGas()
{
    int currentGasCount = BWAPI::Broodwar->self()->gas();

    for (BWAPI::UnitType building : plannedBuildings)
    {
        currentGasCount -= building.gasPrice();
    }

    return currentGasCount;
}

int SpenderManager::availableSupply()
{
    int unusedSupply = (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed()) / 2;

    for (BWAPI::UnitType unit : plannedUnits)
    {
        unusedSupply -= (unit.supplyRequired() / 2);
    }

    return unusedSupply;
}

bool SpenderManager::canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas)
{
    if (((currentMinerals - mineralPrice) >= 0) && ((currentGas - gasPrice) >= 0))
    {
        return true;
    }

    return false;
}

bool SpenderManager::alreadyUsingTiles(BWAPI::TilePosition positon)
{
    for (const Builder& builder : builders)
    {
        if (builder.positionToBuild == BWAPI::Position(positon)) return true;
    }

    return false;
}

BWAPI::Position SpenderManager::getPositionToBuild(BWAPI::UnitType type)
{
    if (type == BWAPI::UnitTypes::Protoss_Nexus)
    {
        int distance = INT_MAX;
        BWAPI::TilePosition closestDistance;

        for (const BWEM::Area& area : theMap.Areas())
        {
            for (const BWEM::Base& base : area.Bases())
            {
                if (BWAPI::Broodwar->self()->getStartLocation().getApproxDistance(base.Location()) < distance
                    && base.Location() != BWAPI::Broodwar->self()->getStartLocation()
                    && BWAPI::Broodwar->canBuildHere(base.Location(), type))
                {
                    distance = BWAPI::Broodwar->self()->getStartLocation().getApproxDistance(base.Location());
                    closestDistance = base.Location();
                }
            }
        }

        //std::cout << "Closest Location at " << closestDistance.x << ", " << closestDistance.y << "\n";
        return BWAPI::Position(closestDistance);
    }
    else if (type == BWAPI::UnitTypes::Protoss_Assimilator)
    {
        std::vector<NexusEconomy> nexusEconomies = commanderReference->getNexusEconomies();

        for (const NexusEconomy& nexusEconomy : nexusEconomies)
        {
            if (nexusEconomy.vespeneGyser != nullptr && nexusEconomy.assimilator == nullptr)
            {
                //std::cout << "Nexus " << nexusEconomy.nexusID << " needs assimilator\n";
                //std::cout << "Gyser position = " << nexusEconomy.vespeneGyser->getPosition() << "\n";

                BWAPI::Position position = BWAPI::Position(nexusEconomy.vespeneGyser->getPosition().x - 55, nexusEconomy.vespeneGyser->getPosition().y - 25);
                return position;
            }
        }
    }
    else
    {
        int distance = INT_MAX;
        BWAPI::TilePosition closestDistance;
        std::vector<BWEB::Block> blocks = BWEB::Blocks::getBlocks();

        for (BWEB::Block block : blocks)
        {
            std::set<BWAPI::TilePosition> placements = block.getPlacements(type);

            for (const BWAPI::TilePosition placement : placements)
            {
                const int distanceToPlacement = BWAPI::Broodwar->self()->getStartLocation().getApproxDistance(placement);

                if (BWAPI::Broodwar->canBuildHere(placement, type)
                    && distanceToPlacement < distance
                    && !alreadyUsingTiles(placement))
                {
                    distance = distanceToPlacement;
                    closestDistance = placement;
                }
            }
        }

        //std::cout << distance << "\n";
        return BWAPI::Position(closestDistance);
    }

    return BWAPI::Position(0, 0);
}

bool SpenderManager::requestedBuilding(BWAPI::UnitType building)
{
    if (buildRequests.size() == 0)
    {
        return false;
    }

    for (BuildRequest& buildRequest : buildRequests)
    {
        if (holds_alternative<BuildStructureRequest>(buildRequest.request))
        {
            const BuildStructureRequest buildingInQueue = get<BuildStructureRequest>(buildRequest.request);

            if (buildingInQueue.buildingType == building)
            {
                return true;
            }
        }
    }

    return false;
}

void SpenderManager::OnFrame()
{
    int currentMineralCount = availableMinerals();
    int currentGasCount = availableGas();
    int currentSupply = availableSupply();

    int mineralPrice = 0;
    int gasPrice = 0;

    if (BWAPI::Broodwar->getFrameCount() % 48 == 0)
    {
        //printQueue();

        /*for (const BWAPI::UnitType building : plannedBuildings)
        {
            std::cout << "Planning to build: " << building << "\n";
        }*/
    }

    //prioritize buildings and units first.
    for (std::vector<BuildRequest>::iterator it = buildRequests.begin(); it != buildRequests.end();)
    {
        if (std::holds_alternative<BuildStructureRequest>(it->request))
        {
            const BuildStructureRequest temp = get<BuildStructureRequest>(it->request);

            mineralPrice = temp.buildingType.mineralPrice();
            gasPrice = temp.buildingType.gasPrice();

            if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
            {
                //Find position to place building using BWEB and BWEM
                const BWAPI::Position positionToBuild = getPositionToBuild(temp.buildingType);

                if (positionToBuild == BWAPI::Position(0, 0))
                {
                    ++it;
                    continue;
                }

                BWAPI::Unit unitAvalible = commanderReference->getUnitToBuild(positionToBuild);
                //BWAPI::Unit unitAvalible = commanderReference->getUnitToScout();

                if (unitAvalible == nullptr)
                {
                    //std::cout << "Did not find worker\n";
                    ++it;
                    continue;
                }
                //std::cout << "Found worker\n";
                //Sometimes worker is not being told to move for whatever reason?

                //std::cout << "Adding " << temp.buildingType << " to the queue\n";

                //Create new builder to keep track of.
                Builder builder;
                builder.probe = unitAvalible;
                builder.building = temp.buildingType;
                builder.positionToBuild = positionToBuild;
                builders.push_back(builder);

                //Plan the building and consider the amount of minerals it will take.
                plannedBuildings.push_back(temp.buildingType);
                currentMineralCount -= mineralPrice;
                currentGasCount -= gasPrice;

                //Remove request from the building requests.
                it = buildRequests.erase(it);
            }
            else
            {
                //Prioritize exapnsions and pylons 
                if (temp.buildingType == BWAPI::UnitTypes::Protoss_Nexus || temp.buildingType == BWAPI::UnitTypes::Protoss_Pylon)
                {
                    break;
                }
                else
                {
                    it++;
                }
            }
        }
        else if (std::holds_alternative<TrainUnitRequest>(it->request))
        {
            const TrainUnitRequest temp = get<TrainUnitRequest>(it->request);

            mineralPrice = temp.unitType.mineralPrice();
            gasPrice = temp.unitType.gasPrice();

            //Check if we train a unit we are not going to get supply blocked.
            if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount) && ((currentSupply - (temp.unitType.supplyRequired() / 2)) >= 0))
            {
                temp.building->train(temp.unitType);
                removeRequestID(temp.building->getID());

                plannedUnits.push_back(temp.unitType);

                currentMineralCount -= mineralPrice;
                currentGasCount -= gasPrice;
                currentSupply -= (temp.unitType.supplyRequired() / 2);

                //Why?
                it = buildRequests.erase(it);
                /*std::cout << "Current Supply: " << BWAPI::Broodwar->self()->supplyTotal() / 2 << "\n";
                std::cout << "Supply used: " << BWAPI::Broodwar->self()->supplyUsed() / 2 << "\n";*/
                //std::cout << "Avalible supply (considering planned units): " << currentSupply << "\n";
                //std::cout << "Training " << temp.unitType << " needs " << temp.unitType.supplyRequired() / 2 << " supply." << "\n";
            }
            else
            {
                it++;
            }
        }
        else
        {
            it++;
        }
    }

    //Check if we can build upgrades
    for (std::vector<BuildRequest>::iterator it = buildRequests.begin(); it != buildRequests.end();)
    {
        if (std::holds_alternative<ResearchUpgradeRequest>(it->request))
        {
            ResearchUpgradeRequest temp = get<ResearchUpgradeRequest>(it->request);

            mineralPrice = temp.upgradeType.mineralPrice();
            gasPrice = temp.upgradeType.gasPrice();

            if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
            {
                temp.building->upgrade(temp.upgradeType);

                currentMineralCount -= mineralPrice;
                currentGasCount -= gasPrice;

                //Why?
                it = buildRequests.erase(it);
                //std::cout << "Researching " << temp.upgradeType << "\n";
            }
            else
            {
                it++;
            }
        }
        else
        {
            it++;
        }

    }

    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end();)
    {
        if (it->probe->isConstructing())
        {
            it++;
            continue;
        }


        //Need to check if we are able to build. Units on tiles can cause buildings NOT to warp in
        if (it->probe->isIdle() || it->probe->getPosition() == it->positionToBuild)
        {
            //std::cout << "In position to build " << it->building << "\n";
            if (!it->probe->canBuild(it->building))
            {
                it++;
                continue;
            }

            const bool buildSuccess = it->probe->build(it->building, BWAPI::TilePosition(it->positionToBuild));

            //Get new position to build if we cannot build at this place.
            if (!buildSuccess)
            {
                //std::cout << "BUILD UNSUCCESSFUL, trying another spot\n";
                it->positionToBuild = getPositionToBuild(it->building);
                it++;
                continue;
            }
            //std::cout << (it->probe->isConstructing()) << "\n";
            //std::cout << "Build command returned true, constructing...\n";

            //Need to utilize BWEB's reserving tile system to improve building placement even further.
            //BWEB::Map::addUsed(BWAPI::TilePosition(it->positionToBuild), it->building);

            if (it->building == BWAPI::UnitTypes::Protoss_Assimilator)
            {
                //Small chance for assimlator to not be build will try to fix this later.
                it = builders.erase(it);
            }
            else
            {
                it++;
            }
        }
        else
        {
            //Testing this cause the move command has a small chance to fail
            if (it->probe->getOrder() != BWAPI::Orders::Move)
            {
                //std::cout << "Move command failed, trying again\n";
                it->probe->move(it->positionToBuild);
            }

            it++;
        }
    }
}

void SpenderManager::onUnitCreate(BWAPI::Unit unit)
{
    //std::cout << unit->getType() << " created\n";

    for (std::vector<BWAPI::UnitType>::iterator it = plannedBuildings.begin(); it != plannedBuildings.end(); ++it)
    {
        if (unit->getType() == *it)
        {
            //std::cout << "removing " << unit->getType() << " from plannedBuildings\n";
            it = plannedBuildings.erase(it);
            break;
        }
    }

    //Remove worker once a building is being warped in.
    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
    {
        if (unit->getType() == it->building)
        {
            it = builders.erase(it);
            break;
        }
    }

    for (std::vector<BWAPI::UnitType>::iterator it = plannedUnits.begin(); it != plannedUnits.end(); ++it)
    {
        if (unit->getType() == *it)
        {
            //std::cout << "removing " << unit->getType() << " from plannedUnits\n";
            it = plannedUnits.erase(it);
            break;
        }
    }
}

void SpenderManager::onUnitDestroy(BWAPI::Unit unit)
{
    BWEB::Map::onUnitDestroy(unit);

    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end();)
    {
        if (it->probe->getID() == unit->getID())
        {
            const BWAPI::Unit unitAvalible = commanderReference->getUnitToBuild(it->positionToBuild);
            it->probe = unitAvalible;
            break;
        }
        else
        {
            it++;
        }
    }

    //check warping buildings
}

void SpenderManager::onUnitMorph(BWAPI::Unit unit)
{
    BWEB::Map::onUnitMorph(unit);
}

void SpenderManager::onUnitDiscover(BWAPI::Unit unit)
{
    BWEB::Map::onUnitDiscover(unit);
}

bool SpenderManager::buildingAlreadyMadeRequest(int unitID)
{
    for (const int requestIdentifier : requestIdentifiers)
    {
        if (requestIdentifier == unitID) return true;
    }

    return false;
}

void SpenderManager::removeRequestID(int unitID)
{
    for (std::vector<int>::iterator it = requestIdentifiers.begin(); it != requestIdentifiers.end(); ++it)
    {
        if (*it == unitID)
        {
            it = requestIdentifiers.erase(it);
            break;
        }
    }
}

bool SpenderManager::checkUnitIsPlanned(BWAPI::UnitType building)
{
    for (BWAPI::UnitType plannedBuilding : plannedBuildings)
    {
        if (building == plannedBuilding)
        {
            return true;
        }
    }

    return false;
}

bool SpenderManager::checkWorkerIsConstructing(BWAPI::Unit unit)
{
    for (const Builder& builder : builders)
    {
        if (builder.probe->getID() == unit->getID()) return true;
    }

    return false;
}

bool SpenderManager::upgradeAlreadyRequested(BWAPI::Unit building)
{
    for (const BuildRequest buildRequest : buildRequests)
    {
        if (std::holds_alternative<ResearchUpgradeRequest>(buildRequest.request))
        {
            const ResearchUpgradeRequest temp = get<ResearchUpgradeRequest>(buildRequest.request);

            if (temp.building->getID() == building->getID()) return true;
        }
    }

    return false;
}