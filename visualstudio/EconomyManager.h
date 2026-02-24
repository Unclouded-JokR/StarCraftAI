#pragma once
#include "../src/starterbot/Tools.h"
#include "NexusEconomy.h"
#include <BWAPI.h>
#include <vector>
#include <unordered_map>

class ProtoBotCommander;

class EconomyManager
{
public:
	ProtoBotCommander* commanderReference;
	std::vector<NexusEconomy> nexusEconomies;

	EconomyManager(ProtoBotCommander* commanderReference);

	//BWAPI EVENTS
	void onStart();
	void onFrame();
	void onUnitDestroy(BWAPI::Unit unit);

	//Nexus Economy Methods
	void assignUnit(BWAPI::Unit unit);
	std::vector<NexusEconomy> getNexusEconomies();
	BWAPI::Unitset getWorkersToTransfer(int numberOfWorkers, NexusEconomy& nexusEconomy);
	void resourcesDepletedTranfer(BWAPI::Unitset workers, NexusEconomy& nexusEconomy);
	BWAPI::Unit getAvalibleWorker(BWAPI::Position buildLocation);
	BWAPI::Unit getUnitScout();
	void needWorkerUnit(BWAPI::UnitType worker, BWAPI::Unit nexus);
	int newMinerals;

	//Requets to Commander
	bool checkRequestAlreadySent(int unitID);
	bool workerIsConstructing(BWAPI::Unit);
	
};

