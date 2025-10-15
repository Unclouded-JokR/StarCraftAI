#pragma once
#include <BWAPI.h>

class UnitManager
{
public:
	//Array of size 20 (We can change this be larger or smaller), that will hold all of the units that are idle.
	BWAPI::Unit idleUnits[20];

	void addUnit(BWAPI::Unit);
	void removeUnit(int index);
	BWAPI::Unit getUnit();
};

