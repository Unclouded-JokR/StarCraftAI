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

BWAPI::Unit EconomyManager::GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units)
{
    BWAPI::Unit closestUnitWOWorker = nullptr;

    for (auto& u : units)
    {
        if (assigned.count(u) == 0)
        {
            assigned[u] = 0;
        }
    }

    for (auto& u : units)
    {
        if (!closestUnitWOWorker || (u->getDistance(p) < closestUnitWOWorker->getDistance(p) && assigned[u] == 0))
        {
            closestUnitWOWorker = u;
        }
    }

    assigned[closestUnitWOWorker] = 1;
    //assignedWorkers[closestUnitWOWorker]
    return closestUnitWOWorker;
}

BWAPI::Unit EconomyManager::GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units)
{
    if (!unit) { return nullptr; }
    return GetClosestUnitToWOWorker(unit->getPosition(), units);
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
                nexusEconomies.push_back(NexusEconomy(unit, nexusEconomies.size() + 1));
            }
            else
            {
                std::cout << "Nexus Already Exists" << "\n";
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
                    std::cout << "Assigned Assimilator " << unit->getID() << " to Nexus " << nexusEconomy.nexusID << "\n";
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
                    std::cout << "Assigned Probe " << unit->getID() << " to Nexus " << nexusEconomy.nexusID << "\n";
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
    const BWAPI::Unitset& myUnits = BWAPI::Broodwar->self()->getUnits();

    for (auto& unit : myUnits)
    {
        if (unit->getType().isWorker())
        {
            return unit;
        }
    }
}