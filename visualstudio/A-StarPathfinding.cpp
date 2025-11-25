#include "A-StarPathfinding.h"
#include "../src/starterbot/MapTools.h"
#include <queue>
#include <vector>

using namespace std;

vector<Node> Path::getNeighbours(const Node& node) {
	vector<Node> neighbours = {};

	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {

			// Ignore self
			if (x == 0 && y == 0) {
				continue;
			}

			// Check diagonals to avoid walking diagonally through corner where two unwalkable tiles meet
			if (x != 0 && y != 0) {
				BWAPI::TilePosition a(node.tile.x + x, node.tile.y);
				BWAPI::TilePosition b(node.tile.x, node.tile.y + y);
				if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(a)) || !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(b)))
					continue;
			}

			BWAPI::TilePosition neighbourTile = BWAPI::TilePosition(node.tile.x + x, node.tile.y + y);

			// If the neighbour tile is walkable, creates a Node of the neighbour tile and adds it to the neighbours vector
			if (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(neighbourTile))) {
				double gCost = node.gCost + ((x != 0 && y != 0) ? 1.414 : 1.0); // Diagonal movement has more cost (Euclidean distance)
				double hCost = neighbourTile.getDistance(end);
				double fCost = gCost + hCost;
				Node neighbourNode = Node(neighbourTile, node.tile, gCost, hCost, fCost, BWAPI::Broodwar->getFrameCount());
				neighbours.push_back(neighbourNode);
			}
		}
	}

	return neighbours;
}

void Path::generateAStarPath() {
	tiles.clear();
	reachable = false;

	// Checks if either the starting or ending tile positions are invalid
	// If so, returns early with no tiles added to the path
	if (!start.isValid() || !end.isValid()) {
		return;
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
	vector<bool> closedSet(mapWidth * mapHeight, false); // Default: all tiles are not closed
	vector<double> gCostMap(mapWidth * mapHeight, DBL_MAX); // Default: all gCosts are as large as possible

	// Store parents in an unordered_map for reconstructing path later
	unordered_map<int, BWAPI::TilePosition> parent;
	
	// Very first node to be added is the starting tile position
	openSet.push(Node(start, start, 0, 0, 0, BWAPI::Broodwar->getFrameCount()));
	gCostMap[TileToIndex(start)] = 0;

	while (openSet.size() > 0) {	
		Node currentNode = openSet.top();
		openSet.pop();
		closedSet[TileToIndex(currentNode.tile)] = true;

		// Check if path is finished
		if (currentNode.tile == end) {
			while (currentNode.tile != start) {
				tiles.push_back(currentNode.tile);
				currentNode.tile = parent[TileToIndex(currentNode.tile)];
			}

			reachable = true;
			tiles.push_back(start);

			// Since we're pushing to the tile vector from end to start, we need to reverse it afterwards
			reverse(tiles.begin(), tiles.end());
			return;
		}

		// Step through each neighbour of the current node and add to open set if valid and not in closed set yet
		for (const auto& neighbour : getNeighbours(currentNode)) {
			// We'll skip the neighbour if:
			// 1. It's already in the closed set
			// 2. It's not walkable terrain
			if (!neighbour.tile.isValid() || !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(neighbour.tile))) {
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

void Path::WalkPath() {
	if (tiles.empty()) {
		return;
	}

	for (const auto& tile : tiles) {
		unit->move(BWAPI::Position(tile), true);
	}
}