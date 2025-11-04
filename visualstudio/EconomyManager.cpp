#include "EconomyManager.h"
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
    //End loop early if we found the nexusEconomy that had the destroyed unit
    //[TODO]: Need to deconstruct nexusEconomy if its a nexus.
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        if (nexusEconomy.OnUnitDestroy(unit) == true) break;
    }
}

void EconomyManager::assignUnit(BWAPI::Unit unit)
{
    switch (unit->getType())
    {
        case BWAPI::UnitTypes::Protoss_Nexus:
        {
            bool alreadyExists = false;
            for (NexusEconomy& nexusEconomy : nexusEconomies)
            {
                if (nexusEconomy.nexus == unit)
                {
                    alreadyExists = true;
                    break;
                }
            }

            if (alreadyExists == false)
            {
                //[TODO]: Also need to transfer workers to expansion.
                nexusEconomies.push_back(NexusEconomy(unit, nexusEconomies.size() + 1));
            }
            else
            {
                //std::cout << "Nexus Already Exists" << "\n";
            }
               
            break;
        }
        case BWAPI::UnitTypes::Protoss_Assimilator:
        {
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

BWAPI::Unit EconomyManager::getAvalibleWorker()
{
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        BWAPI::Unit unitToReturn = nexusEconomy.getWorkerToBuild();

        if(unitToReturn != nullptr) return unitToReturn;
    }
}