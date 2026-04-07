#pragma once
#include "../starterbot/Tools.h"
#include <BWAPI.h>
#include <vector>
#include <unordered_map>
#include <cmath>
#include "A-StarPathfinding.h"

#define WORKERS_PER_ASSIMILATOR 3
#define OPTIMAL_WORKERS_PER_MINERAL 2
#define MAXIMUM_WORKERS_PER_MINERAL 3

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
	//int moveTimer = 5;
	int workerNums;
	int attackingWorkers = 0;
	bool needWorkers = true;

	BWAPI::Unit nexus;
	BWAPI::Unitset workers;
	BWAPI::Unitset minerals;
	BWAPI::Unitset closestMinerals;
	BWAPI::Unit vespeneGyser = nullptr; //Should maybe consider changing this to a unit set like minerals in case multiple gysers.
	//BWAPI::Unitset vespeneGysers;
	BWAPI::Unit assimilator = nullptr;
	std::unordered_map<BWAPI::Unit, int> resourceWorkerCount;
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedResource;
	std::unordered_map<BWAPI::Unit, int> workerOrder;
	std::unordered_map<BWAPI::Unit, Path> workerPaths;

	//Calculated maximums based on number of minerals.
	int optimalWorkerAmount;
	int maximumWorkers;

	//Used to assign excess workers we create to mine gas.
	int workerOverflowAmount;

	NexusEconomy(BWAPI::Unit nexus, int id, EconomyManager* economyReference);
	~NexusEconomy();
	void defendWorker();
	void checkGasSteal();
	void onFrame();
	void printMineralWorkerCounts();
	//void moveToMineral(BWAPI::Unit worker, BWAPI::Unit mineral);

	bool OnUnitDestroy(BWAPI::Unit unit);
	BWAPI::Unit GetClosestMineralToWorker(BWAPI::Unit worker);
	void assignWorker(BWAPI::Unit unit);
	void assignWorkerBulk();
	void assignAssimilator(BWAPI::Unit assimilator);

	BWAPI::Unitset getWorkersToTransfer(int numberOfWorkersForTransfer);
	BWAPI::Unit getWorkerToScout();
	BWAPI::Unit getWorkerToBuild(BWAPI::Position locationToBuild);
	
};
