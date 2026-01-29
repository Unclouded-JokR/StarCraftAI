#include "NexusEconomy.h"
#include "EconomyManager.h"

NexusEconomy::NexusEconomy(BWAPI::Unit nexus, int id, EconomyManager* economyReference) : nexus(nexus), nexusID(id), economyReference(economyReference)
{
	//std::cout << "Created new Nexus Economy " << id << "\n";
}

//[TODO] need to need to deconstruct if a nexus dies and send back all the workers.
NexusEconomy::~NexusEconomy()
{

}

void NexusEconomy::addMissedResources()
{
	const int left = nexus->getPosition().x - 300;
	const int top = nexus->getPosition().y - 300;
	const int right = nexus->getPosition().x + 300;
	const int bottom = nexus->getPosition().y + 300;

	BWAPI::Unitset resourcesInRectangle = BWAPI::Broodwar->getUnitsInRectangle(BWAPI::Position(left, top), BWAPI::Position(right, bottom));
	BWAPI::Unitset newMinerals;

	//Should be better way to do this but this works for now.
	for (const BWAPI::Unit unit : resourcesInRectangle)
	{
		if (vespeneGyser == nullptr && unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
		{
			vespeneGyser = unit;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field
			|| unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field_Type_2
			|| unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field_Type_3)
		{
			bool alreadyInserted = false;
			if (minerals.size() != 0)
			{
				for (BWAPI::Unit mineral : minerals)
				{
					if (mineral->getID() == unit->getID())
						alreadyInserted = true;
				}

				if (alreadyInserted == false)
				{
					newMinerals.insert(unit);
				}
			}
			else
			{
				newMinerals.insert(unit);
			}
		}
	}

	if (newMinerals.size() == 0) return;

	for (BWAPI::Unit mineral : newMinerals)
	{
		minerals.insert(mineral);
		resourceWorkerCount[mineral] = 0;
	}

	optimalWorkerAmount = minerals.size() * OPTIMAL_WORKERS_PER_MINERAL;
	maximumWorkers = optimalWorkerAmount + (vespeneGyser != nullptr ? WORKERS_PER_ASSIMILATOR : 0);

	if (nexusID > 1) economyReference->getWorkersToTransfer(newMinerals.size(), *this);
	//std::cout << "Total Minerals: " << minerals.size() << "\n";
}

void NexusEconomy::onFrame()
{
	lifetime += 1;
	if (lifetime >= 500) lifetime = 500;

	//make this solution better.
	if (lifetime < 500) addMissedResources();

	/*
		===========================
				   Debug
		===========================
	*/
	/*const int left = nexus->getPosition().x - 300;
	const int top = nexus->getPosition().y - 300;
	const int right = nexus->getPosition().x + 300;
	const int bottom = nexus->getPosition().y + 300;
	BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Color(255, 0, 0));*/

	for (BWAPI::Unit mineral : minerals)
	{
		BWAPI::Broodwar->drawLineMap(nexus->getPosition(), mineral->getPosition(), BWAPI::Color(0, 0, 255));
		BWAPI::Broodwar->drawTextMap(mineral->getPosition(), std::to_string(resourceWorkerCount[mineral]).c_str());
	}

	if (assimilator != nullptr) BWAPI::Broodwar->drawLineMap(nexus->getPosition(), assimilator->getPosition(), BWAPI::Color(255, 255, 0));

	if (vespeneGyser != nullptr) BWAPI::Broodwar->drawLineMap(nexus->getPosition(), vespeneGyser->getPosition(), BWAPI::Color(144, 238, 144));

	std::string temp = "Nexus Economy " + std::to_string(nexusID) + "\n" + "Worker Size : " + std::to_string(workers.size()) + "\nMinerals : " + std::to_string(minerals.size());
	BWAPI::Broodwar->drawTextMap(BWAPI::Position(nexus->getPosition().x, nexus->getPosition().y + 40), temp.c_str());

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

	/*
		===========================
				 Main loop
		===========================
	*/
	for (BWAPI::Unit worker : workers)
	{
		BWAPI::Broodwar->drawTextMap(worker->getPosition(), std::to_string(worker->getID()).c_str());

		if (minerals.size() == 0) break;

		//If a worker is constructing skip over them until they are done.
		if (economyReference->workerIsConstructing(worker) || worker->isConstructing() || worker->getOrder() == BWAPI::Orders::Move)
		{
			BWAPI::Broodwar->drawEllipseMap(worker->getPosition(), 3, 3, BWAPI::Color(0, 0, 255), true);
			continue;
		}

		//If a worker is not carrying minerals and hasnt been assigned to one, assign them to farm. 
		if (!worker->isCarryingMinerals() || worker->isIdle() || !worker->isConstructing())
		{
			if (assimilator != nullptr && resourceWorkerCount[assimilator] < WORKERS_PER_ASSIMILATOR)
			{
				worker->gather(assimilator);
				assignedResource[worker] = vespeneGyser;
				resourceWorkerCount[assimilator] += 1;
			}
			else if (assignedResource.find(worker) == assignedResource.end())
			{
				BWAPI::Unit closestMineral = GetClosestMineralToWorker(worker);
				resourceWorkerCount[closestMineral] += 1;
				assignedResource[worker] = closestMineral;
				worker->gather(closestMineral);
			}

			//BWAPI::Broodwar->drawEllipseMap(worker->getPosition(), 3, 3, BWAPI::Color(255, 0, 0), true);
		}
		else
		{
			if (assignedResource.find(worker) != assignedResource.end())
			{
				BWAPI::Unit assignedMineral = assignedResource[worker];
				assignedResource.erase(worker);
				resourceWorkerCount[assignedMineral] -= 1;
				worker->rightClick(nexus);
			}

			//BWAPI::Broodwar->drawEllipseMap(worker->getPosition(), 3, 3, BWAPI::Color(0, 128, 0), true);
		}
	}


	//Train unit if we are not at optimal worker count
	if (!nexus->isTraining()
		&& workers.size() < ((OPTIMAL_WORKERS_PER_MINERAL * minerals.size()) + (vespeneGyser != nullptr ? WORKERS_PER_ASSIMILATOR : 0))
		&& !economyReference->checkRequestAlreadySent(nexus->getID()))
	{
		economyReference->needWorkerUnit(BWAPI::UnitTypes::Protoss_Probe, nexus);
	}
}

void NexusEconomy::printMineralWorkerCounts()
{
	for (BWAPI::Unit mineral : minerals)
	{
		std::cout << "Mineral " << mineral->getID() << " Count: " << resourceWorkerCount[mineral] << "\n";
	}
}

//Return true if the unit was apart of instance of NexusEconomy
//[TODO]: also need to figure out edge case when unit destroyed is a nexus
bool NexusEconomy::OnUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getType().isWorker() && workers.find(unit) != workers.end())
	{
		BWAPI::Unit resource = assignedResource[unit];

		//Check if worker is assigned to mineral or gas, MAKE THIS BETTER
		if (resource == vespeneGyser)
		{
			resourceWorkerCount[assimilator] -= 1;

		}
		else
		{
			resourceWorkerCount[resource] -= 1;
		}

		assignedResource.erase(unit);
		workers.erase(unit);
		return true;
	}
	else if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field && minerals.find(unit) != minerals.end())
	{
		resourceWorkerCount[unit] = 0;
		minerals.erase(unit);

		BWAPI::Unitset workersToRelocate;
		for (BWAPI::Unit worker : workers)
		{
			if (workersToRelocate.size() == OPTIMAL_WORKERS_PER_MINERAL) break;

			if (assignedResource.find(worker) == assignedResource.end())
			{
				workersToRelocate.insert(worker);
			}
			else if (assignedResource.find(worker) != assignedResource.end() && assignedResource[worker] != vespeneGyser)
			{
				BWAPI::Unit mineral = assignedResource[worker];
				resourceWorkerCount[mineral] -= 1;
				workersToRelocate.insert(worker);
			}
		}

		//std::cout << "Worker size before: " << workers.size() << "\n";
		for (BWAPI::Unit worker : workersToRelocate)
		{
			workers.erase(worker);
		}
		//std::cout << "Worker size after: " << workers.size() << "\n";

		economyReference->resourcesDepletedTranfer(workersToRelocate, *this);

		return true;
	}
	else if (unit->getType() == BWAPI::UnitTypes::Protoss_Assimilator && assimilator != nullptr && unit == assimilator)
	{
		std::cout << "Assimilator destroyed...\n";
		assimilator = nullptr;
		resourceWorkerCount[assimilator] = 0;
		return true;
	}
	else if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser && vespeneGyser != nullptr && unit == vespeneGyser)
	{
		std::cout << "Gyser Depleted...\n";
		//assimilator = nullptr;
		resourceWorkerCount[assimilator] = 0;
		return true;
	}

	return false;
}

//[TODO] Change this to A*
BWAPI::Unit NexusEconomy::GetClosestMineralToWorker(BWAPI::Unit worker)
{
	//Check if we have a mineral patch that has no workers
	BWAPI::Unit closestMineral = nullptr;
	int closestMineralDistance = 99999;

	for (BWAPI::Unit mineral : minerals)
	{
		int distanceFromWorker = worker->getDistance(mineral);

		if (resourceWorkerCount[mineral] == 0 && distanceFromWorker < closestMineralDistance)
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
			const int workersAssignedToMineral = resourceWorkerCount[mineral];

			if (resourceWorkerCount[mineral] < OPTIMAL_WORKERS_PER_MINERAL && distanceFromWorker < closestMineralDistance)
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
}

void NexusEconomy::assignAssimilator(BWAPI::Unit assimilator)
{
	this->assimilator = assimilator;
	resourceWorkerCount[assimilator] = 0;
}

//[TODO]: make sure we are handing off probe properlly
BWAPI::Unitset NexusEconomy::getWorkersToTransfer(int numberOfWorkersForTransfer)
{
	BWAPI::Unitset unitsToReturn;

	//Get idle units if possible.
	for (BWAPI::Unit unit : workers)
	{
		if (unit->isIdle())
		{
			if (assignedResource.find(unit) != assignedResource.end())
			{
				BWAPI::Unit assignedMineral = assignedResource[unit];
				resourceWorkerCount[assignedMineral] -= 1;
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

		if (assignedMineral != vespeneGyser)
		{
			resourceWorkerCount[assignedMineral] -= 1;
			assignedResource.erase(unit);
			//workers.erase(unit);
			unitsToReturn.insert(unit);
		}

		if (unitsToReturn.size() == numberOfWorkersForTransfer) break;
	}

	//std::cout << "Got " << unitsToReturn.size() << " Workers\n";

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
				resourceWorkerCount[assignedMineral] -= 1;
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
			resourceWorkerCount[assignedMineral] -= 1;
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
BWAPI::Unit NexusEconomy::getWorkerToBuild(BWAPI::Position locationToBuild)
{
	if (workers.size() == 0) return nullptr;

	BWAPI::Unit unitToReturn = nullptr;
	int minDistance = INT_MAX;

	//Get closest idle units if possible.
	for (const BWAPI::Unit unit : workers)
	{
		if (economyReference->workerIsConstructing(unit)) continue;

		const int distance = locationToBuild.getApproxDistance(unit->getPosition());

		if (unit->isIdle() && distance < minDistance)
		{
			minDistance = distance;

			//Only take workers that are mining mineral patches. Workers assigned to gysers are never deassgined, unless destoryed.
			if (assignedResource.find(unit) != assignedResource.end() && assignedResource[unit] != vespeneGyser)
			{
				unitToReturn = unit;
			}
		}
	}

	//If idle unit is avalible return, otherwise search for the second best option
	if (unitToReturn != nullptr)
	{
		if (assignedResource.find(unitToReturn) != assignedResource.end())
		{
			BWAPI::Unit assignedMineral = assignedResource[unitToReturn];
			//resourceWorkerCount[assignedMineral] -= 1;
			//assignedResource.erase(unitToReturn);
		}

		return unitToReturn;
	}

	minDistance = INT_MAX;
	for (const BWAPI::Unit unit : workers)
	{
		if (economyReference->workerIsConstructing(unit)) continue;

		const int distance = locationToBuild.getApproxDistance(unit->getPosition());

		if (unit->isCarryingMinerals() && distance < minDistance)
		{
			minDistance = distance;
			unitToReturn = unit;
		}
	}

	if (unitToReturn != nullptr)
	{
		//std::cout << "Unit Carrying Minerals " << unitToReturn->getID() << " Avalible!\n";
		return unitToReturn;
	}


	minDistance = INT_MAX;
	for (const BWAPI::Unit unit : workers)
	{
		if (economyReference->workerIsConstructing(unit)) continue;

		const int distance = locationToBuild.getApproxDistance(unit->getPosition());

		if (distance < minDistance && assignedResource.find(unitToReturn) != assignedResource.end() && assignedResource[unit] != vespeneGyser)
		{
			unitToReturn = unit;
		}
	}

	if (assignedResource.find(unitToReturn) != assignedResource.end())
	{
		BWAPI::Unit assignedMineral = assignedResource[unitToReturn];
		//resourceWorkerCount[assignedMineral] -= 1;
		//assignedResource.erase(unitToReturn);
	}
	return unitToReturn;
}