#pragma once
#include "../src/starterbot/Tools.h"
#include <BWAPI.h>
#include <vector>
#include <unordered_map>
#include <cmath>
#include "A-StarPathfinding.h"

#define WORKERS_PER_ASSIMILATOR 3
#define OPTIMAL_WORKERS_PER_MINERAL 2

class EconomyManager;

class NexusEconomy
{
public:
	EconomyManager* economyReference;
	int nexusID;
	int lifetime = 0; //Used to delay assimilator build for now.
	int fff = 0;
	int spot;
	double dx;
	double dy;
	double slope;
	double intercept;
	int spot_to_move;
	int offset;
	int moveTimer = 5;
	int workerNums;

	BWAPI::Unit nexus;
	BWAPI::Unitset workers;
	BWAPI::Unitset minerals;
	BWAPI::Unit vespeneGyser = nullptr; //Should maybe consider changing this to a unit set like minerals in case multiple gysers.
	BWAPI::Unit assimilator = nullptr;
	std::unordered_map<BWAPI::Unit, int> resourceWorkerCount;
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedResource;
	std::unordered_map<BWAPI::Unit, int> workerOrder;
	std::unordered_map<BWAPI::Unit, Path> workerPaths;



	//Calculated maximums based on number of minerals.
	int optimalWorkerAmount;
	int maximumWorkers;

	NexusEconomy(BWAPI::Unit nexus, int id, EconomyManager* economyReference);
	~NexusEconomy();
	int addMissedResources();
	void onFrame();
	void printMineralWorkerCounts();
	void moveToMineral(BWAPI::Unit worker, BWAPI::Unit mineral);

	bool OnUnitDestroy(BWAPI::Unit unit);
	BWAPI::Unit GetClosestMineralToWorker(BWAPI::Unit worker);
	void assignWorker(BWAPI::Unit unit);
	void assignAssimilator(BWAPI::Unit assimilator);

	BWAPI::Unitset getWorkersToTransfer(int numberOfWorkersForTransfer);
	BWAPI::Unit getWorkerToScout();
	BWAPI::Unit getWorkerToBuild(BWAPI::Position locationToBuild);
	
};
