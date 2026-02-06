#include "A-StarPathfinding.h"
#include "../src/starterbot/MapTools.h"
#include <queue>
#include <vector>

vector<std::pair<BWAPI::Position, BWAPI::Position>> rectCoordinates;
vector<BWAPI::TilePosition> closedTiles;

// Generates path from start to end using A* pathfinding algorithm
// IF YOU WANT PATH TO AN INTERACTABLE UNIT (Constructing a geyser, mining minerals, etc.), SET isInteractableEndpoint AS TRUE
Path AStar::GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint) {
	Timer timer = Timer();
	timer.start();
	rectCoordinates.clear();
	closedTiles.clear();

	vector<BWAPI::Position> tiles = vector<BWAPI::Position>();
	// Checks if either the starting or ending tile positions are invalid
	// If so, returns early with no tiles added to the path
	if (!_start.isValid() || !_end.isValid()) {
		std::cout << "ERROR: Starting point or ending point not valid" << endl;
		return Path();
	}

	// Flying units can move directly to the end position without pathfinding
	if (unitType.isFlyer()) {
		tiles.push_back(_end);
		const double dist = _start.getApproxDistance(_end);
		return Path(tiles, dist);
	}

	const BWAPI::TilePosition start = BWAPI::TilePosition(_start);
	const BWAPI::TilePosition end = BWAPI::TilePosition(_end);

	// Optimization using priority_queue for open set
	// The priority_queue will always have the node with the lowest fCost at the top
	// Comparator is overloaded ih the Node struct in A-StarPathfinding.h
	priority_queue<Node> openSet;

	// Optimization using array for closed set
	const int mapWidth = BWAPI::Broodwar->mapWidth();
	const int mapHeight = BWAPI::Broodwar->mapHeight();
	vector<bool> closedSet(mapWidth * mapHeight, false); // Default: no closed tiles
	vector<double> fCostMap(mapWidth * mapHeight, DBL_MAX); // Default: all fCosts are as large as possible

	// Store parents in an unordered_map for reconstructing path later
	unordered_map<int, BWAPI::TilePosition> parent;

	// Very first node to be added is the starting tile position
	openSet.push(Node(start, start, 0, 0, 0));
	fCostMap[TileToIndex(start)] = 0;

	while (openSet.size() > 0) {
		Node currentNode = openSet.top();
		openSet.pop();
		closedSet[TileToIndex(currentNode.tile)] = true;
		closedTiles.push_back(currentNode.tile);

		// Check if path is finished
		if (currentNode.tile == end) {
			int distance = 0;
			BWAPI::Position prevPos = BWAPI::Position(currentNode.tile);
			BWAPI::Position dir;

			while (currentNode.tile != start) {
				tiles.push_back(BWAPI::Position(currentNode.tile));

				currentNode.tile = parent[TileToIndex(currentNode.tile)];
				const BWAPI::Position currentPos = BWAPI::Position(currentNode.tile);

				if (tiles.size() == 1) {
					dir = BWAPI::Position(currentNode.tile) - prevPos;
					distance += prevPos.getApproxDistance(currentPos);
				}
				else{
					BWAPI::Position newDir = currentPos - prevPos;
					BWAPI::Broodwar->printf("Current direction: (%d, %d), New direction: (%d, %d)", dir.x, dir.y, newDir.x, newDir.y);

					if (((abs(newDir.x) == 32 && newDir.y == 0) || (newDir.x == 0 && abs(newDir.y) == 32))) {
						BWAPI::Unitset circle1 = BWAPI::Broodwar->getUnitsInRadius(currentPos, (unitType.width() / 2) + 10, BWAPI::Filter::IsBuilding);
						BWAPI::Unitset circle2 = BWAPI::Broodwar->getUnitsInRadius(prevPos, (unitType.width() / 2) + 10, BWAPI::Filter::IsBuilding);
						// Orthogonal movement alongside a building
						// If the direction doesn't change, we check whether the current and previous tile are near buildings
						// If the current tile is near a building but the previous tile isn't and vice versa, then path is going around a building
						// If both are near buildings, keep the first waypoint but remove future waypoints until direction changes
						if (dir == newDir) {
							if (!circle1.empty() && !circle2.empty()) {
								tiles.pop_back();
								distance -= prevPos.getApproxDistance(currentPos);
							}
						}
						else {
							if ((!circle1.empty() && circle2.empty()) || (circle1.empty() && !circle2.empty())) {
								tiles.erase(tiles.end() - 2);
								distance -= prevPos.getApproxDistance(currentPos);
							}
						}
					}

					dir = newDir;
				}
				prevPos = currentPos;
			}

			tiles.push_back(_start);
			distance += prevPos.getApproxDistance(_start);

			// Since we're pushing to the tile vector from end to start, we need to reverse it afterwards
			reverse(tiles.begin(), tiles.end());

			timer.stop();
			std::cout << "Total time spent generating path: " << timer.getElapsedTime() << endl;
			return Path(tiles, distance);
		}

		// Step through each neighbour of the current node and add to open set if valid and not in closed set yet
		vector<Node> neighbours = getNeighbours(unitType, currentNode, end, isInteractableEndpoint);
		for (const auto neighbour : neighbours) {
			// We'll skip the neighbour if:
			// 1. It's already in the closed set
			// 2. It's not walkable terrain
			if (!neighbour.tile.isValid() || !tileWalkable(unitType, neighbour.tile, BWAPI::TilePosition(end), isInteractableEndpoint)) {
				continue;
			}

			// Check if neighbour is already in closed set
			if (closedSet[TileToIndex(neighbour.tile)]) {
				continue;
			}

			// If current path to neighbour has a more expensive fCost than previously recorded, don't add this path
			if (fCostMap[TileToIndex(neighbour.tile)] < neighbour.fCost) {
				continue;
			}
			else {
				openSet.push(neighbour);
				parent[TileToIndex(neighbour.tile)] = currentNode.tile;
				fCostMap[TileToIndex(neighbour.tile)] = neighbour.fCost;
			}

		}
	}

	return Path();
}

vector<Node> AStar::getNeighbours(BWAPI::UnitType unitType, const Node& currentNode, BWAPI::TilePosition end, bool isInteractableEndpoint) {

	vector<Node> neighbours;

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
				if (!tileWalkable(unitType, a, end, isInteractableEndpoint) || !tileWalkable(unitType, b, end, isInteractableEndpoint))
					continue;
			}

			const BWAPI::TilePosition neighbourTile = BWAPI::TilePosition(currentNode.tile.x + x, currentNode.tile.y + y);

			// If the neighbour tile is walkable, creates a Node of the neighbour tile and adds it to the neighbours vector
			if (tileWalkable(unitType, neighbourTile, end, isInteractableEndpoint)) {
				// Tile cost decided by euclidean distance
				const double gCost = currentNode.gCost + currentNode.tile.getApproxDistance(neighbourTile);
				const double hCost = neighbourTile.getApproxDistance(end);
				const double fCost = gCost + hCost;
				const Node neighbourNode = Node(neighbourTile, currentNode.tile, gCost, hCost, fCost);
				neighbours.push_back(neighbourNode);
			}
		}
	}

	return neighbours;
}

int AStar::TileToIndex(BWAPI::TilePosition tile) {
	return (tile.y * BWAPI::Broodwar->mapWidth()) + tile.x;
}

bool AStar::tileWalkable(BWAPI::UnitType unitType, BWAPI::TilePosition tile, BWAPI::TilePosition end, bool isInteractableEndpoint) {
	if (!tile.isValid()) {
		return false;
	}

	// If the tile is the end tile, return true. Further processing will depend on the boolean isInteractableEndpoint set in GeneratePath()
	if (tile == end && isInteractableEndpoint) {
		return true;
	}

	// Get measurements in terms of WalkPositions (8x8)
	const int unitWidth = unitType.width() / 8;
	const int unitHeight = unitType.height() / 8;

	// Check all WalkPositions the unit would inhabit if it reached the center of the tile
	const BWAPI::WalkPosition wpCenter = BWAPI::WalkPosition(tile);

	for (int xOffset = -unitWidth / 2; xOffset < unitWidth / 2; xOffset++) {
		for (int yOffset = -unitHeight / 2; yOffset < unitHeight / 2; yOffset++) {
			const BWAPI::WalkPosition pos = BWAPI::WalkPosition(wpCenter.x + xOffset, wpCenter.y + yOffset);

			if (!pos.isValid() || !BWAPI::Broodwar->isWalkable(pos)) {
				return false;
			}

			// BWAPI::Broodwar->isWalkable() only checks static terrain so we'll also need to check for buildings
			const BWAPI::Position topLeft = BWAPI::Position(pos.x * 8 - 4, pos.y * 8 - 4);
			const BWAPI::Position bottomRight = BWAPI::Position(pos.x * 8 + 4, pos.y * 8 + 4);
			if (rectCoordinates.size() < 10000) {
				rectCoordinates.push_back(std::make_pair(topLeft, bottomRight));
			}
			const BWAPI::Unitset& unitsInRect = BWAPI::Broodwar->getUnitsInRectangle(topLeft, bottomRight, BWAPI::Filter::IsBuilding);
			if (!unitsInRect.empty()) {
				return false;
			}
		}
	}

	return true;
}

void AStar::drawPath(Path path) {
	if (path.positions.size() <= 1) {
		return;
	}

	BWAPI::Position prevPos = path.positions.at(0);
	for (const BWAPI::Position pos : path.positions) {
		BWAPI::Broodwar->drawLineMap(prevPos, pos, BWAPI::Colors::Yellow);
		BWAPI::Broodwar->drawCircleMap(pos, 3, BWAPI::Colors::Black, true);
		BWAPI::Broodwar->drawCircleMap(pos, 20, BWAPI::Colors::Grey);
		prevPos = pos;
	}

	BWAPI::Broodwar->drawCircleMap(path.positions.at(0), 5, BWAPI::Colors::Green, true);
	BWAPI::Broodwar->drawCircleMap(path.positions.at(path.positions.size() - 1), 5, BWAPI::Colors::Red, true);
}