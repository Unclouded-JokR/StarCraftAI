#include "NexusEconomy.h"
#include "EconomyManager.h"
#include "ProtoBotCommander.h"

NexusEconomy::NexusEconomy(BWAPI::Unit nexus, int id, EconomyManager* economyReference) : nexus(nexus), nexusID(id), economyReference(economyReference)
{
	//std::cout << "Created new Nexus Economy " << id << "\n";
}

//[TODO] need to need to deconstruct if a nexus dies and send back all the workers.
NexusEconomy::~NexusEconomy()
{

}

int NexusEconomy::addMissedResources()
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

		if (unit->getType().isRefinery() && !unit->isBeingConstructed() && unit->getPlayer() != BWAPI::Broodwar->self()) {
			std::cout << "Gas steal detected...\n";
			for (auto u : workers)
			{
				if (u->getType().isWorker() && attackingWorkers < 8) {
					u->attack(unit);
					attackingWorkers += 1;
				}
			}
			attackingWorkers = 0;
		}

		if (unit->isAttacking() && unit->getPlayer() != BWAPI::Broodwar->self()) {
			for (auto u : workers)
			{
				if (u->getType().isWorker() && attackingWorkers < 8) {
					u->attack(unit);
					attackingWorkers += 1;
				}
			}
			attackingWorkers = 0;
		}

		/*for (auto& u : BWAPI::Broodwar->enemy()->getUnits()) {
			if (u->getType().isRefinery() && u->getPlayer() != BWAPI::Broodwar->self()) {
				for (auto u : workers)
				{
					if (u->getType().isWorker() && attackingWorkers < 8) {
						u->attack(unit);
						attackingWorkers += 1;
					}
				}
			}
		}*/
	}

	if (newMinerals.size() == 0) return minerals.size();

	for (BWAPI::Unit mineral : newMinerals)
	{
		minerals.insert(mineral);
		resourceWorkerCount[mineral] = 0;
	}

	optimalWorkerAmount = minerals.size() * OPTIMAL_WORKERS_PER_MINERAL;
	maximumWorkers = optimalWorkerAmount + (vespeneGyser != nullptr ? WORKERS_PER_ASSIMILATOR : 0);

	return minerals.size();

	/*
	if (nexusID > 1)
	{
		BWAPI::Unitset incomingWorkers = economyReference->getWorkersToTransfer(newMinerals.size(), *this);

		for (BWAPI::Unit worker : incomingWorkers)
		{
			assignWorker(worker);
		}
	}*/

	//std::cout << "Total Minerals: " << minerals.size() << "\n";
}

/*
void NexusEconomy::testGasSteal()
{
	BWAPI::Unit enemyWorker = nullptr;
	for (auto& u : BWAPI::Broodwar->enemy()->getUnits()) {
		if (u->getType().isWorker()) {
			enemyWorker = u;
			break;
		}
	}


	if (buildPos != BWAPI::TilePositions::None) {
		enemyWorker->build(buildPos, BWAPI::UnitTypes::Protoss_Pylon);
	}
}
*/

void NexusEconomy::onFrame()
{

	lifetime += 1;
	if (lifetime >= 500) lifetime = 500;
	moveTimer += 1;

	if (lifetime == 500 && BWAPI::Broodwar->getFrameCount() % 150 == 0)
	{
		int kill = addMissedResources();
	}

	//make this solution better.
	if (lifetime < 500) int y = addMissedResources();



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

	//Problem start here

	
	/*for (BWAPI::Unit mineral : minerals)
	{
		BWAPI::Broodwar->drawLineMap(nexus->getPosition(), mineral->getPosition(), BWAPI::Color(0, 0, 255));
		BWAPI::Broodwar->drawTextMap(mineral->getPosition(), std::to_string(resourceWorkerCount[mineral]).c_str());
	}

	if (assimilator != nullptr) BWAPI::Broodwar->drawLineMap(nexus->getPosition(), assimilator->getPosition(), BWAPI::Color(255, 255, 0));

	if (vespeneGyser != nullptr) BWAPI::Broodwar->drawLineMap(nexus->getPosition(), vespeneGyser->getPosition(), BWAPI::Color(144, 238, 144));
	*/
	//std::string temp = "Nexus Economy " + std::to_string(nexusID) + "\n" + "Worker Size : " + std::to_string(workers.size()) + "\nMinerals : " + std::to_string(minerals.size());
	//BWAPI::Broodwar->drawTextMap(BWAPI::Position(nexus->getPosition().x, nexus->getPosition().y + 40), temp.c_str());
	

	//Problem end here

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

	//economyReference->commanderReference->timerManager.stopTimer(TimerManager::test);


	for (BWAPI::Unit worker : workers)
	{
		//if (worker->isIdle()) workerOrder[worker] = 0;
		BWAPI::Broodwar->drawTextMap(worker->getPosition(), std::to_string(worker->getID()).c_str());

		if (minerals.size() == 0) break;

		//If a worker is constructing skip over them until they are done.
		if (economyReference->workerIsConstructing(worker) || worker->isConstructing() || worker->isMoving())
		{
			//BWAPI::Broodwar->drawEllipseMap(worker->getPosition(), 3, 3, BWAPI::Color(0, 0, 255), true);
			continue;
		}


		//If a worker is not carrying minerals and hasnt been assigned to one, assign them to farm. 
		if (!worker->isCarryingMinerals())
		{
			if (assimilator != nullptr && resourceWorkerCount[assimilator] < WORKERS_PER_ASSIMILATOR)
			{
				if (assignedResource[worker]) {
					resourceWorkerCount[assignedResource[worker]] -= 1;
				}

				worker->gather(assimilator);
				assignedResource[worker] = vespeneGyser;
				resourceWorkerCount[assimilator] += 1;


			}
			else if (assignedResource.find(worker) == assignedResource.end())
			{
				//std::cout << "reached this point\n";
				BWAPI::Unit closestMineral = GetClosestMineralToWorker(worker);
				if (closestMineral) {
					resourceWorkerCount[closestMineral] += 1;
					assignedResource[worker] = closestMineral;

					worker->gather(closestMineral);


				}

			}

		}

		if (worker->getOrder() == BWAPI::Orders::MoveToMinerals ||
			worker->getOrder() == BWAPI::Orders::WaitForMinerals || worker->isIdle())
		{
			if (worker->getOrderTarget() != assignedResource[worker])
			{
				worker->rightClick(assignedResource[worker]);
			}
			//continue;
		}





		if (worker->isCarryingMinerals() && worker->isIdle())
		{
			worker->stop(); // breaks BW mining AI
		}
		if (worker->isCarryingMinerals() && worker->isIdle())
		{
			BWAPI::Unit assignedMineral = assignedResource[worker];
			//assignedResource.erase(worker);
			//resourceWorkerCount[assignedMineral] -= 1;
			worker->move(nexus->getPosition());
			worker->rightClick(nexus);
		}


	}




	//Train unit if we are not at optimal worker count
	if (!nexus->isTraining()
		&& workers.size() < ((OPTIMAL_WORKERS_PER_MINERAL * minerals.size()) + (vespeneGyser != nullptr ? WORKERS_PER_ASSIMILATOR : 0))
		&& !economyReference->checkRequestAlreadySent(nexus->getID()))
	{
		economyReference->needWorkerUnit(BWAPI::UnitTypes::Protoss_Probe, nexus);
	}

	if (moveTimer >= 5)
	{
		moveTimer = 0;
	}


	//if (BWAPI::Broodwar->getFrameCount() % 500 == 0)
	//{
	//	std::cout << BWAPI::Broodwar->getFrameCount() << " test: " << BWAPI::Broodwar->self()->gatheredMinerals() << "\n";
	//}



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
		for (auto u : workers)
		{
			if (assignedResource[u] == vespeneGyser)
			{
				assignedResource.erase(u);
			}
		}
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
	workerOrder[unit] = 0;
	//std::cout << "Worker " << unit->getID() << " assigned to nexus " << nexus->getID() << "\n";
}

void NexusEconomy::assignAssimilator(BWAPI::Unit assimilator)
{
	/*if (assimilator->getType().isRefinery() && assimilator->getPlayer() != BWAPI::Broodwar->self()) {
		// Check if unit is within your main base region
			//BWAPI::Broodwar->printf("Gas steal detected!");
		std::cout << "Gas steal detected...\n";
		int attackingWorkers = 0;
		for (auto u : workers)
		{
			if (u->getType().isWorker() && attackingWorkers < 5) {
				u->attack(assimilator);
				attackingWorkers += 1;
			}
		}
	}*/
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
				//BWAPI::Unit assignedMineral = assignedResource[unit];
				//resourceWorkerCount[assignedMineral] -= 1;
				//assignedResource.erase(unit);
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
			workerOrder[unit] = 5;
			//BWAPI::Unit assignedMineral = assignedResource[unit];
			workers.erase(unit);
			resourceWorkerCount[assignedResource[unit]] -= 1;
			assignedResource.erase(unit);
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
			//BWAPI::Unit assignedMineral = assignedResource[unit];
			//resourceWorkerCount[assignedMineral] -= 1;
			//assignedResource.erase(unit);
			//workers.erase(unit);
			unitsToReturn.insert(unit);
		}

		if (unitsToReturn.size() == numberOfWorkersForTransfer) break;
	}

	if (unitsToReturn.size() == numberOfWorkersForTransfer)
	{
		for (BWAPI::Unit unit : unitsToReturn)
		{
			workerOrder[unit] = 5;
			workers.erase(unit);
			resourceWorkerCount[assignedResource[unit]] -= 1;
			assignedResource.erase(unit);
			//BWAPI::Unit assignedMineral = assignedResource[unit];
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
			//resourceWorkerCount[assignedMineral] -= 1;
			//assignedResource.erase(unit);
			//workers.erase(unit);
			unitsToReturn.insert(unit);
		}

		if (unitsToReturn.size() == numberOfWorkersForTransfer) break;
	}

	//std::cout << "Got " << unitsToReturn.size() << " Workers\n";

	for (BWAPI::Unit unit : unitsToReturn)
	{
		workerOrder[unit] = 5;
		workers.erase(unit);
		resourceWorkerCount[assignedResource[unit]] -= 1;
		assignedResource.erase(unit);
		//BWAPI::Unit assignedMineral = assignedResource[unit];
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
	std::cout << "Request for Worker scouts!\n";
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
				//resourceWorkerCount[assignedMineral] -= 1;
				//assignedResource.erase(unit);
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
		workerOrder[unitToReturn] = 3;
		workers.erase(unitToReturn);
		resourceWorkerCount[assignedResource[unitToReturn]] -= 1;
		assignedResource.erase(unitToReturn);
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
		resourceWorkerCount[assignedResource[unitToReturn]] -= 1;
		assignedResource.erase(unitToReturn);
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
			//resourceWorkerCount[assignedMineral] -= 1;
			unitToReturn = unit;
			//assignedResource.erase(unit);
			//workers.erase(unit);
			break;
		}

		index++;
	}

	workers.erase(unitToReturn);
	resourceWorkerCount[assignedResource[unitToReturn]] -= 1;
	assignedResource.erase(unitToReturn);
	std::cout << "Reuqesting Worker scouts!\n";
	return unitToReturn;
}

//[TODO]: make sure we are handing off probe properlly
BWAPI::Unit NexusEconomy::getWorkerToBuild(BWAPI::Position locationToBuild)
{

	BWAPI::Unitset closestWorkers;
	if (closestMinerals.size() == 0)
	{
		int largeDist = 999;
		int compare = 0;
		BWAPI::Unit appended;
		for (int ii = 0; ii < 5; ii++)
		{
			for (auto u : minerals)
			{
				//compare = nexus->getPosition().getApproxDistance(u->getPosition());
				compare = nexus->getPosition().x-u->getPosition().x;
				if (compare < largeDist && !closestMinerals.contains(u))
				{
					appended = u;
				}
			}

			closestMinerals.insert(appended);
		}
	}
	
	
	for (const BWAPI::Unit unit : workers)
	{
		if (closestMinerals.contains(assignedResource[unit]))
		{
			closestWorkers.insert(unit);
		}
	}
	//economyReference->commanderReference->timerManager.startTimer(TimerManager::Testing);

	if (closestWorkers.size() == 0) return nullptr;

	BWAPI::Unit unitToReturn = nullptr;
	int minDistance = INT_MAX;

	//Get closest idle units if possible.
	for (const BWAPI::Unit unit : closestWorkers)
	{
		if (economyReference->workerIsConstructing(unit)) continue;

		//const int distance = locationToBuild.getApproxDistance(unit->getPosition());
		int distance = 0;
		theMap.GetPath(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), BWAPI::Position(locationToBuild), &distance);


		if (unit->isIdle() && distance < minDistance)
		{
			minDistance = distance;

			//Only take workers that are mining mineral patches. Workers assigned to gysers are never deassgined, unless destoryed.
			if (assignedResource.find(unit) != assignedResource.end() && assignedResource[unit] != vespeneGyser)
			{
				unitToReturn = unit;
				std::cout << unit->getID() << " is going to build!" << "\n";
			}
		}
	}

	//economyReference->commanderReference->timerManager.stopTimer(TimerManager::Testing);
	//std::cout << "time to build: " << economyReference->commanderReference->timerManager.getTotalElapsedTest() << " From m1\n";

	//If idle unit is avalible return, otherwise search for the second best option
	if (unitToReturn != nullptr)
	{
		if (assignedResource.find(unitToReturn) != assignedResource.end())
		{
			BWAPI::Unit assignedMineral = assignedResource[unitToReturn];
			//resourceWorkerCount[assignedMineral] -= 1;
			//assignedResource.erase(unitToReturn);
		}

		workerOrder[unitToReturn] = 2;
		return unitToReturn;
	}

	minDistance = INT_MAX;
	for (const BWAPI::Unit unit : closestWorkers)
	{
		if (economyReference->workerIsConstructing(unit)) continue;

		//const int distance = locationToBuild.getApproxDistance(unit->getPosition());
		int distance = 0;
		theMap.GetPath(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), BWAPI::Position(locationToBuild), &distance);

		if (unit->isCarryingMinerals() && distance < minDistance)
		{
			minDistance = distance;
			unitToReturn = unit;
		}
	}

	
	//economyReference->commanderReference->timerManager.stopTimer(TimerManager::Testing);
	//std::cout << "time to build: " << economyReference->commanderReference->timerManager.getTotalElapsedTest() << " from m2\n";

	if (unitToReturn != nullptr)
	{
		//std::cout << "Unit Carrying Minerals " << unitToReturn->getID() << " Avalible!\n";
		workerOrder[unitToReturn] = 2;
		return unitToReturn;
	}


	minDistance = INT_MAX;
	for (const BWAPI::Unit unit : closestWorkers)
	{
		if (economyReference->workerIsConstructing(unit)) continue;

		//const int distance = locationToBuild.getApproxDistance(unit->getPosition());
		int distance = 0;
		theMap.GetPath(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), BWAPI::Position(locationToBuild), &distance);

		if (distance < minDistance && assignedResource.find(unitToReturn) != assignedResource.end() && assignedResource[unit] != vespeneGyser)
		{
			unitToReturn = unit;
			std::cout << unit->getID() << " is going to build!" << "\n";
		}
	}

	if (assignedResource.find(unitToReturn) != assignedResource.end())
	{
		BWAPI::Unit assignedMineral = assignedResource[unitToReturn];
		//resourceWorkerCount[assignedMineral] -= 1;
		//assignedResource.erase(unitToReturn);
	}
	workerOrder[unitToReturn] = 2;

	//economyReference->commanderReference->timerManager.stopTimer(TimerManager::Testing);
	//std::cout << "time to build: " << economyReference->commanderReference->timerManager.getTotalElapsedTest() << " from m3\n";



	return unitToReturn;
}

//map = maps/BroodWar/aiide/(?)*.sc?

/*
if (unit->getType().isRefinery() && unit->getPlayer() != Broodwar->self()) {
    // Check if unit is within your main base region
    if (unit->getTilePosition().getDistance(yourBaseLocation) < 15) {
        Broodwar->printf("Gas steal detected!");
    }
}
*/

//ai     = ../bin/StarterBot.dll
//ai_dbg = .. / bin / StarterBot_d.dll