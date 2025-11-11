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
	void OnFrame();
	void onUnitDestroy(BWAPI::Unit unit);
	void assignUnit(BWAPI::Unit unit);
	BWAPI::Unit getAvalibleWorker();
	void needWorkerUnit(BWAPI::UnitType worker, BWAPI::Unit nexus);
	bool checkRequestAlreadySent(int unitID);
};

