#pragma once
#include <BWAPI.h>

class BuildManager;

class BuildingPlacer
{
public:
	BuildManager* buildManagerReference;

	BuildingPlacer(BuildManager*);
	BWAPI::Position getPositionToBuild(BWAPI::UnitType);
	bool alreadyUsingTiles(BWAPI::TilePosition);
};

