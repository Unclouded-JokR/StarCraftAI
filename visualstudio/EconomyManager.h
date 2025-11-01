#pragma once
#include <BWAPI.h>
#include <unordered_map>
#include <vector>

class ProtoBotCommander;

class EconomyManager
{
public:
	ProtoBotCommander* commanderReference;

	EconomyManager(ProtoBotCommander* commanderReference);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units, int workers_from_com);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units, int workers_from_com);
	void OnFrame();
	std::unordered_map<BWAPI::Unit, int> assigned;
	std::unordered_map<BWAPI::Unit, std::vector<BWAPI::Unit>> assignedWorkers;
	std::vector<BWAPI::Unit> available_workers;
	int workers_per_hs = 1;

	void assignUnit(BWAPI::Unit unit);
	BWAPI::Unit getAvalibleWorker();
};

