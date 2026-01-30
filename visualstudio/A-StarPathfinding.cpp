#include "A-StarPathfinding.h"
#include "../src/starterbot/MapTools.h"
#include <queue>
#include <vector>

using namespace std;

Path AStar::GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end) {
	BWAPI::TilePosition start = BWAPI::TilePosition(_start);
	BWAPI::TilePosition end = BWAPI::TilePosition(_end);
	vector<BWAPI::Position> tiles;

	// Checks if either the starting or ending tile positions are invalid
	// If so, returns early with no tiles added to the path
	if (!start.isValid() || !end.isValid()) {
		return Path();
	}

	// Flying units can move directly to the end position without pathfinding
	if (unitType.isFlyer()) {
		tiles.push_back(_end);
		double dist = _start.getDistance(_end);
		return Path(tiles, dist);
	}

	// Optimization using priority_queue for open set
	// The priority_queue will always have the node with the lowest fCost at the top
	// Comparator is overloaded ih the Node struct in A-StarPathfinding.h
	priority_queue<Node> openSet;

	// Optimization using array for closed set
	// Array to represent closed set for quick lookup (assuming max map size of 256x256)
	// Tile position will be converted into 1D value for indexing
	const int mapWidth = BWAPI::Broodwar->mapWidth();
	const int mapHeight = BWAPI::Broodwar->mapHeight();
	vector<bool> closedSet(mapWidth * mapHeight, false); // Default: no closed tiles
	vector<double> gCostMap(mapWidth * mapHeight, DBL_MAX); // Default: all gCosts are as large as possible

	// Store parents in an unordered_map for reconstructing path later
	unordered_map<int, BWAPI::TilePosition> parent;

	// Very first node to be added is the starting tile position
	openSet.push(Node(start, start, 0, 0, 0));
	gCostMap[TileToIndex(start)] = 0;

	while (openSet.size() > 0) {
		Node currentNode = openSet.top();
		openSet.pop();
		closedSet[TileToIndex(currentNode.tile)] = true;

		// Check if path is finished
		if (currentNode.tile == end) {
			double distance = 0;
			BWAPI::Position prevPos = BWAPI::Position(currentNode.tile);
			while (currentNode.tile != start) {
				const BWAPI::Position pathTile = BWAPI::Position(currentNode.tile);
				tiles.push_back(pathTile);
				currentNode.tile = parent[TileToIndex(currentNode.tile)];

				distance += prevPos.getDistance(pathTile);
				prevPos = pathTile;
			}

			tiles.push_back(_start);
			distance += prevPos.getDistance(_start);

			// Since we're pushing to the tile vector from end to start, we need to reverse it afterwards
			reverse(tiles.begin(), tiles.end());
			return Path(tiles, distance);
		}

		// Step through each neighbour of the current node and add to open set if valid and not in closed set yet
		for (const auto& neighbour : getNeighbours(unitType, currentNode, end)) {
			// We'll skip the neighbour if:
			// 1. It's already in the closed set
			// 2. It's not walkable terrain
			if (!neighbour.tile.isValid() || !tileWalkable(unitType, neighbour.tile)) {
				continue;
			}

			// Check if neighbour is already in closed set
			bool cs = closedSet[TileToIndex(neighbour.tile)];
			if (cs == true) {
				continue;
			}

			// If neighbour is not in the closed set, add to open set ONLY IF it has a better gCost than previously recorded
			if (neighbour.gCost >= gCostMap[TileToIndex(neighbour.tile)]) {
				continue;
			}
			else {
				openSet.push(neighbour);
				parent[TileToIndex(neighbour.tile)] = currentNode.tile;
				gCostMap[TileToIndex(neighbour.tile)] = neighbour.gCost;
			}

		}
	}
}

vector<Node> AStar::getNeighbours(BWAPI::UnitType unitType, const Node& currentNode, BWAPI::TilePosition end) {
	vector<Node> neighbours = {};

	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {

			// Ignore self
			if (x == 0 && y == 0) {
				continue;
			}

			// Check diagonals to avoid walking diagonally through corner where two unwalkable tiles meet
			if (x != 0 && y != 0) {
				BWAPI::TilePosition a(currentNode.tile.x + x, currentNode.tile.y);
				BWAPI::TilePosition b(currentNode.tile.x, currentNode.tile.y + y);
				if (!tileWalkable(unitType, a) || !tileWalkable(unitType, b))
				continue;
			}

			BWAPI::TilePosition neighbourTile = BWAPI::TilePosition(currentNode.tile.x + x, currentNode.tile.y + y);

			// If the neighbour tile is walkable, creates a Node of the neighbour tile and adds it to the neighbours vector
			if (tileWalkable(unitType, neighbourTile)) {
				double gCost = currentNode.gCost + ((x != 0 && y != 0) ? 1.414 : 1.0); // Diagonal movement has more cost (Euclidean distance)
				double hCost = neighbourTile.getDistance(end);
				double fCost = gCost + hCost;
				Node neighbourNode = Node(neighbourTile, currentNode.tile, gCost, hCost, fCost);
				neighbours.push_back(neighbourNode);
			}
		}
	}

	return neighbours;
}

int AStar::TileToIndex(BWAPI::TilePosition tile) {
	return (tile.y * BWAPI::Broodwar->mapWidth()) + tile.x;
}

bool AStar::tileWalkable(BWAPI::UnitType unitType, BWAPI::TilePosition tile) {
	int unitWidth = unitType.width();
	int unitHeight = unitType.height();
	
	// When checking for walkable, we check if any of the 4 corners of the unit would be unwalkable if CENTERED on tile
	// If so, the unit would get stuck on them when trying to move through the tile.
	BWAPI::Position center = BWAPI::Position(tile.x * 32 + 16, tile.y * 32 + 16);
	int left = center.x - (unitWidth / 2);
	int right = center.x + (unitWidth / 2);
	int top = center.y - (unitHeight / 2);
	int bottom = center.y + (unitHeight / 2);

	// Pixel position -> Walk position
	int walkTileLeft = left / 8;
	int walkTileRight = right / 8;
	int walkTileTop = top / 8;
	int walkTileBottom = bottom / 8;

	// BWAPI::Broodwar->isWalkable() only checks static terrain, so we also need to check for buildings on the tile
	const auto& unitsOnTile = BWAPI::Broodwar->getUnitsInRectangle(left, top, right, bottom);
	for (const auto& unit : unitsOnTile) {
		if (unitType.isBuilding()) {
			return false;
		}
	}

	// Iterates through every walk tile the unit would occupy
	for (int wx = walkTileLeft; wx <= walkTileRight; wx++) {
		for (int wy = walkTileTop; wy <= walkTileBottom; wy++) {

			BWAPI::WalkPosition walkPos(wx, wy);
			if (!BWAPI::Broodwar->isWalkable(walkPos)) {
				return false;
			}
		}
	}
}