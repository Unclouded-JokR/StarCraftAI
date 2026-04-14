#include "EconomyManager.h"
#include "NexusEconomy.h"
#include "ProtoBotCommander.h"

//hi

EconomyManager::EconomyManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

#pragma region BWAPI EVENTS
void EconomyManager::onStart()
{
    nexusEconomies.clear();
}

void EconomyManager::onFrame()
{
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        nexusEconomy.onFrame();

        if (nexusEconomy.lifetime == 125)
        {
            if (nexusEconomy.nexusID > 1 && nexusEconomy.workers.size() == 0)
            {
                nexusEconomy.workers = getWorkersToTransfer(nexusEconomy.minerals.size(), nexusEconomy);;
                nexusEconomy.assignWorkerBulk();
            }
        }
    }
}

void EconomyManager::onUnitDestroy(BWAPI::Unit unit)
{
    if (unit->getPlayer() == BWAPI::Broodwar->enemy()) return;
    int i = 0;

    //Check which nexus economy the unit is assigned to.
    for (std::vector<NexusEconomy>::iterator it = nexusEconomies.begin(); it != nexusEconomies.end(); ++it) {

        //[TODO] Get assign workers to the closest nexus instead of just main.
        if (unit->getID() == it->nexus->getID())
        {
            //Get the workers at the destroyed nexus and reassign them.
            BWAPI::Unitset nexusEconomyWorkers = it->workers;

            //std::cout << "Nexus size: " << nexusEconomies.size();

            //it = nexusEconomies.erase(it);
            if (nexusEconomies.size() == 1)
            {
                it = nexusEconomies.erase(it);
                break;
            }

            //Assign workers to our "Main Base" index 0 of the Nexus Economies.
            for (BWAPI::Unit worker : nexusEconomyWorkers)
            {
                //nexusEconomies.at(0).assignWorker(worker);
                if (i >= nexusEconomies.size()-1)
                {
                    i = 0;
                }

                if (nexusEconomies.at(i).nexus->getID() != it->nexus->getID())
                {
                    nexusEconomies.at(i).assignWorker(worker);
                }
                else
                {
                    i += 1;
                    nexusEconomies.at(i).assignWorker(worker);
                }
                i += 1;

            }

            it = nexusEconomies.erase(it);

            break;
        }
        else if (it->OnUnitDestroy(unit) == true) break;
    }
}
#pragma endregion

#pragma region Nexus Economy Methods

/*
    This is kinda like OnUnitCreate which would be better to call it but whatever
    
    We currently assign units to a nexus economy based on distance. Could change the way this works but it is functional for now.
*/
void EconomyManager::assignUnit(BWAPI::Unit unit)
{
    if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

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
            getMineralsAtBase(unit->getTilePosition(), temp);
            nexusEconomies.push_back(temp);
            //std::cout << "nexus: " << temp.workers.size();

        }
        else
        {
            //std::cout << "Nexus Already Exists" << "\n";
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


/*
    Transfering of workers from one nexus economy to another.
*/
BWAPI::Unitset EconomyManager::getWorkersToTransfer(int numberOfWorkers, NexusEconomy& nexusEconomyRequest)
{
    BWAPI::Unitset workersToTransfer;
    //Need to check if the size is sufficent for the transer possibly.
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        if (nexusEconomy.nexusID == nexusEconomyRequest.nexusID) continue;

        //Check if a nexus economy has workers to spare.
        if (nexusEconomy.workers.size() != 0)
        {
            workersToTransfer = nexusEconomy.getWorkersToTransfer(numberOfWorkers);

            //for (BWAPI::Unit worker : workersToTransfer)
            //{
                //nexusEconomyRequest.assignWorker(worker);
            //}


            //std::cout << "New nexus workers: " << nexusEconomyRequest.workers.size() << "\n";
            break;
        }
    }

    return workersToTransfer;

}

void EconomyManager::resourcesDepletedTranfer(BWAPI::Unitset workersToTransfer, NexusEconomy& transferFrom)
{
    //std::cout << "Mineral has been depleted at Nexus Economy " << transferFrom.nexusID << ": Transfering " << workersToTransfer.size() << " probes...\n";
    BWAPI::Unitset workersAdded;

    //Search Nexus Economies for open worker slots.
    for (NexusEconomy& nexusEconomy : nexusEconomies)
    {
        if (nexusEconomy.nexusID == transferFrom.nexusID) continue;

        if (nexusEconomy.workers.size()+1 < nexusEconomy.optimalWorkerAmount || (nexusEconomy.workers.size() < nexusEconomy.optimalWorkerAmount && nexusEconomy.nexus->getTrainingQueue().empty()))
        {
            //std::cout << "Moving workers from Nexus Economy " << transferFrom.nexusID << " to Nexus Economy " << nexusEconomy.nexusID << "\n";
            //std::cout << "Nexus Economy " << nexusEconomy.nexusID << " size before: " << nexusEconomy.workers.size() << "\n";

            for (BWAPI::Unit worker : workersToTransfer)
            {
                nexusEconomy.assignWorker(worker);
                workersAdded.insert(worker);

                //std::cout << "Adding worker\n";

                if ((nexusEconomy.workers.size()+1 == nexusEconomy.optimalWorkerAmount && !nexusEconomy.nexus->getTrainingQueue().empty())|| workersAdded.size() == workersToTransfer.size() || nexusEconomy.workers.size() == nexusEconomy.optimalWorkerAmount)
                {
                    break;
                }
            }

            if (workersAdded.size() == workersToTransfer.size()) break;

            //std::cout << "Nexus Economy " << nexusEconomy.nexusID << " size after: " << nexusEconomy.workers.size() << "\n";
        }

        //Remove workers added.
        for (BWAPI::Unit worker : workersAdded)
        {
            workersToTransfer.erase(worker);
        }
        workersAdded.clear();
    }

   

    //std::cout << "Workers left to transfer: " << workersToTransfer.size() << "\n";

    //No nexus economies avalible just send them to the first one that has minerals.
    if (workersToTransfer.size() != 0)
    {
        //std::cout << "No open nexus economies, just transfering them anyway\n";
        for (NexusEconomy& nexusEconomy : nexusEconomies)
        {
            if (nexusEconomy.nexusID == transferFrom.nexusID) continue;

            //std::cout << "Moving workers from Nexus Economy " << transferFrom.nexusID << " to Nexus Economy " << nexusEconomy.nexusID << "\n";

            //std::cout << "Nexus Economy " << nexusEconomy.nexusID << " size before: " << nexusEconomy.workers.size() << "\n";
            for (BWAPI::Unit worker : workersToTransfer)
            {
                nexusEconomy.assignWorker(worker);
                workersAdded.insert(worker);
            }
            //std::cout << "Nexus Economy " << nexusEconomy.nexusID << " size after: " << nexusEconomy.workers.size() << "\n";
            for (BWAPI::Unit worker : workersAdded)
            {
                workersToTransfer.erase(worker);
            }
            workersAdded.clear();
        }
    }

    workersToTransfer.clear();
    workersAdded.clear();
}

//[TODO]: Works but does not factor in travel distance. Need a way to estimate the travel time to get better workers.
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

void EconomyManager::getMineralsAtBase(BWAPI::TilePosition nexusLocation, NexusEconomy& newNexus)
{
    const BWEM::Base* myBase = nullptr;
    bool found = false;

    for (const Area& area : theMap.Areas())
    {
        for (const BWEM::Base& base : area.Bases()) {
            if (base.Location().getApproxDistance(nexusLocation) < 5) {
                myBase = &base;
                found = true;
                break;
            }
        }
        if (found) { break; }
    }

    BWAPI::Unitset mineralsToReturn;
    BWAPI::Unitset geysersToReturn;

    if (myBase)
    {
        for (const auto* mineral : myBase->Minerals()) 
        {
             mineralsToReturn.insert(mineral->Unit());
        }
        newNexus.minerals = mineralsToReturn;

        if (myBase->Geysers().size() != 0)
        {
            for (const auto* geyser : myBase->Geysers())
            {
                if (geyser == nullptr) continue;

                newNexus.vespeneGyser = geyser->Unit();

                BWAPI::Unitset unitsOnGeyser = BWAPI::Broodwar->getUnitsInRectangle(BWAPI::Position(geyser->TopLeft()), BWAPI::Position(geyser->BottomRight()));

                for (BWAPI::Unit unit : unitsOnGeyser)
                {
                    if (unit->getPlayer() == BWAPI::Broodwar->self() && unit->getType().isRefinery() && unit->exists())
                    {
                        newNexus.assimilator = unit;
                    }
                }
            }
        }
    }

    newNexus.optimalWorkerAmount = newNexus.minerals.size() * OPTIMAL_WORKERS_PER_MINERAL;
    newNexus.maximumWorkers = newNexus.optimalWorkerAmount + (newNexus.vespeneGyser != nullptr ? WORKERS_PER_ASSIMILATOR : 0);

    std::cout << "# of Minerals at nexus location: " << newNexus.minerals.size() << "\n";
    std::cout << "# Geyser at nexus location? " << (newNexus.vespeneGyser != nullptr ? "true\n" : "false\n");

    //To keep probe production constant make this to be able to have more workers than nessecary at main or any other base. Transfers should disperse workers.
    newNexus.workerOverflowAmount = (newNexus.minerals.size() * MAXIMUM_WORKERS_PER_MINERAL) + (newNexus.vespeneGyser != nullptr ? WORKERS_PER_ASSIMILATOR : 0);
}

std::vector<NexusEconomy> EconomyManager::getNexusEconomies()
{
    return nexusEconomies;
}
#pragma endregion

#pragma region Commander Requests
bool EconomyManager::checkRequestAlreadySent(int unitID)
{
    return commanderReference->alreadySentRequest(unitID);
}

bool EconomyManager::workerIsConstructing(BWAPI::Unit unit)
{
    return commanderReference->checkWorkerIsConstructing(unit);
}
#pragma endregion