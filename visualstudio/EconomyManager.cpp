#include "EconomyManager.h"

EconomyManager::EconomyManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

void EconomyManager::OnFrame()
{
    for (const auto& unit : assignedWorkers)
    {
        if (unit.second->isIdle())
        {
            assigned[unit.first] = 0;
        }
        

    }

    const BWAPI::Unitset& myUnits = BWAPI::Broodwar->self()->getUnits();
    for (auto& unit : myUnits)
    {
        // Check the unit type, if it is an idle worker, then we want to send it somewhere
        if (unit->getType().isWorker() && unit->isIdle())
        {
            // Get the closest mineral to this worker unit
            BWAPI::Unit closestMineral = GetClosestUnitToWOWorker(unit, BWAPI::Broodwar->getMinerals());

            // If a valid mineral was found, right click it with the unit in order to start harvesting
            if (closestMineral) 
            {  
                unit->rightClick(closestMineral); 
                assignedWorkers[closestMineral] = unit;
            }
        }
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

}

BWAPI::Unit EconomyManager::getAvalibleWorker()
{
    return nullptr;
}