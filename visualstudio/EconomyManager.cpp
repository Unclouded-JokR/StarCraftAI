#include "EconomyManager.h"
#include "ProtoBotCommander.h"

EconomyManager::EconomyManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

void EconomyManager::OnFrame()
{
    for (const auto& unit : assignedWorkers)
    {
        for (const auto& un : unit.second)
        {
            if (un->isIdle())
            {
                assigned[unit.first]--;
            }
        }

    }
    
    //I will replace myUnits with availableWorkers here
    const BWAPI::Unitset& myUnits = BWAPI::Broodwar->self()->getUnits();

    if(myUnits.size() >= BWAPI::Broodwar->getMinerals().size() * workers_per_hs)
    {
        workers_per_hs++;
    }

    
    for (auto& unit : myUnits)
    {
        // Check the unit type, if it is an idle worker, then we want to send it somewhere
        if (unit->getType().isWorker() && unit->isIdle())
        {
            // Get the closest mineral to this worker unit
            BWAPI::Unit closestMineral = GetClosestUnitToWOWorker(unit, BWAPI::Broodwar->getMinerals(), workers_per_hs);

            // If a valid mineral was found, right click it with the unit in order to start harvesting
            if (closestMineral) 
            {  
                unit->rightClick(closestMineral); 
                assignedWorkers[closestMineral].push_back(unit);
            }
        }
    }
}

BWAPI::Unit EconomyManager::GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units, int workers_from_com)
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
        if (!closestUnitWOWorker || (u->getDistance(p) < closestUnitWOWorker->getDistance(p) && assigned[u] < workers_from_com))
        {
            closestUnitWOWorker = u;
        }
    }

    assigned[closestUnitWOWorker]++;
    //assignedWorkers[closestUnitWOWorker]
    return closestUnitWOWorker;
}

BWAPI::Unit EconomyManager::GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units, int workers_from_com)
{
    if (!unit) { return nullptr; }
    return GetClosestUnitToWOWorker(unit->getPosition(), units, workers_from_com);
}

void EconomyManager::assignUnit(BWAPI::Unit unit)
{
    if (unit->getType().isWorker())
    {
        available_workers.push_back(unit);
    }

}

BWAPI::Unit EconomyManager::getAvalibleWorker()
{
    return nullptr;
}