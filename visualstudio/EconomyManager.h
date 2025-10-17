#pragma once

#include <BWAPI.h>
#include <unordered_map>

class EconomyManager
{
public:
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units);
	void OnFrame();
	std::unordered_map<BWAPI::Unit, int> assigned;
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedWorkers;

};

