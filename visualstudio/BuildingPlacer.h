#pragma once
#include <BWAPI.h>
#include <vector>
#include <set>
#include "../visualstudio/BWEB/Source/BWEB.h"

#define PYLON_POWER_WIDTH 16
#define PYLON_POWER_HEIGHT 10

struct BlockData {
	int largeBuildingLocations = 0; //Building locations that support 4x3 tiles
	int mediumBuildingLocations = 0; //Locations that support 3x2 tiles
	int powerBlocks; //Blocks used to power other medium and large building locations 2x2 tiles
};

class BuildingPlacer
{
private:
	std::vector<std::vector<int>> poweredTiles;
	std::vector<BWEB::Block> poweredBlocks;
	std::vector<BWEB::Block> largeBlocks;
	std::vector<BWEB::Block> mediumBlocks;
	std::vector<BWEB::Block> smallBlocks;
	std::vector<BWEB::Block> tinyBlocks;

	std::vector<BWEB::Block> tempBlocks;
	int mapWidth = 0;
	int mapHeight = 0;

public:
	int largeBuildingLocations = 0;
	int mediumBuildingLocations = 0
		;
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

