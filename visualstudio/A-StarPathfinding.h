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
};

static class AStar {
	private:
		static vector<Node> getNeighbours(BWAPI::UnitType unitType, const Node& currentNode, BWAPI::TilePosition end);
		static int TileToIndex(BWAPI::TilePosition tile);
		static bool tileWalkable(BWAPI::UnitType unitType, BWAPI::TilePosition tile);
		static void smoothPath(vector<BWAPI::Position>& vec, BWAPI::UnitType type);
	public:
		static Path GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end);
		static void drawPath(Path path);
};