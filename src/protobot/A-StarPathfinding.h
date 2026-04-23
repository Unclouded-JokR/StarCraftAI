#pragma once
#include <BWAPI.h>
#include "Timer.h"
#include "BWEM/src/bwem.h"
#include "BWEB/Source/BWEB.h"
#include "Hashes.h"

//#define DEBUG_PATH
//#define DEBUG_PRECACHE
//#define DRAW_PRECACHE
//#define DEBUG_SUBPATH
#define TIME_LIMIT_ENABLED true
#define TIME_LIMIT_MS 500.0
#define HEURISTIC_WEIGHT 1.5

using namespace std;

namespace {
	auto& bwem_map = BWEM::Map::Instance();
}

class Path;

extern vector<std::pair<BWAPI::Position, BWAPI::Position>> rectCoordinates;
extern vector<BWAPI::Position> precachedPositions;

struct Node {
	BWAPI::TilePosition tile, parent;
	double gCost, hCost, fCost;

	Node(BWAPI::TilePosition tile, BWAPI::TilePosition parent, int gCost, int hCost, double fCost) {
		this->tile = tile;
		this->parent = parent;
		this->gCost = gCost;
		this->hCost = hCost;
		this->fCost = fCost;
	};

	// Default Node constructor has an invalid BWAPI TilePosition value
	Node() {
		tile = BWAPI::TilePosition(-1, -1);
	};

	bool operator <(const Node& rhs) const {
		// Tie-breaker
		if (this->fCost == rhs.fCost) {
			return this->hCost < rhs.hCost;
		}
		return this->fCost < rhs.fCost;
	}
	bool operator >(const Node& rhs) const {
		if (this->fCost == rhs.fCost) {
			return this->hCost > rhs.hCost;
		}
		return this->fCost > rhs.fCost;
	}
	bool operator ==(const Node& rhs) const {
		return tile == rhs.tile;
	}
};

class Path {
	public:
		vector<BWAPI::Position> positions = vector<BWAPI::Position>();
		int distance = 0;

		Path(vector<BWAPI::Position> positions, int distance) {
			this->positions = positions;
			this->distance = distance;
		};

		Path() noexcept {
			this->positions = vector<BWAPI::Position>();
			this->distance = 0;
		};

		void flip() {
			vector<BWAPI::Position> vec = this->positions;
			reverse(vec.begin(), vec.end());
			this->positions = vec;
		}

		bool operator ==(const Path& other) const{
			return this->positions == other.positions;
		}

		Path operator+(const Path& other){
			vector<BWAPI::Position> finalPositions;
			const int finalDist = this->distance + other.distance;
			finalPositions.insert(finalPositions.end(), this->positions.begin(), this->positions.end());
			finalPositions.insert(finalPositions.end(), other.positions.begin(), other.positions.end());
			return Path(finalPositions, finalDist);
		}
};

class AStar {
private:
	static map<pair<const BWEM::Area::id, const BWEM::Area::id>, const Path> AreaPathCache;
	static map<pair<const BWEM::ChokePoint*, const BWEM::ChokePoint*>, Path> ChokepointPathCache;
	static vector<pair<const BWAPI::WalkPosition, const BWAPI::WalkPosition>> UncachedAreaPairs;

	static int TileToIndex(BWAPI::TilePosition tile);
	static bool tileWalkable(BWAPI::UnitType unitType, BWAPI::TilePosition tile, BWAPI::TilePosition end, bool isInteractableEndpoint);
	static Path generateSubPath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint = false);
	static Path closestCachedPath(BWAPI::Position start, BWAPI::Position end);

public:
	/// \brief Generates path from start to end using A* pathfinding algorithm
	/// 
	/// <summary>
	/// IF YOU WANT A PATH TO AN INTERACTABLE UNIT (Constructing a geyser, mining minerals, etc.), SET isInteractableEndpoint AS TRUE
	/// 
	/// Using the A* algorithm, this function steps through BWAPI::TilePositions on the map to find an optimal path to the target position.
	/// \n This A* algorithm makes use of the precached paths between areas and chokepoints to optimize pathfinding. 
	/// \n If the start and end positions are in different areas, it will first find the path between the areas using the cached paths, then generate subpaths between the chokepoints in the area path, and finally generate subpaths from the start position to the first chokepoint and from the last chokepoint to the end position.
	/// \n\n <b>Heuristic</b>: BWAPI's getApproxDistance() multiplied by a heuristic weight. This is because BWAPI's getApproxDistance() is optimized and runs in O(1) time, so we can afford to use it as a heuristic even if it sacrifices some accuracy.
	/// \n\n <b>Walkability</b>: Checks tile walkability using tileWalkable() function. This function uses the unitType to get the unit's size and checks if any objects on the tile will collide with the unit.
	/// </summary>
	/// <param name="_start"></param>
	/// <param name="unitType"></param>
	/// <param name="_end"></param>
	/// <param name="isInteractableEndpoint"></param>
	/// <returns></returns>
	static Path GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint = false);

	/// <summary>
	/// Draws path on map for debugging. Uses BWAPI::Broodwar->drawLineMap()
	/// </summary>
	/// <param name="path"></param>
	static void drawPath(Path path);

	/// \brief Fills UncachedAreaPairs with pairs of areas that are neighbours. All of these pairs will be used to precache paths in fillAreaPathCache().
	/// 
	/// <summary>
	/// Stores all pairs of areas on the map by looping through all pairs of areas and checking if they are accessible neighbours. If so, adds the pair to UncachedAreaPairs vector.
	/// \n This is kept in a separate function in order to load the structure at the start of the game.
	/// \n\n The starting frame of the game is not confined to SSCAIT time limits, so we can afford to do some preprocessing before the game starts.
	/// <summary>
	static void fillUncachedAreaPairs();

	/// \brief Loops through all area pairs from UncachedAreaPairs() and stores a path in-between them.
	/// 
	/// <summary>
	/// Runs generateSubPath() to generate a Path object for every pair of areas.
	/// \n The paths are stored in AreaPathCache with the key as the pair of area ids. This pair is stored as (lower id, higher id).
	/// <summary>
	static void fillAreaPathCache();
	static void clearPathCache();
};