#pragma once
#include <BWAPI.h>
#include "Timer.h";
#include "../../BWEM/src/bwem.h"
#include "../visualstudio/BWEB/Source/BWEB.h"

#define DEBUG
#define TIME_LIMIT_ENABLED false
#define HEURISTIC_WEIGHT 2.0
#define TIME_LIMIT_MS 50.0
#define FRAMES_BETWEEN_CACHING

using namespace std;
namespace {
	auto& bwem_map = BWEM::Map::Instance();
}

class Path;

extern vector<std::pair<BWAPI::Position, BWAPI::Position>> rectCoordinates;
extern vector<std::pair<BWAPI::TilePosition, double>> closedTiles;
extern vector<BWAPI::TilePosition> earlyExpansionTiles;
extern vector<BWAPI::Position> precachedPathPositions;

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

// Hash for storing BWAPI::TilePositions in an unordered_set 
struct TilePositionHash {
	std::size_t operator()(const BWAPI::TilePosition& tile) const {
		return std::hash<int>()(0.5 * (tile.x + tile.y) * (tile.x + tile.y + 1) + tile.y);
	}
};

// Hash for storing AreaId pairs as keys in an unordered_map
// Using a regular std::map for now due to a linker errors thats happening with the unordered_map
struct AreaIdPairHash {
	std::size_t operator()(const std::pair<BWEM::Area::id, BWEM::Area::id>& v) const {
		return std::hash<int>{}(v.first) ^ std::hash<int>{}(v.second);
	}
};

// Hash used for storing AreaId, Area pairs
struct AreaIdHash {
	std::size_t operator()(const BWEM::Area::id v) const {
		return std::hash<int>{}(v);
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

		bool operator ==(const Path& path) {
			return this->positions == path.positions;
		}
};

class AStar {
	private:
		static map<pair<BWEM::Area::id, BWEM::Area::id>, Path> AreaPathCache;
		static map<pair<BWAPI::WalkPosition, BWAPI::WalkPosition>, Path> ChokepointPathCache;

		static int TileToIndex(BWAPI::TilePosition tile);
		static bool tileWalkable(BWAPI::UnitType unitType, BWAPI::TilePosition tile, BWAPI::TilePosition end, bool isInteractableEndpoint);
		static double squaredDistance(BWAPI::Position pos1, BWAPI::Position pos2);
		static double chebyshevDistance(BWAPI::Position pos1, BWAPI::Position pos2);
		static double octileDistance(BWAPI::Position pos1, BWAPI::Position pos2);
		static Path generateSubPath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint=false);

	public:
		static Path GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint=false);
		static void drawPath(Path path);
		static void fillAreaPathCache();
};