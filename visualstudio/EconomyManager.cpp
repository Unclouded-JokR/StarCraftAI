#include "EconomyManager.h"
#include "NexusEconomy.h"
#include "ProtoBotCommander.h"

EconomyManager::EconomyManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

void EconomyManager::OnFrame()
{
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        nexusEconomy.OnFrame();
    }
}

void EconomyManager::onUnitDestroy(BWAPI::Unit unit)
{
    if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

    for (std::vector<NexusEconomy>::iterator it = nexusEconomies.begin(); it != nexusEconomies.end(); ++it) {
        //[TODO] fix handling nexus being destroyed. 
        //if (unit->getID() == it->nexus->getID())
        //{
        //    //it = nexusEconomies.erase(it);
        //    break;
        //}

        if (it->OnUnitDestroy(unit) == true) break;
    }
}

void EconomyManager::getWorkersToTransfer(int numberOfWorkers, NexusEconomy& nexusEconomy)
{
    BWAPI::Unitset workersToTransfer = nexusEconomies.at(0).getWorkersToTransfer(numberOfWorkers);

    for (BWAPI::Unit worker : workersToTransfer)
    {
        nexusEconomy.assignWorker(worker);
    }

    std::cout << "New nexus workers: " << nexusEconomy.workers.size() << "\n";
}

void EconomyManager::assignUnit(BWAPI::Unit unit)
{
    switch (unit->getType())
    {
        case BWAPI::UnitTypes::Protoss_Nexus:
        {
            bool alreadyExists = false;
            for (const NexusEconomy& nexusEconomy : nexusEconomies)
            {
                if (nexusEconomy.nexus == unit)
                {
                    alreadyExists = true;
                    break;
                }
            }

            if (alreadyExists == false)
            {
                NexusEconomy temp = NexusEconomy(unit, nexusEconomies.size() + 1, this);
                nexusEconomies.push_back(temp);
            }
            else
            {
                std::cout << "Nexus Already Exists" << "\n";
            }

            break;
        }
        case BWAPI::UnitTypes::Protoss_Assimilator:
        {
            //[TODO] need to verify that this will not assign a assimilator if we are performing a gas steal 
            for (NexusEconomy& nexusEconomy : nexusEconomies)
            {
                if (unit->getDistance(nexusEconomy.nexus->getPosition()) <= 300)
                {
                    nexusEconomy.assignAssimilator(unit);
                    //std::cout << "Assigned Assimilator " << unit->getID() << " to Nexus " << nexusEconomy.nexusID << "\n";
                    break;
                }
            }
            break;
        }
        case BWAPI::UnitTypes::Protoss_Probe:
        {
            for (NexusEconomy& nexusEconomy : nexusEconomies)
            {
                if (unit->getDistance(nexusEconomy.nexus->getPosition()) <= 300)
                {
                    //std::cout << "Assigned Probe " << unit->getID() << " to Nexus " << nexusEconomy.nexusID << "\n";
                    nexusEconomy.assignWorker(unit);
                    break;
                }
            }
            break;
        }
    }
}

std::vector<NexusEconomy> EconomyManager::getNexusEconomies()
{
    return nexusEconomies;
}

//[TODO]: Get the closest worker to the request we are trying to make.
BWAPI::Unit EconomyManager::getAvalibleWorker(BWAPI::Position buildLocation)
{
    BWAPI::Unit closestWorker = nullptr;
    int distance = INT_MAX;

    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        BWAPI::Unit unitToReturn = nexusEconomy.getWorkerToBuild(buildLocation);

        if (unitToReturn == nullptr) continue;

        const int approxDis = buildLocation.getApproxDistance(unitToReturn->getPosition());

        if (approxDis < distance)
        {
            closestWorker = unitToReturn;
            distance = approxDis;
        }
    }

    return closestWorker;
}

BWAPI::Unit EconomyManager::getUnitScout()
{
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        BWAPI::Unit unitToReturn = nexusEconomy.getWorkerToScout();

        if (unitToReturn != nullptr) return unitToReturn;
    }
}

void EconomyManager::needWorkerUnit(BWAPI::UnitType worker, BWAPI::Unit nexus)
{
    commanderReference->requestUnitToTrain(worker, nexus);
}

bool EconomyManager::checkRequestAlreadySent(int unitID)
{
    return commanderReference->alreadySentRequest(unitID);
}

bool EconomyManager::workerIsConstructing(BWAPI::Unit unit)
{
    return commanderReference->checkWorkerIsConstructing(unit);
}