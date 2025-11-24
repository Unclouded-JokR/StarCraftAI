#include "A-StarPathfinding.h"
#include "../src/starterbot/MapTools.h"
#include <queue>
#include <vector>

using namespace std;

vector<Node> Path::getNeighbours(Node node) {
	vector<Node> neighbours = {};

	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {

			// Ignore self
			if (x == 0 && y == 0) {
				continue;
			}

			BWAPI::TilePosition neighbourTile = BWAPI::TilePosition(node.tile.x + x, node.tile.y + y);

			// If the neighbour tile is valid, creates a Node of the neighbour tile and adds it to the neighbours vector
			if (neighbourTile.isValid()) {
				double gCost = start.getDistance(neighbourTile);
				double hCost = end.getDistance(neighbourTile);
				double fCost = gCost + hCost;
				Node neighbourNode = Node(neighbourTile, node.tile, gCost, hCost, fCost);
				neighbours.push_back(neighbourNode);
			}
		}
	}

	return neighbours;
}

void Path::generateAStarPath() {
	// Checks if either the starting or ending tile positions are invalid
	// If so, returns early with no tiles added to the path
	if (!start.isValid() || !end.isValid()) {
		return;
	}

	vector<Node> openSet;
	vector<Node> closedSet;

	openSet.push_back(Node(start, start, 0, 0, 0));

	while (openSet.size() > 0) {
		Node currentNode = openSet[0];

		// Each iteration, finds the node in the open set with the lowest fCost (and hCost as a tiebreaker)
		for (const auto& node : openSet) {
			if (node.fCost < currentNode.fCost || (node.fCost == currentNode.fCost && node.hCost < currentNode.hCost)) {
				currentNode = node;
			}
		}

		openSet.erase(remove_if(openSet.begin(), openSet.end(),
			[&](const Node& node) { return node.tile == currentNode.tile; }), openSet.end());

		closedSet.push_back(currentNode);

		if (currentNode.tile == end) {
			reachable = true;

			// Reconstructs path by stepping backwards from the end node through parents until the start is reached
			BWAPI::TilePosition pathTile = currentNode.tile;
			while (pathTile != start) {
				tiles.push_back(pathTile);
				auto it = find_if(closedSet.begin(), closedSet.end(),
					[&](const Node& node) { return node.tile == pathTile; });
				if (it != closedSet.end()) {
					pathTile = it->parent;
				}
				else {
					break;
				}
			}

			// The path is currently backwards so we reverse it
			reverse(tiles.begin(), tiles.end());

			return;
		}

		for (const auto& neighbour : getNeighbours(currentNode)) {

			// We'll skip the neighbour if:
			// 1. It's already in the closed set
			// 2. It's not walkable terrain
			if (find_if(closedSet.begin(), closedSet.end(),
				[&](const Node& node) { return node.tile == neighbour.tile; }) != closedSet.end()
				|| BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(neighbour.tile)))
			{
				continue;
			}

			// If the neighbour has not been visited before and is not in the open set, we'll add it to the open set
			// If it's already in the openSet, we'll check if our current path to it is cheaper than a previously found path to the neighbour
			// If cheaper, we'll update the neighbour's cost to our current path cost and set its parent to the current node
			auto openIt = find_if(openSet.begin(), openSet.end(),
				[&](const Node& node) { return node.tile == neighbour.tile; });
			if (openIt == openSet.end()) {
				openSet.push_back(neighbour);
			}
			else if (neighbour.gCost < openIt->gCost) {
				openIt->gCost = neighbour.gCost;
				openIt->fCost = neighbour.fCost;
				openIt->parent = neighbour.parent;
			}
		}
	}
}