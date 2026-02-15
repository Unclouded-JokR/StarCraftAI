#pragma once
#include <BWAPI.h>
#include <vector>
#include <unordered_map>
#include "../visualstudio/BWEB/Source/BWEB.h"
#include <map>

#define PYLON_POWER_WIDTH 16
#define PYLON_POWER_HEIGHT 10
#define MINIMUM_LARGE_POWERED_PLACEMENTS 4
#define MINIMUM_MEDIUM_POWERED_PLACEMENTS 2

//Need to keep track of different blocks sizes somehow
//Large Blocks have atleast 2 more large placements
//Medium Blocks have 1 large placement or 1/2 medium placements.
//Small Blocks are anything otherwise.

struct PlacementInfo
{
	enum PlacementFlag { SUCCESS, NO_POWER, NO_BLOCKS, NO_GYSERS, NO_PLACEMENTS, NO_EXPANSIONS, DEFAULT };
	PlacementFlag flag = PlacementFlag::DEFAULT;

	BWAPI::Position position = BWAPI::Positions::Invalid;
};

struct BlockData {

	enum BlockSize {LARGE, MEDIUM, SUPPLY, UNDEFINED};
	BlockSize Blocksize = BlockSize::UNDEFINED;

	//Only matters for Medium and Large Blocks
	enum PowerState {FULLY_POWERED, HALF_POWERED, NOT_POWERED};
	PowerState Power_State = PowerState::NOT_POWERED;

	int Large_Placements = 0;
	int Medium_Placements = 0; 
	int Power_Placements = 0; //Blocks used to power other medium and large building locations, 2x2 blocks will use this as a counter.

	int Used_Power_Placements = 0;

	//BWEM Area a block is located in.
	const BWEM::Area* Block_AreaLocation; 

	std::set<BWAPI::TilePosition> PowerTiles;
};

class BuildingPlacer
{
private:
	//Might not need these and just want to store the info in BlockData
	//std::vector<std::vector<int>> poweredTiles;

	std::map<BWAPI::TilePosition, BlockData> Block_Information;

	//Might need this to make getting blocks in Areas we own easier.
	std::unordered_set<const BWEM::Area *> AreasOccupied;

	//List of all the blocks from the areas we own.
	std::vector<BWEB::Block> ProtoBot_Blocks;

	//All the Areas the BWEM has and the blocks that are assigned to them. For the purpose of expanding our base we will only place in areas we own.
	//This should not apply to Walls, Proxy, and Cheeses.
	std::unordered_map<const BWEM::Area*, std::vector<BWEB::Block>> Area_Blocks;

	int mapWidth = 0;
	int mapHeight = 0;

public:
	int Powered_LargePlacements = 0;
	int Powered_MediumPlacements = 0;
	int Used_LargeBuildingPlacements;
	int Used_MediumBuildingPlacements;

	BuildingPlacer();
	PlacementInfo getPositionToBuild(BWAPI::UnitType);
	bool alreadyUsingTiles(BWAPI::TilePosition, int, int, bool assimilator);
	bool checkPower(BWAPI::TilePosition, BWAPI::UnitType);
	BWAPI::TilePosition checkBuildingBlocks();
	BWAPI::TilePosition checkPowerReserveBlocks();
	BWAPI::TilePosition findAvailableExpansion();
	BWAPI::TilePosition findAvailableGyser();
	BWAPI::TilePosition findAvaliblePlacement(BWAPI::UnitType);

	void drawPoweredTiles();

	void onStart();
	void onUnitCreate(BWAPI::Unit);
	void onUnitComplete(BWAPI::Unit);
	void onUnitDestroy(BWAPI::Unit);
	void onUnitMorph(BWAPI::Unit);
	void onUnitDiscover(BWAPI::Unit);
};

