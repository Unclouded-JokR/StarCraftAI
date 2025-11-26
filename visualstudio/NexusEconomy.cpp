#include "NexusEconomy.h"
#include "EconomyManager.h"

NexusEconomy::NexusEconomy(BWAPI::Unit nexus, int id, EconomyManager* economyReference) : nexus(nexus), nexusID(id), economyReference(economyReference)
{
	maximumWorkerPerMineral = OPTIMAL_WORKERS_PER_MINERAL;

	BWAPI::Unitset unitsInRadius = nexus->getUnitsInRadius(320, BWAPI::Filter::IsMineralField);

	for (BWAPI::Unit unit : unitsInRadius)
	{
		//std::cout << unit->getType() << "\n";
		std::cout << "getting minerals\n";
		minerals.insert(unit);
	}

	optimalWorkerAmount = minerals.size() * OPTIMAL_WORKERS_PER_MINERAL;
	maximumWorkerAmount = minerals.size() * MAXIMUM_WORKERS_PER_MINERAL;
	maximumWorkers = optimalWorkerAmount + WORKERS_PER_ASSIMILATOR;

	for (BWAPI::Unit mineral : minerals)
	{
		mineralWorkerCount[mineral] = 0;
	}

	std::cout << "Created new Nexus Economy " << id << "\n";
	std::cout << "Total Minerals: " << minerals.size() << "\n";

	red = rand() % 256;
	blue = rand() % 256;
	green = rand() % 256;
}

//[TODO] need to need to deconstruct if a nexus dies and send back all the workers.
NexusEconomy::~NexusEconomy()
{

}

void NexusEconomy::OnFrame()
{


	for (BWAPI::Unit mineral : minerals)
	{
		BWAPI::Broodwar->drawTextMap(mineral->getPosition(), std::to_string(mineral->getID()).c_str());
		BWAPI::Broodwar->drawEllipseMap(mineral->getPosition(), 2, 2, BWAPI::Color(red, blue, green), true);

	}

	if (assimilator != nullptr) BWAPI::Broodwar->drawTextMap(assimilator->getPosition(), std::to_string(assimilator->getID()).c_str());
	
	if (nexus != nullptr) 
	{
		std::string temp = "Nexus ID: " + std::to_string(nexus->getID()) + "\n" + "Worker Size : " + std::to_string(workers.size());
		BWAPI::Broodwar->drawTextMap(nexus->getPosition(), temp.c_str());
	}

	for (BWAPI::Unit worker : workers)
	{
		BWAPI::Broodwar->drawTextMap(worker->getPosition(), std::to_string(worker->getID()).c_str());

		//If a worker is constructing skip over them until they are done.
		if (worker->isConstructing())
		{
			BWAPI::Broodwar->drawEllipseMap(worker->getPosition(), 2, 2, BWAPI::Color(0, 0, 255), true);
			continue;
		}

		//If a worker is not carrying minerals and hasnt been assigned to one, assign them to farm. 
		if (!worker->isCarryingMinerals() || worker->isIdle())
		{
			if (assimilator != nullptr && assimilatorWorkerCount < WORKERS_PER_ASSIMILATOR)
			{
				worker->gather(assimilator);
				assimilatorWorkerCount += 1;
				continue;
			}

			if (assignedResource.find(worker) == assignedResource.end())
			{
				BWAPI::Unit closestMineral = GetClosestMineralToWorker(worker);
				mineralWorkerCount[closestMineral] += 1;
				assignedResource[worker] = closestMineral;
				worker->gather(closestMineral);
			}

			BWAPI::Broodwar->drawEllipseMap(worker->getPosition(), 2, 2, BWAPI::Color(255, 0, 0), true);
		}
		else
		{
			if (assignedResource.find(worker) != assignedResource.end())
			{
				BWAPI::Unit assignedMineral = assignedResource[worker];
				assignedResource.erase(worker);
				mineralWorkerCount[assignedMineral] -= 1;
				worker->rightClick(nexus);
			}

			BWAPI::Broodwar->drawEllipseMap(worker->getPosition(), 2, 2, BWAPI::Color(0, 128, 0), true);
		}
	}

	//if (frame % 48 == 0) std::cout << "Nexus already sent request == " << !economyReference->checkRequestAlreadySent(nexus->getID()) << "\n";
	if (nexus != nullptr)
	{
		if (!nexus->isTraining() && workers.size() < maximumWorkers && !economyReference->checkRequestAlreadySent(nexus->getID()))
		{
			economyReference->needWorkerUnit(BWAPI::UnitTypes::Protoss_Probe, nexus);
		}
	}

	//Debug Print Statements
	/*if (BWAPI::Broodwar->getFrameCount() % 500 == 0 && BWAPI::Broodwar->getFrameCount() != 0)
	{
		std::cout << "Frame: " << BWAPI::Broodwar->getFrameCount() << " Minerals Gathered: " << BWAPI::Broodwar->self()->gatheredMinerals() << "\n";
	}
	else if(BWAPI::Broodwar->getFrameCount() == 0)
	{
		std::cout << "Frame: " << BWAPI::Broodwar->getFrameCount() << " Minerals Gathered: " << 0 << "\n";
	}*/

	/*if (BWAPI::Broodwar->getFrameCount() % 48 == 0)
	{
		std::cout << "===== MineralAssignments =====\n";
		printMineralWorkerCounts();
	}*/
}

void NexusEconomy::printMineralWorkerCounts()
{
	for (BWAPI::Unit mineral : minerals)
	{
		std::cout << "Mineral " << mineral->getID() << " Count: " << mineralWorkerCount[mineral] << "\n";
	}
}

//Return true if the unit was apart of instance of NexusEconomy
//[TODO]: need to figure out what to do with units once a mineral patch or gas is done.
//[TODO]: also need to figure out edge case when unit destroyed is a nexus
bool NexusEconomy::OnUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self())
		return false;

	if (unit->getType().isWorker() && workers.find(unit) != workers.end())
	{
		BWAPI::Unit assignedMineral = assignedResource[unit];
		mineralWorkerCount[assignedMineral] -= 1;
		assignedResource.erase(unit);
		workers.erase(unit);
	}
	else if (unit->getType().isResourceContainer())
	{
		if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
		{
			assimilator = nullptr;
			assimilatorWorkerCount = 0;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field && minerals.find(unit) != minerals.end())
		{
			mineralWorkerCount[unit] = 0;
			minerals.erase(unit);
		}
	}
	else if (unit->getType() == BWAPI::UnitTypes::Protoss_Assimilator && (unit != assimilator || assimilator == nullptr))
	{
		assimilator = nullptr;
		assimilatorWorkerCount = 0;
	}
	else if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus)
	{
		nexus = nullptr;
		BWAPI::Unitset allWorkers;
		for (BWAPI::Unit unit1 : workers)
		{
			allWorkers.insert(unit1);
		}

		for (BWAPI::Unit unit2 : allWorkers)
		{
			workers.erase(unit2);
		}

		economyReference->destroyedNexus(allWorkers);

		return true;
	}
}

BWAPI::Unit NexusEconomy::GetClosestMineralToWorker(BWAPI::Unit worker)
{
	//Check if we have a mineral patch that has no workers
	BWAPI::Unit closestMineral = nullptr;
	int closestMineralDistance = 99999;

	for (BWAPI::Unit mineral : minerals)
	{
		int distanceFromWorker = worker->getDistance(mineral);

		if (mineralWorkerCount[mineral] == 0 && distanceFromWorker < closestMineralDistance)
		{
			closestMineralDistance = distanceFromWorker;
			closestMineral = mineral;
		}
	}

	if (closestMineral == nullptr)
	{
		for (BWAPI::Unit mineral : minerals)
		{
			const int distanceFromWorker = worker->getDistance(mineral);
			const int workersAssignedToMineral = mineralWorkerCount[mineral];

			if (mineralWorkerCount[mineral] < maximumWorkerPerMineral && distanceFromWorker < closestMineralDistance)
			{
				closestMineralDistance = distanceFromWorker;
				closestMineral = mineral;
			}
		}
	}

	return closestMineral;
}

void NexusEconomy::assignWorker(BWAPI::Unit unit)
{
	workers.insert(unit);
	std::cout << "added worker " << unit->getID() << " to " << std::to_string(nexus->getID()) << "\n";
	std::cout << "worker size of  " + std::to_string(nexus->getID()) + " is " + std::to_string(workers.size()) + "\n";

}

void NexusEconomy::assignAssimilator(BWAPI::Unit assimilator)
{
	this->assimilator = assimilator;
	assimilatorWorkerCount = 0;
}

void NexusEconomy::workOverTime()
{
	maximumWorkers = maximumWorkerAmount;
	maximumWorkerPerMineral = OPTIMAL_WORKERS_PER_MINERAL + WORKERS_PER_ASSIMILATOR;
}

//[TODO]: Hand workers to combat or scout when we get out of rage.
void NexusEconomy::breakTime()
{
	maximumWorkers = optimalWorkerAmount;
	maximumWorkerPerMineral = MAXIMUM_WORKERS_PER_MINERAL + WORKERS_PER_ASSIMILATOR;
}

//[TODO]: make sure we are handing off probe properlly
BWAPI::Unitset NexusEconomy::getWorkersToTransfer(int numberOfWorkersForTransfer)
{
	BWAPI::Unitset unitsToReturn;
	//BWAPI::Unitset unitsToBeRemoved;

	//Get idle units if possible.
	for (BWAPI::Unit unit : workers)
	{
		if (unit->isIdle())
		{
			if (assignedResource.find(unit) != assignedResource.end())
			{
				BWAPI::Unit assignedMineral = assignedResource[unit];
				mineralWorkerCount[assignedMineral] -= 1;
				assignedResource.erase(unit);
				//workers.erase(unit);
				unitsToReturn.insert(unit);
			}
			else
			{
				//workers.erase(unit);
				unitsToReturn.insert(unit);
			}
		}

		if (unitsToReturn.size() == numberOfWorkersForTransfer) break;
	}

	if (unitsToReturn.size() == numberOfWorkersForTransfer)
	{
		for (BWAPI::Unit unit : unitsToReturn)
		{
			workers.erase(unit);
		}
/*
		for (BWAPI::Unit unit : workers)
		{
			std::cout << "worker " << unit->getID() << "\n";
		}*/


		return unitsToReturn;

	}

	//Choose random unit if we did not find unit.

	for (BWAPI::Unit unit : workers)
	{
		if (unit->isCarryingMinerals())
		{
			/*BWAPI::Unit assignedMineral = assignedResource[unit];
			mineralWorkerCount[assignedMineral] -= 1;
			assignedResource.erase(unit);*/
			//workers.erase(unit);
			unitsToReturn.insert(unit);
		}

		if (unitsToReturn.size() == numberOfWorkersForTransfer) break;
	}

	if (unitsToReturn.size() == numberOfWorkersForTransfer)
	{
		for (BWAPI::Unit unit : unitsToReturn)
		{
			workers.erase(unit);
		}
		/*
		for (BWAPI::Unit unit : workers)
		{
			std::cout << "worker " << unit->getID() << "\n";
		}

		*/
		return unitsToReturn;
	}

	//If not enough units we found, get units until we meet the number of workers needs
	for (BWAPI::Unit unit : workers)
	{
		BWAPI::Unit assignedMineral = assignedResource[unit];
		mineralWorkerCount[assignedMineral] -= 1;
		assignedResource.erase(unit);
		//workers.erase(unit);
		unitsToReturn.insert(unit);

		if (unitsToReturn.size() == numberOfWorkersForTransfer) break;
	}

	std::cout << "Got " << unitsToReturn.size() << " Workers\n";

	for (BWAPI::Unit unit : unitsToReturn)
	{
		workers.erase(unit);
	}

	/*
	for (BWAPI::Unit unit : workers)
	{
		std::cout << "worker " << unit->getID() << "\n";
	}
	*/

	return unitsToReturn;
}

//[TODO]: make sure we are handing off probe properlly
BWAPI::Unit NexusEconomy::getWorkerToScout()
{
	if (workers.size() == 0) return nullptr;

	BWAPI::Unit unitToReturn = nullptr;

	//Get idle units if possible.
	for (BWAPI::Unit unit : workers)
	{

		if (unit->isIdle())
		{
			if (assignedResource.find(unit) != assignedResource.end())
			{
				BWAPI::Unit assignedMineral = assignedResource[unit];
				mineralWorkerCount[assignedMineral] -= 1;
				assignedResource.erase(unit);
				unitToReturn = unit;
			}
			else
			{
				unitToReturn = unit;
			}
			//workers.erase(unit);
			break;
		}
	}

	if (unitToReturn != nullptr)
	{
		workers.erase(unitToReturn);
		std::cout << "Reuqesting Worker scouts!\n";
		return unitToReturn;
	}
		

	//Choose random unit if we did not find unit.

	for (BWAPI::Unit unit : workers)
	{
		if (unit->isCarryingMinerals())
		{
			unitToReturn = unit;
			break;
		}
		//workers.erase(unit);
	}

	if (unitToReturn != nullptr)
	{
		workers.erase(unitToReturn);
		std::cout << "Reuqesting Worker scouts!\n";
		return unitToReturn;
	}

	//If not unit is avalible that meets prior conditions choose unit randomly for now.
	const int random = rand() % workers.size();
	std::cout << "Random Index Choosen: " << random << "\n";

	int index = 0;
	for (BWAPI::Unit unit : workers)
	{
		if (index == random)
		{
			BWAPI::Unit assignedMineral = assignedResource[unit];
			mineralWorkerCount[assignedMineral] -= 1;
			unitToReturn = unit;
			assignedResource.erase(unit);
			//workers.erase(unit);
			break;
		}

		index++;
	}

	workers.erase(unitToReturn);
	std::cout << "Reuqesting Worker scouts!\n";
	return unitToReturn;
}

//[TODO]: make sure we are handing off probe properlly
BWAPI::Unit NexusEconomy::getWorkerToBuild()
{
	if (workers.size() == 0) return nullptr;

	std::cout << "Reuqesting Worker!\n";


	BWAPI::Unit unitToReturn = nullptr;

	//Get idle units if possible.
	for (BWAPI::Unit unit : workers)
	{
		if (unit->isIdle())
		{
			if (assignedResource.find(unit) != assignedResource.end())
			{
				BWAPI::Unit assignedMineral = assignedResource[unit];
				mineralWorkerCount[assignedMineral] -= 1;
				assignedResource.erase(unit);
				unitToReturn = unit;
			}
			else
			{
				unitToReturn = unit;
			}
			break;
		}
	}

	if (unitToReturn != nullptr)
	{
		//std::cout << "Idle Unit " << unitToReturn->getID() << " Avalible!\n";
		return unitToReturn;
	}

	//Choose random unit if we did not find unit.

	for (BWAPI::Unit unit : workers)
	{
		if (unit->isCarryingMinerals())
		{
			unitToReturn = unit;
			break;
		}
	}

	if (unitToReturn != nullptr)
	{
		//std::cout << "Unit Carrying Minerals " << unitToReturn->getID() << " Avalible!\n";
		return unitToReturn;
	}

	//If not unit is avalible that meets prior conditions choose unit randomly for now.
	const int random = rand() % workers.size();
	//std::cout << "Random Index Choosen: " << random << "\n";

	int index = 0;
	for (BWAPI::Unit unit : workers)
	{
		if (index == random)
		{
			BWAPI::Unit assignedMineral = assignedResource[unit];
			mineralWorkerCount[assignedMineral] -= 1;
			assignedResource.erase(unit);
			unitToReturn = unit;
			break;
		}

		index++;
	}

	//std::cout << "Random Unit " << unitToReturn->getID() << " Avalible!\n";
	return unitToReturn;
}