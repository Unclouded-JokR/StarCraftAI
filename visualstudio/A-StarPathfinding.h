#pragma once

#include <BWAPI.h>
#include "../visualstudio/BWEB/Source/BWEB.h"
#include <vector>
#include <bwem.h>
#include "Timer.h";

using namespace std;

extern vector<std::pair<BWAPI::Position, BWAPI::Position>> rectCoordinates;
extern vector<std::pair<BWAPI::TilePosition, double>> closedTiles;

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

struct NodeHash {
	std::size_t operator()(const Node& node) const {
		return std::hash<int>()(0.5 * (node.tile.x + node.tile.y) * (node.tile.x + node.tile.y + 1) + node.tile.y);
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

class AStar {
	private:
		static vector<Node> getNeighbours(BWAPI::UnitType unitType, const Node& currentNode, BWAPI::TilePosition end, bool isInteractableEndpoint);
		static int TileToIndex(BWAPI::TilePosition tile);
		static bool tileWalkable(BWAPI::UnitType unitType, BWAPI::TilePosition tile, BWAPI::TilePosition end, bool isInteractableEndpoint);
		static bool isOrthogonal(BWAPI::Position pos);
		static double squaredDistance(BWAPI::Position pos1, BWAPI::Position pos2);
		static double chebyshevDistance(BWAPI::Position pos1, BWAPI::Position pos2);
		static double octileDistance(BWAPI::Position pos1, BWAPI::Position pos2);

	public:
		static Path GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint = false);
		static void drawPath(Path path);
};