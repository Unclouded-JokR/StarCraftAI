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
    //[TODO]: Need to deconstruct nexusEconomy if its a nexus.
    //End loop early if we found the nexusEconomy that had the destroyed unit
    //NexusEconomy ToBeRemoved = NexusEconomy(unit, nexusEconomies.size() + 1, this);
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {

        if (nexusEconomy.OnUnitDestroy(unit) == true) break;
    }
    //nexusEconomies.erase(*ToBeRemoved);
    //nexusEconomies.erase(std::remove(nexusEconomies.begin(), nexusEconomies.end(), ToBeRemoved), nexusEconomies.end());
    //delete ToBeRemoved;

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
                //[TODO]: Current expansion implementation is not working as intended.
                // Workers should be assigned to the correct nexus economy and mine ONLY the minerals around their nexus.
                //Minerals are not being picked up properlly when we expand and workers are not being transfered.

                NexusEconomy temp = NexusEconomy(unit, nexusEconomies.size() + 1, this);
                nexusEconomies.push_back(temp);

                //[TODO]: Transfer workers from main to the next nexus economy.
                if (nexusEconomies.size() > 1)
                {
                    BWAPI::Unitset workersToTransfer = nexusEconomies.at(0).getWorkersToTransfer(temp.minerals.size());

                    for (BWAPI::Unit worker : workersToTransfer)
                    {
                        temp.assignWorker(worker);
                    }
                    std::cout << "New nexus workers: " << temp.workers.size() << "\n";
                }
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
                if (unit->getDistance(nexusEconomy.nexus->getPosition()) <= 500)
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
                if (unit->getDistance(nexusEconomy.nexus->getPosition()) <= 500)
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

bool EconomyManager::checkAssimilator()
{
    //This will probably need to be improved so we can tell where to build this assimlator instead of true of false.
    //Make the nexus economy call the build manager instead. Should make this super easy then
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        if (nexusEconomy.assimilator != nullptr)
        {
            //Break our of loop if a nexus economy already requested
            return true;
        }
    }
}

//[TODO]: Get the closest worker to the request we are trying to make.
BWAPI::Unit EconomyManager::getAvalibleWorker()
{
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        BWAPI::Unit unitToReturn = nexusEconomy.getWorkerToBuild();

        if (unitToReturn != nullptr) return unitToReturn;
    }
}

BWAPI::Unit EconomyManager::getUnitScout()
{
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        BWAPI::Unit unitToReturn = nexusEconomy.getWorkerToScout();
        //std::cout << "New nexus workers: " << nexusEconomy.workers.size() << "\n";


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

void EconomyManager::destroyedNexus(BWAPI::Unit worker)
{
    if (nexusEconomies.size() > 1)
    {
        nexusEconomies.at(0).assignWorker(worker);
    }
}
