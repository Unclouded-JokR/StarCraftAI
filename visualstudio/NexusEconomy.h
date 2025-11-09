#pragma once
#include "../src/starterbot/Tools.h"
#include <BWAPI.h>
#include <vector>
#include <unordered_map>

#define WORKERS_PER_ASSIMILATOR 3
#define OPTIMAL_WORKERS_PER_MINERAL 2
#define MAXIMUM_WORKERS_PER_MINERAL 3

class EconomyManager;

class NexusEconomy
{
public:
	EconomyManager* economyReference;
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
	bool requestAlreadyMade = false;

	NexusEconomy(BWAPI::Unit nexus, int id, EconomyManager* economyReference);
	~NexusEconomy();
	void OnFrame();
	void printMineralWorkerCounts();

	bool OnUnitDestroy(BWAPI::Unit unit);
	BWAPI::Unit GetClosestMineralToWorker(BWAPI::Unit worker);
	void assignWorker(BWAPI::Unit unit);
	void assignAssimilator(BWAPI::Unit assimilator);

	void workOverTime();
	void breakTime();

	BWAPI::Unitset getWorkersToTransfer(int numberOfWorkersForTransfer);
	BWAPI::Unit getWorkerToScout();
	BWAPI::Unit getWorkerToBuild();
};
