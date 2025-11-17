#include "ProtoBotCommander.h"
#include "SpenderManager.h"

SpenderManager::SpenderManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
    std::cout << "Spender Manager Made!" << "\n";
    
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

void SpenderManager::addRequest(BWAPI::UpgradeType upgradeToResearch)
{
    BuildRequest requestToAdd;
    ResearchUpgradeRequest temp;
    temp.upgradeType = upgradeToResearch;
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
        if (std::holds_alternative<BuildStructureRequest>(buildRequest.request))
        {
            const BuildStructureRequest buildingInQueue = std::get<BuildStructureRequest>(buildRequest.request);
            std::cout << "Index: " << index << " " << buildingInQueue.buildingType << "\n";
        }
        else if (std::holds_alternative<TrainUnitRequest>(buildRequest.request))
        {
            const TrainUnitRequest unitInQueue = std::get<TrainUnitRequest>(buildRequest.request);
            std::cout << "Index: " << index << " " << unitInQueue.unitType << "\n";
        }
        else if (std::holds_alternative<ResearchUpgradeRequest>(buildRequest.request))
        {
            const ResearchUpgradeRequest upgradeInQueue = std::get<ResearchUpgradeRequest>(buildRequest.request);
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

bool SpenderManager::requestedBuilding(BWAPI::UnitType building)
{
    if (buildRequests.size() == 0)
    {
        return false;
    }

    for (BuildRequest& buildRequest : buildRequests)
    {
        if (std::holds_alternative<BuildStructureRequest>(buildRequest.request))
        {
            const BuildStructureRequest buildingInQueue = std::get<BuildStructureRequest>(buildRequest.request);

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

    //if (BWAPI::Broodwar->getFrameCount() % 48 == 0) printQueue();

    for (std::vector<BuildRequest>::iterator it = buildRequests.begin(); it != buildRequests.end();)
    {
        if (std::holds_alternative<BuildStructureRequest>(it->request))
        {
            const BuildStructureRequest temp = std::get<BuildStructureRequest>(it->request);

            mineralPrice = temp.buildingType.mineralPrice();
            gasPrice = temp.buildingType.gasPrice();

            if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
            {
                //Need to get unit in on frame not here
                BWAPI::Unit unitAvalible = commanderReference->getUnitToBuild();

                if (unitAvalible == nullptr)
                {
                    ++it;
                    continue;
                }

                //Currently this can cause a loop that can pause the game. Although this should be fixed once the build algorithm is made.
                //std::cout << "Unit " << unitAvalible->getID() << " has been assgined to construct " << temp.buildingType << "\n";
                bool success = false;

                if (temp.buildingType != BWAPI::UnitTypes::Protoss_Nexus)
                {
                    success = Tools::BuildBuilding(unitAvalible, temp.buildingType);
                }
                else
                {
                    success = Tools::BuildBuilding(unitAvalible, temp.buildingType);
                }

                //std::cout << "Build Building success? " << success << "\n";
                if (!success)
                {
                    ++it;
                    continue;
                }
                
                plannedBuildings.push_back(temp.buildingType);

                //Need to add units to data structure that keeps track of them until they completed the build.

                currentMineralCount -= mineralPrice;
                currentGasCount -= gasPrice;

                //Why?
                it = buildRequests.erase(it);
                //std::cout << "Building " << temp.buildingType << "\n";
            }
            else
            {
                it++;
            }
        }
        else if (std::holds_alternative<TrainUnitRequest>(it->request))
        {
            const TrainUnitRequest temp = std::get<TrainUnitRequest>(it->request);

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
        else if (std::holds_alternative<ResearchUpgradeRequest>(it->request))
        {
            ResearchUpgradeRequest temp = std::get<ResearchUpgradeRequest>(it->request);

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

    }
}

void SpenderManager::onUnitCreate(BWAPI::Unit unit)
{
    for (std::vector<BWAPI::UnitType>::iterator it = plannedBuildings.begin(); it != plannedBuildings.end(); ++it)
    {
        if (unit->getType() == *it)
        {
            it = plannedBuildings.erase(it);
            break;
        }
    }

    for (std::vector<BWAPI::UnitType>::iterator it = plannedUnits.begin(); it != plannedUnits.end(); ++it)
    {
        if (unit->getType() == *it)
        {
            it = plannedUnits.erase(it);
            break;
        }
    }
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