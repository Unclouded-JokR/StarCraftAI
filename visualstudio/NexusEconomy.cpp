#include "NexusEconomy.h"
#include "EconomyManager.h"

NexusEconomy::NexusEconomy(BWAPI::Unit nexus, int id, EconomyManager* economyReference) : nexus(nexus), nexusID(id), economyReference(economyReference)
{
	maximumWorkerPerMineral = OPTIMAL_WORKERS_PER_MINERAL;
	std::cout << "Created new Nexus Economy " << id << "\n";
}

//[TODO] need to need to deconstruct if a nexus dies and send back all the workers.
NexusEconomy::~NexusEconomy()
{

}

void NexusEconomy::addMissedResources()
{
	maximumWorkerPerMineral = OPTIMAL_WORKERS_PER_MINERAL;

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
		mineralWorkerCount[mineral] = 0;
	}

	optimalWorkerAmount = minerals.size() * OPTIMAL_WORKERS_PER_MINERAL;
	maximumWorkerAmount = minerals.size() * MAXIMUM_WORKERS_PER_MINERAL;
	maximumWorkers = optimalWorkerAmount + WORKERS_PER_ASSIMILATOR;

	if (nexusID > 1) economyReference->getWorkersToTransfer(newMinerals.size(), *this);
	//std::cout << "Total Minerals: " << minerals.size() << "\n";
}

void NexusEconomy::OnFrame()
{
	lifetime += 1;
	if (lifetime >= 500) lifetime = 500;

	//make this solution better.
	addMissedResources();

	/*
		===========================
				   Debug
		===========================
	*/
	const int left = nexus->getPosition().x - 300;
	const int top = nexus->getPosition().y - 300;
	const int right = nexus->getPosition().x + 300;
	const int bottom = nexus->getPosition().y + 300;
	BWAPI::Broodwar->drawBoxMap(BWAPI::Position(left, top), BWAPI::Position(right, bottom), BWAPI::Color(255, 0, 0));

	for (BWAPI::Unit mineral : minerals)
	{
		BWAPI::Broodwar->drawLineMap(nexus->getPosition(), mineral->getPosition(), BWAPI::Color(0, 0, 255));
	}

	if (assimilator != nullptr) BWAPI::Broodwar->drawLineMap(nexus->getPosition(), assimilator->getPosition(), BWAPI::Color(255, 255, 0));

	if (vespeneGyser != nullptr) BWAPI::Broodwar->drawLineMap(nexus->getPosition(), vespeneGyser->getPosition(), BWAPI::Color(144, 238, 144));

	std::string temp = "Nexus ID: " + std::to_string(nexus->getID()) + "\n" + "Worker Size : " + std::to_string(workers.size()) + "\nMinerals Assigned : " + std::to_string(minerals.size());
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
		if (minerals.size() == 0) continue;

		//If a worker is constructing skip over them until they are done.
		//Add method to check if a worker is being used to place a building if isConstructing is not working.
		if (worker->getOrder() == BWAPI::Orders::Move || worker->isConstructing())
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
				assignedResource[worker] = vespeneGyser;
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

	//Train unit if we are not at optimal worker count
	if (!nexus->isTraining() && workers.size() < maximumWorkers && !economyReference->checkRequestAlreadySent(nexus->getID()))
	{
		economyReference->needWorkerUnit(BWAPI::UnitTypes::Protoss_Probe, nexus);
	}
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
	//Unit is not ours, return
	if (unit->getPlayer() != BWAPI::Broodwar->self())
		return false;

	if (unit->getType().isWorker() && workers.find(unit) != workers.end())
	{
		BWAPI::Unit resource = assignedResource[unit];

		//Check if worker is assigned to mineral or gas
		if (resource == vespeneGyser)
		{
			assimilatorWorkerCount -= 1;
			
		}
		else
		{
			mineralWorkerCount[resource] -= 1;
		}

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

		if (assignedMineral != vespeneGyser)
		{
			mineralWorkerCount[assignedMineral] -= 1;
			assignedResource.erase(unit);
			//workers.erase(unit);
			unitsToReturn.insert(unit);
		}

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
BWAPI::Unit NexusEconomy::getWorkerToBuild(BWAPI::Position locationToBuild)
{
	if (workers.size() == 0) return nullptr;

	BWAPI::Unit unitToReturn = nullptr;
	int minDistance = INT_MAX;

	//Get closest idle units if possible.
	for (const BWAPI::Unit unit : workers)
	{
		const int distance = locationToBuild.getApproxDistance(unit->getPosition());

		if (unit->isIdle() && distance < minDistance)
		{
			minDistance = distance;

			//Only take workers that are mining mineral patches. Workers assigned to gysers are never deassgined, unless destoryed.
			if (assignedResource.find(unit) != assignedResource.end() && assignedResource[unit] != vespeneGyser)
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
		}
	}

	//If idle unit is avalible return, otherwise search for the second best option
	if (unitToReturn != nullptr)
	{
		return unitToReturn;
	}

	minDistance = INT_MAX;
	for (const BWAPI::Unit unit : workers)
	{
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
		const int distance = locationToBuild.getApproxDistance(unit->getPosition());

		if (distance < minDistance)
		{
			BWAPI::Unit assignedMineral = assignedResource[unit];
			mineralWorkerCount[assignedMineral] -= 1;
			assignedResource.erase(unit);
			unitToReturn = unit;
		}
	}
	return unitToReturn;
}