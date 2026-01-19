#pragma once
#include "../src/starterbot/Tools.h"
#include <BWAPI.h>
#include <vector>
#include <unordered_map>

#define WORKERS_PER_ASSIMILATOR 3
#define OPTIMAL_WORKERS_PER_MINERAL 2

class EconomyManager;

class NexusEconomy
{
public:
	EconomyManager* economyReference;
	int nexusID;
	int lifetime = 0; //Used to delay assimilator build for now.

	BWAPI::Unit nexus;
	BWAPI::Unitset workers;
	BWAPI::Unitset minerals;
	BWAPI::Unit vespeneGyser = nullptr; //Should maybe consider changing this to a unit set like minerals in case multiple gysers.
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
	bool requestAlreadyMade = false;

	NexusEconomy(BWAPI::Unit nexus, int id, EconomyManager* economyReference);
	~NexusEconomy();
	void addMissedResources();
	void onFrame();
	void printMineralWorkerCounts();

	bool OnUnitDestroy(BWAPI::Unit unit);
	BWAPI::Unit GetClosestMineralToWorker(BWAPI::Unit worker);
	void assignWorker(BWAPI::Unit unit);
	void assignAssimilator(BWAPI::Unit assimilator);

	BWAPI::Unitset getWorkersToTransfer(int numberOfWorkersForTransfer);
	BWAPI::Unit getWorkerToScout();
	BWAPI::Unit getWorkerToBuild(BWAPI::Position locationToBuild);
};
