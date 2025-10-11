#pragma once
#include "UnitManager.h"
#include <BWAPI.h>

class RequestManager
{
public:
	BWAPI::Unit requestUnit(BWAPI::Unit); //add command
	BWAPI::Unit requestUnitBuild(BWAPI::UnitCommand &);
	
};

