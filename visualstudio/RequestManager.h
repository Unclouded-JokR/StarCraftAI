#pragma once
#include "UnitManager.h"
#include <BWAPI.h>

class RequestManager
{
public:
	BWAPI::Unit requestUnit(BWAPI::UnitType); //add command
	BWAPI::Unit requestUnitBuild(BWAPI::UnitCommand &);
	
};

