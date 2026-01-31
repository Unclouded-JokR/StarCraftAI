#pragma once
#include <BWAPI.h>

class BuildingPlacer
{
public:
	BuildingPlacer();
	BWAPI::Position getPositionToBuild(BWAPI::UnitType);
	bool alreadyUsingTiles(BWAPI::TilePosition, int, int);
};

