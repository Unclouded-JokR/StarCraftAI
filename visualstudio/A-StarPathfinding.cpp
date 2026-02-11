#include "A-StarPathfinding.h"
#include <queue>

vector<std::pair<BWAPI::Position, BWAPI::Position>> rectCoordinates;
vector<std::pair<BWAPI::TilePosition, double>> closedTiles;

// Generates path from start to end using A* pathfinding algorithm
// IF YOU WANT PATH TO AN INTERACTABLE UNIT (Constructing a geyser, mining minerals, etc.), SET isInteractableEndpoint AS TRUE
Path AStar::GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint) {
	Timer timer = Timer();
	timer.start();
	rectCoordinates.clear();
	closedTiles.clear();

	//cout << "Actual distance: " << _start.getDistance(_end) << endl;
	//cout << "Heuristic distance: " << chebyshevDistance(_start, _end) << endl;

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
	priority_queue<Node, vector<Node>, greater<Node>> openSet;
	unordered_set<Node, NodeHash> openSetNodes;

	// Optimization using array for closed set
	const int mapWidth = BWAPI::Broodwar->mapWidth();
	const int mapHeight = BWAPI::Broodwar->mapHeight();
	vector<bool> closedSet(mapWidth * mapHeight, false); // Default: no closed tiles
	vector<double> gCostMap(mapWidth * mapHeight, DBL_MAX); // Default: all gCosts are as large as possible

	// Store parents in an unordered_map for reconstructing path later
	unordered_map<int, BWAPI::TilePosition> parent;

	// Very first node to be added is the starting tile position
	Node firstNode = Node(start, start, 0, 0, 0);
	openSet.push(firstNode);
	openSetNodes.insert(firstNode);
	gCostMap[TileToIndex(start)] = 0;

	while (openSet.size() > 0) {
		Node currentNode = openSet.top();

		// Check if path is finishedx
		if (currentNode.tile == end) {
			int distance = 0;
			BWAPI::Position prevPos = BWAPI::Position(currentNode.tile);
			BWAPI::Position dir;
			BWAPI::Position currentWaypoint;
			BWAPI::Position prevWaypoint;

			while (currentNode.tile != start) {
				// FIX (Units stuck on assimilator) : If endpoint is interactable, do not center the tile
				if (isInteractableEndpoint && currentNode.tile == end) {
					tiles.push_back(BWAPI::Position(currentNode.tile));
				}
				else {
					tiles.push_back(BWAPI::Position(currentNode.tile) + BWAPI::Position(16, 16));
				}

				currentNode.tile = parent[TileToIndex(currentNode.tile)];
				const BWAPI::Position currentPos = BWAPI::Position(currentNode.tile);

				if (tiles.size() == 2) {
					dir = tiles.at(1) - tiles.at(0);
					distance += tiles.at(1).getApproxDistance(tiles.at(0));
				}
				else if (tiles.size() > 2) {
					BWAPI::Position currentWaypoint = tiles.at(tiles.size() - 1);
					BWAPI::Position prevWaypoint = tiles.at(tiles.size() - 2);
					const BWAPI::Position newDir = currentWaypoint - prevWaypoint;
					const double currentDistance = currentWaypoint.getApproxDistance(prevWaypoint);

					if (dir == newDir) {
						distance += currentWaypoint.getApproxDistance(tiles.at(tiles.size() - 3));
						tiles.erase(tiles.end() - 2);
					}
					else {
						dir = newDir;
						distance += currentDistance;
					}
				}

				const BWAPI::Unit closestObstacle = BWAPI::Broodwar->getClosestUnit(currentWaypoint, (BWAPI::Filter::IsBuilding || BWAPI::Filter::IsResourceContainer), unitType.width() / 2 + 5);
				if (closestObstacle != nullptr && closestObstacle->exists()) {
					const BWAPI::Position buildingDir = closestObstacle->getPosition() - currentWaypoint;
					int xOffset = 0;
					int yOffset = 0;

					if (buildingDir.x < 0) {
						xOffset += 5;
					}
					if (buildingDir.y < 0) {
						yOffset += 5;
					}

					if (buildingDir.x > 0) {
						xOffset -= 5;
					}
					if (buildingDir.y > 0) {
						yOffset -= 5;
					}

					const BWAPI::Position newWaypoint = BWAPI::Position(currentWaypoint.x + xOffset, currentWaypoint.y + yOffset);
					dir = newWaypoint - prevWaypoint;
					tiles.erase(tiles.end() - 1);
					tiles.push_back(newWaypoint);
				}
		
				prevPos = currentPos;
			}
			
			if (tiles.size() < 1) {
				tiles.push_back(_start);
				distance += prevPos.getApproxDistance(_start);
			}

			// Since we're pushing to the tile vector from end to start, we need to reverse it afterwards
			reverse(tiles.begin(), tiles.end());

			timer.stop();
			//std::cout << "Milliseconds spent generating path: " << timer.getElapsedTimeInMilliSec() << endl;
			return Path(tiles, distance);
		}

		openSet.pop();
		openSetNodes.erase(currentNode);
		closedSet[TileToIndex(currentNode.tile)] = true;
		closedTiles.push_back(std::make_pair(currentNode.tile, currentNode.fCost));

		// Step through each neighbour of the current node and add to open set if valid and not in closed set yet
		// Neighbour's walkability is already checked inside of getNeighbours()
		for (const auto neighbour : getNeighbours(unitType, currentNode, end, isInteractableEndpoint)) {
			// Don't visit neighbour nodes that have already been evaluated
			if (closedSet[TileToIndex(neighbour.tile)]) {
				continue;
			}

			// We'll skip the neighbour if its not valid
			if (!neighbour.tile.isValid()) {
				continue;
			}
			
			// If the neighbouring node is not in the open set yet, then add it.
			// If its already in the open set, see if this new path is better.
			if (!openSetNodes.contains(neighbour)) {
				openSet.push(neighbour);
				openSetNodes.insert(neighbour);
			}
			else if (gCostMap[TileToIndex(neighbour.tile)] < neighbour.gCost) {
				continue;
			}

			parent[TileToIndex(neighbour.tile)] = currentNode.tile;
			gCostMap[TileToIndex(neighbour.tile)] = neighbour.gCost;
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
				// For gcost, I assume an orthogonal cost of 1 (1 pixel movement). I approximate diagonal movement to 1.414 (square root of 2).
				const double gCost = currentNode.gCost + ((x != 0 && y != 0) ? 45.255 : 32.0);

				// Heuristic done using squaredDistance to avoid expensive sqrt() in euclidean distance across large distances
				// TODO: Fix heuristic causing units to hug walls all the time
				const double hCost = octileDistance(BWAPI::Position(neighbourTile), BWAPI::Position(end));
				const double fCost = gCost + hCost;
				//cout << "Tile costs: " << gCost << " | " << hCost << " | " << fCost << endl;
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

	// If the tile is the end tile, returns true. Further processing will depend on the boolean isInteractableEndpoint set in GeneratePath()
	if (tile == end && isInteractableEndpoint) {
		return true;
	}

	// Gets measurements in terms of WalkPositions (8x8)
	const int unitWidth = unitType.width() / 8;
	const int unitHeight = unitType.height() / 8;

	const BWAPI::Position tileCenter = BWAPI::Position(tile) + BWAPI::Position(16, 16);
	if (!tileCenter.isValid()) {
		return false;
	}

	// Checks all WalkPositions the unit would inhabit if it reached the center of the tile
	for (int xOffset = -unitWidth / 2; xOffset < unitWidth / 2; xOffset++) {
		for (int yOffset = -unitHeight / 2; yOffset < unitHeight / 2; yOffset++) {
			const BWAPI::WalkPosition pos = BWAPI::WalkPosition(tileCenter) + BWAPI::WalkPosition(xOffset, yOffset);;
			if (!BWAPI::Broodwar->isWalkable(pos)) {
				return false;
			}
		}
	}

	const BWAPI::Position topLeft = BWAPI::Position(tileCenter.x - (unitType.width() / 2), tileCenter.y - (unitType.height() / 2));
	const BWAPI::Position bottomRight = BWAPI::Position(tileCenter.x + (unitType.width() / 2), tileCenter.y + (unitType.height() / 2));
	if (rectCoordinates.size() < 10000) {
		rectCoordinates.push_back(std::make_pair(topLeft, bottomRight));
	}
	
	// Checks if any buildings are touching where the unit would be in the middle of the tile
	const BWAPI::Unitset& unitsInRect = BWAPI::Broodwar->getUnitsInRectangle(topLeft, bottomRight, BWAPI::Filter::IsBuilding);
	if (!unitsInRect.empty()) {
		return false;
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
		prevPos = pos;
	}

	BWAPI::Broodwar->drawCircleMap(path.positions.at(0), 5, BWAPI::Colors::Green, true);
	BWAPI::Broodwar->drawCircleMap(path.positions.at(path.positions.size() - 1), 5, BWAPI::Colors::Red, true);
}

double AStar::squaredDistance(BWAPI::Position pos1, BWAPI::Position pos2) {
	return pow((pos2.x - pos1.x), 2) + pow((pos2.y - pos1.y), 2);
}
double AStar::chebyshevDistance(BWAPI::Position pos1, BWAPI::Position pos2) {
	int val = max(abs(pos2.x - pos1.x), abs(pos2.y - pos1.y));
	/*if (pos1.x != pos2.x && pos1.y != pos2.y) {
		return 1.414 * val;
	}*/
	
	return val;
}
double AStar::octileDistance(BWAPI::Position pos1, BWAPI::Position pos2) {
	int dx = pos2.x - pos1.x;
	int dy = pos2.y - pos1.y;
	return (1.414 * min(dx, dy)) + (1.0 * abs(dx - dy));
}