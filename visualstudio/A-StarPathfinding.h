#pragma once

#include <BWAPI.h>
#include "../visualstudio/BWEB/Source/BWEB.h"
#include <vector>
#include <bwem.h>

using namespace std;

struct Node {
	BWAPI::TilePosition tile, parent;
	double gCost, hCost, fCost;
	int id = 0;

	Node(BWAPI::TilePosition tile, BWAPI::TilePosition parent, double gCost, double hCost, double fCost, int id) {
		this->tile = tile;
		this->parent = parent;
		this->gCost = gCost;
		this->hCost = hCost;
		this->fCost = fCost;
		this->id = id;
	};

	// Default Node constructor has an invalid BWAPI TilePosition value
	Node() {
		tile = BWAPI::TilePosition(-1, -1);
		id = 0;
	};

	bool operator <(const Node& rhs) const {
		return fCost > rhs.fCost;
	}

	bool operator ==(const Node& rhs) const {
		return tile == rhs.tile;
	}
};

class Path {
	BWAPI::Unit unit;
	BWAPI::TilePosition start;
	BWAPI::TilePosition	end;
	unordered_map<BWAPI::Unit, int> currentUnitPathIndex;

	public:
		vector<BWAPI::TilePosition> tiles;
		bool reachable;

		// Initializes the Path with the units start position and the target end position
		// Also initializes whether the path is reachable (default would be false until generated);
		Path(BWAPI::Unit unit, BWAPI::Position end) {
			this->unit = unit;
			this->start = BWAPI::TilePosition(unit->getPosition());
			this->end = BWAPI::TilePosition(end);
			this->reachable = false;
			this->tiles = {};
		}

		Path()
		{
			this->unit = nullptr;
			this->start = BWAPI::TilePositions::Invalid;
			this->end = BWAPI::TilePositions::Invalid;
			this->reachable = false;
			this->tiles = {};
		};

		void generateAStarPath();

		vector<Node> getNeighbours(const Node& node);
		int TileToIndex(BWAPI::TilePosition tile) {
			return (tile.y * BWAPI::Broodwar->mapWidth()) + tile.x;
		}

		void WalkPath();

		bool tileWalkable(BWAPI::TilePosition tile);
};