#pragma once
#include <BWAPI.h>
#include <vector>
#include <unordered_map>
#include "../visualstudio/BWEB/Source/BWEB.h"

#define PYLON_POWER_WIDTH 16
#define PYLON_POWER_HEIGHT 10

//Need to keep track of different blocks sizes somehow
//Large Blocks have atleast 2 more large placements
//Medium Blocks have 1 large placement or 1/2 medium placements.
//Small Blocks are anything otherwise.

struct BlockData {

	int Large_Placements = 0;
	int Medium_Placements = 0; 
	int Power_Placements = 0; //Blocks used to power other medium and large building locations, 2x2 blocks will use this as a counter.

	int Large_UsedPlacements = 0;
	int Medium_UsedPlacements = 0;
	int Power_UsedPlacements = 0;

	//BWEM Area a block is located in.
	const BWEM::Area* Block_AreaLocation; 
};

class BuildingPlacer
{
private:
	std::vector<std::vector<int>> poweredTiles;
	std::vector<BWEB::Block> largeBlocks;
	std::vector<BWEB::Block> mediumBlocks;
	std::vector<BWEB::Block> smallBlocks;
	std::map<BWAPI::TilePosition, BlockData> Block_Information;

	int mapWidth = 0;
	int mapHeight = 0;

public:
	int Used_LargeBuildingPlacements;
	int Used_MediumBuildingPlacements;

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

