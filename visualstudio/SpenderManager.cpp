#include "ProtoBotCommander.h"
#include "SpenderManager.h"

SpenderManager::SpenderManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
    std::cout << "Spender Manager Made!" << "\n";
}

void SpenderManager::addRequest(BWAPI::UnitType unitType)
{
    BuildRequest requestToAdd;
    BuildStructureRequest temp;
    temp.buildingType = unitType;
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
    requestToAdd.request = temp;

    buildRequests.push_back(requestToAdd);
}

bool SpenderManager::canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas)
{
    if (((currentMinerals - mineralPrice) >= 0) && ((currentGas - gasPrice) >= 0))
    {
        return true;
    }

    return false;
}

bool SpenderManager::isAlreadyBuildingSupply()
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

            if (buildingInQueue.buildingType == BWAPI::UnitTypes::Protoss_Pylon)
            {
                std::cout << "Pylon in queue" << "\n";
                return true;
            }
        }
    } 

    std::cout << "Pylon not in Queue" << "\n";
    return false;
}

void SpenderManager::OnFrame()
{
    int currentMineralCount = BWAPI::Broodwar->self()->minerals();
    int currentGasCount = BWAPI::Broodwar->self()->gas();

    int mineralPrice = 0;
    int gasPrice = 0;

    if(BWAPI::Broodwar->getFrameCount() % 48 == 0) std::cout << "Requests.size() =  " << buildRequests.size() << "\n";

    for (std::vector<BuildRequest>::iterator it = buildRequests.begin(); it != buildRequests.end();)
    {
        if (holds_alternative<BuildStructureRequest>(it->request))
        {
            const BuildStructureRequest temp = get<BuildStructureRequest>(it->request);

            mineralPrice = temp.buildingType.mineralPrice();
            gasPrice = temp.buildingType.gasPrice();

            if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
            {
                BWAPI::Unit unitAvalible = commanderReference->getUnitToBuild();
                Tools::BuildBuilding(unitAvalible, temp.buildingType);

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
        else if(holds_alternative<TrainUnitRequest>(it->request))
        {
            const TrainUnitRequest temp = get<TrainUnitRequest>(it->request);

            mineralPrice = temp.unitType.mineralPrice();
            gasPrice = temp.unitType.gasPrice();

            if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
            {
                temp.building->train(temp.unitType);

                currentMineralCount -= mineralPrice;
                currentGasCount -= gasPrice;

                //Why?
                it = buildRequests.erase(it);
                //std::cout << "Training " << temp.unitType << "\n";
            }
            else
            {
                it++;
            }
        }
        else if (holds_alternative<ResearchUpgradeRequest>(it->request))
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

    }
}