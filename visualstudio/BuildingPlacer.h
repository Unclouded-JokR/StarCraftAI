#pragma once
#include <BWAPI.h>

class BuildingPlacer
{
public:
	BuildingPlacer();
	BWAPI::Position getPositionToBuild(BWAPI::UnitType);
	bool alreadyUsingTiles(BWAPI::TilePosition, int, int);

	void onUnitCreate(BWAPI::Unit);
	void onUnitDestroy(BWAPI::Unit);
	void onUnitMorph(BWAPI::Unit);
	void onUnitDiscover(BWAPI::Unit);
};

