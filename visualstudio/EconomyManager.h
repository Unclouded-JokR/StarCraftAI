#pragma once
#include <BWAPI.h>
#include <unordered_map>

class ProtoBotCommander;

class EconomyManager
{
public:
	ProtoBotCommander* commanderReference;
	std::unordered_map<BWAPI::Unit, int> assigned;
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedWorkers;

	EconomyManager(ProtoBotCommander* commanderReference);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units);
	void OnFrame();
	void assignUnit(BWAPI::Unit unit);
	BWAPI::Unit getAvalibleWorker();
};

