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
	BWAPI::Unit nexus;
	BWAPI::Unitset workers;
	BWAPI::Unitset minerals;
	BWAPI::Unit assimilator = nullptr;
	std::unordered_map<BWAPI::Unit, int> mineralWorkerCount;
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedResource;
	int assimilatorWorkerCount = 0;
	const int nexusID;
	int maximumWorkerAmount;
	int optimalWorkerAmount;
	int maximumWorkers;
	int maximumWorkerPerMineral;

	bool overtime = false;
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
		maximumWorkers = optimalWorkerAmount;

		for (BWAPI::Unit mineral : minerals)
		{
			mineralWorkerCount[mineral] = 0;
		}

		std::cout << "Created new Nexus Economy" << "\n";
		std::cout << "Total Minerals: " << minerals.size() << "\n";
	}

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

			if (worker->isIdle())
			{
				//Worker is already assigned
				if (assignedResource.find(worker) != assignedResource.end())
				{
					if (worker->isCarryingMinerals())
					{
						worker->rightClick(nexus);
					}
					else
					{
						BWAPI::Unit closestMineral = assignedResource[worker];
						worker->gather(closestMineral);
					}
					continue;
				}

				if (assimilator != nullptr && assimilatorWorkerCount < WORKERS_PER_ASSIMILATOR)
				{
					worker->gather(assimilator);
					std::cout << "Assigning Worker to Assimilator" << "\n";
					assimilatorWorkerCount += 1;
					continue;
				}

				BWAPI::Unit closestMineral = GetClosestMineralToWorker(worker);
				if (closestMineral == nullptr)
				{
					continue;
				}
				else
				{
					mineralWorkerCount[closestMineral] += 1;
					assignedResource[worker] = closestMineral;
					worker->gather(closestMineral);
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
	bool onUnitDestroy(BWAPI::Unit unit)
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
				int distanceFromWorker = worker->getDistance(mineral);

				if (mineralWorkerCount[mineral] < maximumWorkerPerMineral && distanceFromWorker < closestMineralDistance)
				{
					closestMineralDistance = distanceFromWorker;
					closestMineral = mineral;
				}
			}
		}

		return closestMineral;

		////Case for having 2 or 3 workers per mineral
		//for (BWAPI::Unit mineral : minerals)
		//{
		//	if (mineralWorkerCount[mineral] < workerCap)
		//	{
		//		return mineral;
		//	}
		//}
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
		maximumWorkerPerMineral = OPTIMAL_WORKERS_PER_MINERAL;
	}

	void breakTime()
	{
		maximumWorkers = optimalWorkerAmount;
		maximumWorkerPerMineral = MAXIMUM_WORKERS_PER_MINERAL;
	}

	//[TODO]: make sure we are handing off probe properlly
	BWAPI::Unit getWorker()
	{
		BWAPI::Unit unitToReturn = nullptr;

		for (BWAPI::Unit unit : workers)
		{
			if (unit->isIdle())
			{
				BWAPI::Unit assignedMineral = assignedResource[unit];
				mineralWorkerCount[assignedMineral] -= 1;
				unitToReturn = unit;
				break;
			}
		}

		if (unitToReturn != nullptr)
		{
			workers.erase(unitToReturn);
			return unitToReturn;
		}

		//if (unitToReturn != nullptr)
		//{
		//	/*BWAPI::Unit assinedMineral = assignedResource[unitToReturn];
		//	workers.erase(unitToReturn);
		//	mineralWorkerCount
		//	return unitToReturn;*/
		//}

		//for (BWAPI::Unit unit : workers)
		//{
		//	if (unit->isCarryingMinerals())
		//	{
		//		unitToReturn = unit;
		//		break;
		//	}
		//}

		//else
		//{
		//	workers.erase(unitToReturn);
		//}
		//
		//return unitToReturn;
		return nullptr;
	}
};

class EconomyManager
{
public:
	ProtoBotCommander* commanderReference;
	std::vector<NexusEconomy> nexusEconomies;

	void OnFrame();
	void assignUnit(BWAPI::Unit unit);
	BWAPI::Unit getAvalibleWorker();




	std::unordered_map<BWAPI::Unit, int> assigned;
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedWorkers;

	EconomyManager(ProtoBotCommander* commanderReference);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units);
};

