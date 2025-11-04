#pragma once
#include "../src/starterbot/Tools.h"
#include <BWAPI.h>
#include <vector>
#include <unordered_map>

#define WORKERS_PER_ASSIMILATOR 3
#define OPTIMAL_WORKERS_PER_MINERAL 2
#define MAXIMUM_WORKERS_PER_MINERAL 3

class ProtoBotCommander;

class NexusEconomy
{
public:
	const int nexusID;
	BWAPI::Unit nexus;
	BWAPI::Unitset workers;
	BWAPI::Unitset minerals;
	BWAPI::Unit assimilator = nullptr;
	std::unordered_map<BWAPI::Unit, int> mineralWorkerCount;
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedResource;
	int assimilatorWorkerCount = 0;

	//Calculated maximums based on number of miners.
	int maximumWorkerAmount;
	int optimalWorkerAmount;

	//Values that can change based on overtime.
	int maximumWorkerPerMineral;
	int maximumWorkers;
	bool overtime = false;

	//So we dont overflow the terminal.
	int lastPrintFrame = 0;

	NexusEconomy(BWAPI::Unit nexus, int id) : nexus(nexus) , nexusID(id)
	{
		maximumWorkerPerMineral = OPTIMAL_WORKERS_PER_MINERAL;

		BWAPI::Unitset unitsInRadius = nexus->getUnitsInRadius(150);

		for (BWAPI::Unit unit : unitsInRadius)
		{
			if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
			{
				minerals.insert(unit);
			}
		}

		optimalWorkerAmount = minerals.size() * OPTIMAL_WORKERS_PER_MINERAL;
		maximumWorkerAmount = minerals.size() * MAXIMUM_WORKERS_PER_MINERAL;
		maximumWorkers = optimalWorkerAmount + WORKERS_PER_ASSIMILATOR;

		for (BWAPI::Unit mineral : minerals)
		{
			mineralWorkerCount[mineral] = 0;
		}

		std::cout << "Created new Nexus Economy" << "\n";
		std::cout << "Total Minerals: " << minerals.size() << "\n";
	}

	//[TODO] need to call deconstructor if a nexus dies and send back all the workers.
	~NexusEconomy()
	{

	}

	void OnFrame()
	{
		int frame = BWAPI::Broodwar->getFrameCount();
		for (BWAPI::Unit mineral : minerals)
		{
			//BWAPI::Broodwar->drawEllipseMap(mineral->getPosition(), 5, 5, BWAPI::Color(255, 0, 0), true);
			BWAPI::Broodwar->drawTextMap(mineral->getPosition(), std::to_string(mineral->getID()).c_str());
		}

		if(assimilator != nullptr) BWAPI::Broodwar->drawTextMap(assimilator->getPosition(), std::to_string(assimilator->getID()).c_str());

		for (BWAPI::Unit worker : workers)
		{
			BWAPI::Broodwar->drawEllipseMap(worker->getPosition(), 2, 2, BWAPI::Color(255, 0, 0), true);

			//If a worker is not carrying minerals and hasnt been assigned to one, assign them to farm. 
			if (!worker->isCarryingMinerals() || worker->isIdle())
			{
				//[TODO]: need to assign workers to assimilator incase they die, dont change who is assigned.
				if (assimilator != nullptr && assimilatorWorkerCount < WORKERS_PER_ASSIMILATOR)
				{
					worker->gather(assimilator);
					std::cout << "Assigning Worker to Assimilator" << "\n";
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
			}
			else
			{
				if (assignedResource.find(worker) != assignedResource.end())
				{
					BWAPI::Unit assignedMineral = assignedResource[worker];
					assignedResource.erase(worker);
					mineralWorkerCount[assignedMineral] -= 1;
					continue;
				}
			}
		}

		//Train more workers until we reach the workerCap
		if (!nexus->isTraining() && workers.size() < maximumWorkers) nexus->train(BWAPI::UnitTypes::Protoss_Probe);

		/*if (frame - lastPrintFrame >= 240)
		{
			printMineralWorkerCounts();
			lastPrintFrame = frame;
		}*/
	}
	

	void printMineralWorkerCounts()
	{
		for (BWAPI::Unit mineral : minerals)
		{
			std::cout << "Mineral " << mineral->getID() << " Count: " << mineralWorkerCount[mineral] << "\n";
		}
	}

	//Return true if the unit was apart of instance of NexusEconomy
	//[TODO]: need to figure out what to do with units once a mineral patch or gas is done.
	//[TODO]: also need to figure out edge case when unit destroyed is a nexus
	bool OnUnitDestroy(BWAPI::Unit unit)
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
	}

	BWAPI::Unit GetClosestMineralToWorker(BWAPI::Unit worker)
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

	void assignWorker(BWAPI::Unit unit)
	{
		workers.insert(unit);
	}

	void assignAssimilator(BWAPI::Unit assimilator)
	{
		this->assimilator = assimilator;
		assimilatorWorkerCount = 0;
	}

	void workOverTime()
	{
		maximumWorkers = maximumWorkerAmount;
		maximumWorkerPerMineral = OPTIMAL_WORKERS_PER_MINERAL + WORKERS_PER_ASSIMILATOR;
	}

	//Hand workers to combat or scout when we dont need them anymore.
	void breakTime()
	{
		maximumWorkers = optimalWorkerAmount;
		maximumWorkerPerMineral = MAXIMUM_WORKERS_PER_MINERAL + WORKERS_PER_ASSIMILATOR;
	}

	BWAPI::Unit getWorkerToScout()
	{
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
				workers.erase(unit);
				break;
			}
		}

		if (unitToReturn != nullptr) return unitToReturn;

		//Choose random unit if we did not find unit.

		for (BWAPI::Unit unit : workers)
		{
			if (unit->isCarryingMinerals())
			{
				unitToReturn = unit;
			}
			workers.erase(unit);
		}

		if (unitToReturn != nullptr) return unitToReturn;

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
				workers.erase(unit);
				break;
			}

			index++;
		}

		return unitToReturn;
	}
	//[TODO]: make sure we are handing off probe properlly
	BWAPI::Unit getWorkerToBuild()
	{
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

		if (unitToReturn != nullptr) return unitToReturn;
		
		//Choose random unit if we did not find unit.

		for (BWAPI::Unit unit : workers)
		{
			if (unit->isCarryingMinerals())
			{
				/*BWAPI::Unit assignedMineral = assignedResource[unit];
				mineralWorkerCount[assignedMineral] -= 1;
				assignedResource.erase(unit);*/
				unitToReturn = unit;
			}
		}

		if (unitToReturn != nullptr) return unitToReturn;

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
				break;
			} 

			index++;
		}

		return unitToReturn;
	}
};

class EconomyManager
{
public:
	ProtoBotCommander* commanderReference;
	std::vector<NexusEconomy> nexusEconomies;

	void OnFrame();
	void onUnitDestroy(BWAPI::Unit unit);
	void assignUnit(BWAPI::Unit unit);
	BWAPI::Unit getAvalibleWorker();

	std::unordered_map<BWAPI::Unit, int> assigned;
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedWorkers;

	EconomyManager(ProtoBotCommander* commanderReference);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units);
};

