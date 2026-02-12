#pragma once
#include <BWAPI.h>
#include <vector>
#include "../visualstudio/BWEB/Source/BWEB.h"

#define PYLON_POWER_DIAMETER 16
#define PYLON_POWER_HEIGHT 10


class BuildingPlacer
{
private:
	std::vector<std::vector<int>> poweredTiles;
	std::vector<BWEB::Block> poweredBlocks;

	int mapWidth = 0;
	int mapHeight = 0;
	int offsets[10] = { 4, 2, 1, 0, 0, 0, 0, 1, 2, 4 };

public:
	BuildingPlacer();
	BWAPI::Position getPositionToBuild(BWAPI::UnitType);
	bool alreadyUsingTiles(BWAPI::TilePosition, int, int);
	bool checkPower(BWAPI::TilePosition, BWAPI::UnitType);
	void drawPoweredTiles();

	void onStart();
	void onUnitCreate(BWAPI::Unit);
	void onUnitComplete(BWAPI::Unit);
	void onUnitDestroy(BWAPI::Unit);
	void onUnitMorph(BWAPI::Unit);
	void onUnitDiscover(BWAPI::Unit);
};

