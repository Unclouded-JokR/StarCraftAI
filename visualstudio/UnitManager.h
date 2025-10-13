#pragma once
#include <BWAPI.h>
#include <vector>

class UnitManager
{
public:
	std::vector<BWAPI::Unit> idleUnits = {};
	
	void assignUnitJobs();
	void addUnit(BWAPI::Unit);
	void removeUnit(int index);
	BWAPI::Unit getUnit(BWAPI::UnitType);
	BWAPI::Unitset& getUnits(BWAPI::UnitType, int numUnits);
};

