#pragma once
#include "SpenderManager.h"
#include "BuildManager.h"

SpenderManager::SpenderManager()
{

}
#pragma region Debug Statements
void SpenderManager::printQueue()
{
   
}
#pragma endregion

#pragma region Helper Methods
int SpenderManager::availableMinerals(std::vector<ResourceRequest> &requests)
{
    int currentMineralCount = BWAPI::Broodwar->self()->minerals();

    for (const ResourceRequest &request : requests)
    {
        //Sanity Check
        if (request.state == ResourceRequest::State::Accepted_Completed ||
            request.state == ResourceRequest::State::PendingApproval) continue;

        if (request.type == ResourceRequest::Type::Building) currentMineralCount -= request.unit.mineralPrice();
    }

    return currentMineralCount;
}

int SpenderManager::availableGas(std::vector<ResourceRequest> &requests)
{
    int currentGasCount = BWAPI::Broodwar->self()->gas();

    for (const ResourceRequest &request : requests)
    {
        //Sanity Check
        if (request.state == ResourceRequest::State::Accepted_Completed ||
            request.state == ResourceRequest::State::PendingApproval) continue;

        if(request.type == ResourceRequest::Type::Building) currentGasCount -= request.unit.gasPrice();
    }

    return currentGasCount;
}

//[TODO] consider making this take into consideration the time it will take to arrive at a place. 
int SpenderManager::availableSupply()
{
    //int unusedSupply = (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed()) / 2;

    return (BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed()) / 2;;
}

int SpenderManager::plannedSupply(std::vector<ResourceRequest> &requests, BWAPI::Unitset buildings)
{
    const int totalSupply = BWAPI::Broodwar->self()->supplyTotal() / 2; //Current supply total StarCraft notes us having.
    const int usedSupply = BWAPI::Broodwar->self()->supplyUsed() / 2; //Used supply total StarCraft notes us having.

    int plannedUsedSuply = 0; //Supply we plan on using (Units are in the build queue but have not been accepted yet.
    int plannedSupply = 0; //Buildings we plan on constructing that provide supply. 

    for (const ResourceRequest &request : requests)
    {
        //Sanity Check
        if (request.state == ResourceRequest::State::Accepted_Completed) continue;


        switch (request.type)
        {
            case ResourceRequest::Type::Unit:
                plannedUsedSuply += (request.unit.supplyRequired() / 2);
                break;
            case ResourceRequest::Type::Building:
                plannedSupply += request.unit.supplyProvided() / 2;
                break;
        }
    }

    //Consider buildings that are not constructed yet for potential supply.
    for (const BWAPI::Unit building : buildings)
    {
        if (!building->isCompleted())
        {
            plannedSupply += building->getType().supplyProvided() / 2;
        }
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
    
}

void SpenderManager::OnFrame(std::vector<ResourceRequest> &requests)
{
    //Calculate avalible minerals to spend. This considers the minerals we plan to spend as well.
    int currentMineralCount = availableMinerals(requests);
    int currentGasCount = availableGas(requests);

    //Need to modify spender manager to be able to consider supply usage.
    int currentSupply = availableSupply();

    int mineralPrice = 0;
    int gasPrice = 0;

    for (ResourceRequest& request : requests)
    {
        if (request.state != ResourceRequest::State::PendingApproval) continue;

        switch (request.type)
        {
            case ResourceRequest::Type::Unit:
                mineralPrice = request.unit.mineralPrice();
                gasPrice = request.unit.gasPrice();

                if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
                {
                    request.state = ResourceRequest::State::Approved_InProgress;
                    currentMineralCount -= mineralPrice;
                    currentGasCount -= gasPrice;
                }
                break;
            case ResourceRequest::Type::Building:
                mineralPrice = request.unit.mineralPrice();
                gasPrice = request.unit.gasPrice();

                if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
                {
                    request.state = ResourceRequest::State::Approved_InProgress;
                    currentMineralCount -= mineralPrice;
                    currentGasCount -= gasPrice;
                }
                break;
            case ResourceRequest::Type::Upgrade:
                mineralPrice = request.upgrade.mineralPrice();
                gasPrice = request.upgrade.gasPrice();

                if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
                {
                    request.state = ResourceRequest::State::Approved_InProgress;
                    currentMineralCount -= mineralPrice;
                    currentGasCount -= gasPrice;
                }
                break;
            case ResourceRequest::Type::Tech:
                mineralPrice = request.tech.mineralPrice();
                gasPrice = request.tech.gasPrice();

                if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
                {
                    request.state = ResourceRequest::State::Approved_InProgress;
                    currentMineralCount -= mineralPrice;
                    currentGasCount -= gasPrice;
                }
                break;
        }

        break;
    }
}
#pragma endregion