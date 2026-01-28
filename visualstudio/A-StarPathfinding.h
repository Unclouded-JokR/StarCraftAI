#pragma once

#include <BWAPI.h>
#include "../visualstudio/BWEB/Source/BWEB.h"
#include <vector>
#include <bwem.h>

using namespace std;

struct Node {
	BWAPI::TilePosition tile, parent;
	double gCost, hCost, fCost;

	Node(BWAPI::TilePosition tile, BWAPI::TilePosition parent, double gCost, double hCost, double fCost) {
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
		return fCost > rhs.fCost;
	}

	bool operator ==(const Node& rhs) const {
		return tile == rhs.tile;
	}
};

class AStar {
private:
	vector<Node> getNeighbours(BWAPI::UnitType unitType, const Node& currentNode, BWAPI::TilePosition end);

	int TileToIndex(BWAPI::TilePosition tile);

	bool tileWalkable(BWAPI::UnitType unitType, BWAPI::TilePosition tile);

public:
	vector<BWAPI::TilePosition> GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end);
};