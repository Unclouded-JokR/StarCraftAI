#include "ProtoBotCommander.h"
#include "SpenderManager.h"

SpenderManager::SpenderManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
}

/*
    Current requests that can be added:
        - Buildings
        - Upgrades (some)
        - Training units

    Upgrades is "some" since BWAPI defines some of the data types for abilities as Research instead of Upgrades.
    Future modifications need to be made to spender manager to make sure we can recieve other different request types and be able to handle it. 1/19/26
*/
#pragma region Add Requests Methods
void SpenderManager::addRequest(BWAPI::UnitType building)
{
    BuildRequest requestToAdd;
    BuildStructureRequest buildingRequest;
    buildingRequest.buildingType = building;

    requestToAdd.request = buildingRequest;
    buildRequests.push_back(requestToAdd);
}

void SpenderManager::addRequest(BWAPI::Unit unit, BWAPI::UpgradeType upgradeToResearch)
{
    BuildRequest requestToAdd;
    ResearchUpgradeRequest researchRequest;
    researchRequest.upgradeType = upgradeToResearch;
    researchRequest.building = unit;

    requestToAdd.request = researchRequest;
    buildRequests.push_back(requestToAdd);
}

void SpenderManager::addRequest(BWAPI::UnitType unit, BWAPI::TilePosition location, BWAPI::Unit requester)
{
    BuildRequest requestToAdd;
    UnitBuildStructureRequest buildRequest;
    buildRequest.worker = requester;
    buildRequest.buildingType = unit;
    buildRequest.locationToBuild = location;

    requestToAdd.request = buildRequest;
    buildRequests.push_back(requestToAdd);
}

void SpenderManager::addRequest(BWAPI::UnitType unit, BWAPI::Unit requester)
{
    BuildRequest requestToAdd;
    TrainUnitRequest trainRequest;
    trainRequest.unitType = unit;
    trainRequest.building = requester;

    //Add unit to already sent requests
    requestIdentifiers.push_back(requester->getID());

    requestToAdd.request = trainRequest;
    buildRequests.push_back(requestToAdd);
}
#pragma endregion

#pragma region Debug Statements
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
#pragma endregion

#pragma region Helper Methods
int SpenderManager::availableMinerals()
{
    int currentMineralCount = BWAPI::Broodwar->self()->minerals();

    //Only need to factor in buildings since mineral and upgrade spending happens immedietly.
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

int SpenderManager::plannedSupply()
{
    const int totalSupply = BWAPI::Broodwar->self()->supplyTotal() / 2; //Current supply total StarCraft notes us having.
    int usedSupply = BWAPI::Broodwar->self()->supplyUsed() / 2; //Used supply total StarCraft notes us having.
    int plannedUsedSuply = 0; //Supply we plan on using (Units are in the build queue but have not been accepted yet.
    int plannedSupply = 0; //Buildings we plan on constructing that provide supply. 

    for (BWAPI::UnitType unit : plannedUnits)
    {
        usedSupply += (unit.supplyRequired() / 2);
    }

    /*
        Add buildings that provide supply currently in the queue to plannedSupply.
        Add units that use supply to plannedUsedSupply 
    */
    for (BuildRequest buildRequest : buildRequests)
    {
        if (std::holds_alternative<BuildStructureRequest>(buildRequest.request))
        {
            const BuildStructureRequest buildStrctureRequest = get<BuildStructureRequest>(buildRequest.request);

            if (buildStrctureRequest.buildingType == BWAPI::UnitTypes::Protoss_Pylon
                || buildStrctureRequest.buildingType == BWAPI::UnitTypes::Protoss_Nexus)
            {
                plannedSupply += buildStrctureRequest.buildingType.supplyProvided() / 2;
            }
        }
        else if (std::holds_alternative<TrainUnitRequest>(buildRequest.request))
        {
            const TrainUnitRequest buildStrctureRequest = get<TrainUnitRequest>(buildRequest.request);

            plannedUsedSuply += buildStrctureRequest.unitType.supplyRequired() / 2;
        }
    }

    //Building request has been accepted it has just not been built yet.
    for (const BWAPI::UnitType building : plannedBuildings)
    {
        //if (building.supplyProvided() / 2 > 0) std::cout << "Planned Building " << building << ", provides " << building.supplyProvided() / 2 << " supply\n";

        plannedSupply += building.supplyProvided() / 2;
    }

    //Building is not being warped in.
    for (const BWAPI::Unit unit : incompleteBuildings)
    {
        //if (unit->getType().supplyProvided() / 2 > 0) std::cout << "Incomplete Building " << unit->getType() << ", provides " << unit->getType().supplyProvided() / 2 << " supply\n";

        plannedSupply += unit->getType().supplyProvided() / 2;
    }

    /*std::cout << "Total supply (with planned building considerations) = " << (totalSupply + plannedSupply) << "\n";
    std::cout << "Used supply (with planned unit considerations) = " << usedSupply << "\n";
    std::cout << "StarCraft Supply avalible = " << (totalSupply - usedSupply) << "\n";*/
    return ((totalSupply + plannedSupply) - (usedSupply + plannedUsedSuply));
}

bool SpenderManager::canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas)
{
    if (((currentMinerals - mineralPrice) >= 0) && ((currentGas - gasPrice) >= 0))
    {
        return true;
    }

    return false;
}
#pragma endregion


#pragma region BWAPI Events
void SpenderManager::onStart()
{
    std::cout << "Spender Manager Initialized" << "\n";
    builders.clear();
    buildRequests.clear();
    plannedBuildings.clear();
    plannedUnits.clear();
    requestIdentifiers.clear();
    incompleteBuildings.clear();
}

void SpenderManager::OnFrame()
{
    int currentMineralCount = availableMinerals();
    int currentGasCount = availableGas();
    int currentSupply = availableSupply();

    int mineralPrice = 0;
    int gasPrice = 0;

    //if (BWAPI::Broodwar->getFrameCount() % 256) std::cout << "Planned supply test:\n" << plannedSupply() << "\n";

    //if (BWAPI::Broodwar->getFrameCount() % 96 == 0)
    //{
    //    //printQueue();
    //    /*for (const BWAPI::UnitType building : plannedBuildings)
    //    {
    //        std::cout << "Planning to build: " << building << "\n";
    //    }*/
    //}

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
                ////Prioritize exapnsions and pylons 
                //if (temp.buildingType == BWAPI::UnitTypes::Protoss_Nexus || temp.buildingType == BWAPI::UnitTypes::Protoss_Pylon)
                //{
                //    break;
                //}
                //else
                //{
                //    it++;
                //}

                it++;
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
        else if (std::holds_alternative<UnitBuildStructureRequest>(it->request))
        {
            const UnitBuildStructureRequest temp = get<UnitBuildStructureRequest>(it->request);

            mineralPrice = temp.buildingType.mineralPrice();
            gasPrice = temp.buildingType.gasPrice();

            if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount) && ((currentSupply - (temp.buildingType.supplyRequired() / 2)) >= 0))
            {
                //Send back acknowledgement to scout that they are allowed to spend minerals.
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
    if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

    //std::cout << unit->getType() << " created\n";
    if (unit->getType().isBuilding() && !unit->isCompleted()) incompleteBuildings.insert(unit);

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

void SpenderManager::onUnitComplete(BWAPI::Unit unit)
{
    if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

    //std::cout << unit->getType() << " completed\n";

    if (!unit->getType().isBuilding()) return;

    //Might cause error if the unit was not added possibly.
    std::cout << "Removing " << unit->getType() << "\n";
    incompleteBuildings.erase(unit);
}

void SpenderManager::onUnitMorph(BWAPI::Unit unit)
{
    BWEB::Map::onUnitMorph(unit);
}

void SpenderManager::onUnitDiscover(BWAPI::Unit unit)
{
    BWEB::Map::onUnitDiscover(unit);
}
#pragma endregion

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