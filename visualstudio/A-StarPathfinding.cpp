#include "A-StarPathfinding.h"
#include <queue>

vector<std::pair<BWAPI::Position, BWAPI::Position>> rectCoordinates;
vector<std::pair<BWAPI::TilePosition, double>> closedTiles;
vector<BWAPI::TilePosition> earlyExpansionTiles;

// Generates path from start to end using A* pathfinding algorithm
// IF YOU WANT PATH TO AN INTERACTABLE UNIT (Constructing a geyser, mining minerals, etc.), SET isInteractableEndpoint AS TRUE
Path AStar::GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint) {
	Timer totalTimer = Timer();
	totalTimer.start();
	rectCoordinates.clear();
	closedTiles.clear();
	earlyExpansionTiles.clear();

	//cout << "Actual distance: " << _start.getDistance(_end) << endl;
	//cout << "Heuristic distance: " << chebyshevDistance(_start, _end) << endl;

	vector<BWAPI::Position> positions = vector<BWAPI::Position>();
	vector<BWAPI::Position> subPathPositions = vector<BWAPI::Position>();
	int finalDistance = 0;
	// Checks if either the starting or ending tile positions are invalid
	// If so, returns early with no positions added to the path
	if (!_start.isValid() || !_end.isValid()) {
		cout << "ERROR: Starting point or ending point not valid" << endl;
		return Path();
	}

	// Flying units can move directly to the end position without pathfinding
	if (unitType.isFlyer()) {
		positions.push_back(_end);
		finalDistance  = _start.getApproxDistance(_end);
		return Path(positions, finalDistance);
	}

	BWAPI::TilePosition start = BWAPI::TilePosition(_start);
	const BWAPI::TilePosition end = BWAPI::TilePosition(_end);

	// Optimization using subpath generation with BWEM
	// Checks BWEM for optimal path of chokepoints from start's area to end's area
	// Path generation is done to find subpaths between each chokepoint
	// Reduces search space for efficiency :)
	const BWEM::Area* area1 = nullptr;
	const BWEM::Area* area2 = nullptr;
	bool subPathFound = false;
	BWEM::CPPath optimalChokepointPath;

	if (!bwem_map.Areas().empty()) {
		area1 = bwem_map.GetNearestArea(start);
		area2 = bwem_map.GetNearestArea(end);
	}
	if ((area1 != nullptr && area2 != nullptr) && area1 != area2) {
		cout << "Subpath found. Generating subpaths between chokepoints" << endl;
		subPathFound = true;
		optimalChokepointPath = bwem_map.GetPath(_start, _end);

		// Start -> first chokepoint
		Path firstPath = generateSubPath(_start, unitType, BWAPI::Position(optimalChokepointPath[0]->Center()));
		subPathPositions.insert(subPathPositions.end(), firstPath.positions.begin(), firstPath.positions.end());
		if (firstPath.positions.empty()) {
			cout << "No Path Found : " << totalTimer.getElapsedTimeInMilliSec() << " ms" << endl;
			return Path();
		}

		// Chokepoints -> chokepoints
		for (int i = 0; i < optimalChokepointPath.size() - 1; i++) {
			const BWAPI::Position pos1 = BWAPI::Position(optimalChokepointPath[i]->Center());
			const BWAPI::Position pos2 = BWAPI::Position(optimalChokepointPath[i + 1]->Center());

			Path subPath = generateSubPath(pos1, unitType, pos2);
			if (subPath.positions.empty()) {
				cout << "No Path Found : " << totalTimer.getElapsedTimeInMilliSec() << " ms" << endl;
				return Path();
			}

			finalDistance += subPath.distance;
			subPathPositions.insert(subPathPositions.end(), subPath.positions.begin(), subPath.positions.end());
		}

		start = BWAPI::TilePosition(subPathPositions.back());
	}

	// Optimization using priority_queue for open set
	// The priority_queue will always have the node with the lowest fCost at the top
	// No need for O(N) lookup time :)
	priority_queue<Node, vector<Node>, greater<Node>> openSet;

	// Optimization using array for closed set
	// O(1) lookup time :)
	const int mapWidth = BWAPI::Broodwar->mapWidth();
	const int mapHeight = BWAPI::Broodwar->mapHeight();
	vector<bool> closedSet(mapWidth* mapHeight, false); // Default: no closed positions
	vector<int> gCostMap(mapWidth* mapHeight, INT_MAX); // Default: all gCosts are as large as possible

	// Store parents in an unordered_map for reconstructing path later
	unordered_map<int, BWAPI::TilePosition> parent;

	// Very first node to be added is the starting tile position
	Node firstNode = Node(start, start, 0, 0, 0);
	openSet.push(firstNode);
	gCostMap[TileToIndex(start)] = 0;
	Node currentNode = Node();

	bool earlyExpansion = false;
	while (openSet.size() > 0) {
		if (!earlyExpansion) {
			currentNode = openSet.top();
			openSet.pop();
		}
		closedSet[TileToIndex(currentNode.tile)] = true;
		closedTiles.push_back(std::make_pair(currentNode.tile, currentNode.fCost));

		// Check if path is finishedx
		if (currentNode.tile == end) {
			Timer endTimer = Timer();
			endTimer.start();
			BWAPI::Position prevPos = BWAPI::Position(currentNode.tile);
			BWAPI::Position dir;
			BWAPI::Position currentWaypoint;
			BWAPI::Position prevWaypoint;

			while (currentNode.tile != start) {
				if (isInteractableEndpoint && currentNode.tile == end) {
					positions.push_back(BWAPI::Position(currentNode.tile));
				}
				else {
					positions.push_back(BWAPI::Position(currentNode.tile) + BWAPI::Position(16, 16));
				}
				currentNode.tile = parent[TileToIndex(currentNode.tile)];
				const BWAPI::Position currentPos = BWAPI::Position(currentNode.tile);

				if (positions.size() == 2) {
					dir = positions.at(1) - positions.at(0);
					finalDistance += positions.at(1).getApproxDistance(positions.at(0));
				}
				else if (positions.size() > 2) {
					BWAPI::Position currentWaypoint = positions.at(positions.size() - 1);
					BWAPI::Position prevWaypoint = positions.at(positions.size() - 2);
					const BWAPI::Position newDir = currentWaypoint - prevWaypoint;
					const double currentDistance = currentWaypoint.getApproxDistance(prevWaypoint);

					if (dir == newDir) {
						finalDistance += currentWaypoint.getApproxDistance(positions.at(positions.size() - 3));
						positions.erase(positions.end() - 2);
					}
					else {
						dir = newDir;
						finalDistance += currentDistance;
					}
				}

				const BWAPI::Unit closestObstacle = BWAPI::Broodwar->getClosestUnit(currentWaypoint, (BWAPI::Filter::IsBuilding || BWAPI::Filter::IsSpecialBuilding), unitType.width() / 2 + 5);
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
					positions.erase(positions.end() - 1);
					positions.push_back(newWaypoint);
				}

				prevPos = currentPos;
			}

			if (!subPathFound && positions.size() > 1) {
				positions.push_back(_start);
				finalDistance += prevPos.getApproxDistance(_start);
			}

			// Since we're pushing to the tile vector from end to start, we need to reverse it afterwards
			reverse(positions.begin(), positions.end());
			if (subPathFound) {
				subPathPositions.insert(subPathPositions.end(), positions.begin(), positions.end());
				totalTimer.stop();
				cout << "Milliseconds spent generating path: " << totalTimer.getElapsedTimeInMilliSec() << " ms" << endl;
				return Path(subPathPositions, finalDistance);
			}

			endTimer.stop();
			cout << "Time spent backtracking + smoothing path: " << endTimer.getElapsedTimeInMilliSec() << endl;

			totalTimer.stop();
			cout << "Milliseconds spent generating path: " << totalTimer.getElapsedTimeInMilliSec() << " ms" << endl;
			return Path(positions, finalDistance);
		}

		// Step through each neighbour of the current node. The first neighbour that has a lower fCost is instantly explored next
		// Neighbour's walkability is already checked inside of getNeighbours()
		Timer neighbourTimer = Timer();
		neighbourTimer.start();
		for (int x = -1; x <= 1; x++) {
			for (int y = -1; y <= 1; y++) {

				// Ignore self
				if (x == 0 && y == 0) {
					continue;
				}

				// Check
				if (x != 0 && y != 0) {
					BWAPI::TilePosition a(currentNode.tile.x + x, currentNode.tile.y);
					BWAPI::TilePosition b(currentNode.tile.x, currentNode.tile.y + y);
					if (!tileWalkable(unitType, a, end, isInteractableEndpoint) || !tileWalkable(unitType, b, end, isInteractableEndpoint))
						continue;
				}

				const BWAPI::TilePosition neighbourTile = BWAPI::TilePosition(currentNode.tile.x + x, currentNode.tile.y + y);
				// If the neighbour tile is walkable, creates a Node of the neighbour tile and adds it to the neighbours vector
				if (tileWalkable(unitType, neighbourTile, end, isInteractableEndpoint)) {
					if (closedSet[TileToIndex(neighbourTile)]) {
						continue;
					}

					// CHANGE: Using integers for gCost and hCost for efficiency while sacrificing some accuracy across long distances
					// Makes use of how optimized BWAPI's getApproxDistance is
					const int gCost = currentNode.gCost + currentNode.tile.getApproxDistance(neighbourTile) * ((x != 0 && y != 0) ? 14 : 10);
					// Only process the rest of the neighbour if it's gCost is less than what was previously seen
					if (gCost < gCostMap[TileToIndex(neighbourTile)]) {
						// TODO: Optimize using pre-computed grid of hCosts. Only need 1 end node to base it off of.
						// TilePosition.getApproxDistance(goal) counts the x moves and y moves it would take to get to the goal (dx + dy basically).
						// Therefore, the hCost grid of any other goal is just an offset of the pre-computed value (since its just a coordinate shifted by integers)
						const int hCost = neighbourTile.getApproxDistance(end) * 10;
						const double fCost = gCost + (HEURISTIC_WEIGHT * hCost);

						cout << "Tile costs: " << gCost << " | " << hCost << " | " << fCost << endl;
						const Node neighbourNode = Node(neighbourTile, currentNode.tile, gCost, hCost, fCost);

						//cout << "Comparing neighbour and current fCost: " << neighbourNode.fCost << " | " << currentNode.fCost << endl;
// 
						// NOTE: For now, not using early neighbour expansion. Having issues with how the algorithm explores neighbours.
						/*if (neighbourNode.fCost < currentNode.fCost && earlyExpansion == false) {
							earlyNode = neighbourNode;
							cout << "Found early expansion" << endl;
							earlyExpansionTiles.push_back(earlyNode.tile);
							earlyExpansion = true;
						}*/

						openSet.push(neighbourNode);
						parent[TileToIndex(neighbourTile)] = currentNode.tile;
						gCostMap[TileToIndex(neighbourTile)] = gCost;
					}
				}
			}
		}
		neighbourTimer.stop();
		cout << "Time spent evaluating 8 neighbours: " << neighbourTimer.getElapsedTimeInMilliSec() << endl;
	}

	return Path();
}

// Generates path from start to end using A* pathfinding algorithm
// IF YOU WANT PATH TO AN INTERACTABLE UNIT (Constructing a geyser, mining minerals, etc.), SET isInteractableEndpoint AS TRUE
Path AStar::generateSubPath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint) {

	vector<BWAPI::Position> positions = vector<BWAPI::Position>();
	vector<BWAPI::Position> subPathPositions = vector<BWAPI::Position>();
	int finalDistance = 0;
	// Checks if either the starting or ending tile positions are invalid
	// If so, returns early with no positions added to the path
	if (!_start.isValid() || !_end.isValid()) {
		cout << "ERROR: Starting point or ending point not valid" << endl;
		return Path();
	}

	// Flying units can move directly to the end position without pathfinding
	if (unitType.isFlyer()) {
		positions.push_back(_end);
		finalDistance = _start.getApproxDistance(_end);
		return Path(positions, finalDistance);
	}

	BWAPI::TilePosition start = BWAPI::TilePosition(_start);
	const BWAPI::TilePosition end = BWAPI::TilePosition(_end);

	// Optimization using priority_queue for open set
	// The priority_queue will always have the node with the lowest fCost at the top
	// No need for O(N) lookup time :)
	priority_queue<Node, vector<Node>, greater<Node>> openSet;

	// Optimization using array for closed set
	// O(1) lookup time :)
	const int mapWidth = BWAPI::Broodwar->mapWidth();
	const int mapHeight = BWAPI::Broodwar->mapHeight();
	vector<bool> closedSet(mapWidth * mapHeight, false); // Default: no closed positions
	vector<double> gCostMap(mapWidth * mapHeight, DBL_MAX); // Default: all gCosts are as large as possible

	// Store parents in an unordered_map for reconstructing path later
	unordered_map<int, BWAPI::TilePosition> parent;

	// Very first node to be added is the starting tile position
	Node firstNode = Node(start, start, 0, 0, 0);
	openSet.push(firstNode);
	gCostMap[TileToIndex(start)] = 0;

	while (openSet.size() > 0) {
		bool earlyExpansion = false;
		Node currentNode = openSet.top();
		openSet.pop();
		closedSet[TileToIndex(currentNode.tile)] = true;
		closedTiles.push_back(std::make_pair(currentNode.tile, currentNode.fCost));

		// Check if path is finishedx
		if (currentNode.tile == end) {
			BWAPI::Position prevPos = BWAPI::Position(currentNode.tile);
			BWAPI::Position dir;
			BWAPI::Position currentWaypoint;
			BWAPI::Position prevWaypoint;

			while (currentNode.tile != start) {
				if (isInteractableEndpoint && currentNode.tile == end) {
					positions.push_back(BWAPI::Position(currentNode.tile));
				}
				else {
					positions.push_back(BWAPI::Position(currentNode.tile) + BWAPI::Position(16, 16));
				}
				currentNode.tile = parent[TileToIndex(currentNode.tile)];
				const BWAPI::Position currentPos = BWAPI::Position(currentNode.tile);

				if (positions.size() == 2) {
					dir = positions.at(1) - positions.at(0);
					finalDistance += positions.at(1).getApproxDistance(positions.at(0));
				}
				else if (positions.size() > 2) {
					BWAPI::Position currentWaypoint = positions.at(positions.size() - 1);
					BWAPI::Position prevWaypoint = positions.at(positions.size() - 2);
					const BWAPI::Position newDir = currentWaypoint - prevWaypoint;
					const double currentDistance = currentWaypoint.getApproxDistance(prevWaypoint);

					if (dir == newDir) {
						finalDistance += currentWaypoint.getApproxDistance(positions.at(positions.size() - 3));
						positions.erase(positions.end() - 2);
					}
					else {
						dir = newDir;
						finalDistance += currentDistance;
					}
				}

				const BWAPI::Unit closestObstacle = BWAPI::Broodwar->getClosestUnit(currentWaypoint, (BWAPI::Filter::IsBuilding || BWAPI::Filter::IsSpecialBuilding), unitType.width() / 2 + 5);
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
					positions.erase(positions.end() - 1);
					positions.push_back(newWaypoint);
				}

				prevPos = currentPos;
			}

			// Since we're pushing to the tile vector from end to start, we need to reverse it afterwards
			reverse(positions.begin(), positions.end());
			return Path(positions, finalDistance);
		}

		// Step through each neighbour of the current node. The first neighbour that has a lower fCost is instantly explored next
		// Neighbour's walkability is already checked inside of getNeighbours()
		for (int x = -1; x <= 1; x++) {
			for (int y = -1; y <= 1; y++) {

				// Ignore self
				if (x == 0 && y == 0) {
					continue;
				}
				if (BWEB::Map::isWalkable(currentNode.tile + BWAPI::TilePosition(x, y), unitType));

				// Check diagonals to avoid walking diagonally through corner where two unwalkable positions meet
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
					const double hCost = chebyshevDistance(BWAPI::Position(neighbourTile), BWAPI::Position(end));
					const double fCost = gCost + hCost;
					//cout << "Tile costs: " << gCost << " | " << hCost << " | " << fCost << endl;
					const Node neighbourNode = Node(neighbourTile, currentNode.tile, gCost, hCost, fCost);

					// Don't visit neighbour nodes that have already been evaluated
					if (closedSet[TileToIndex(neighbourNode.tile)]) {
						continue;
					}

					// We'll skip the neighbour if its not valid
					if (!neighbourNode.tile.isValid()) {
						continue;
					}

					if (neighbourNode.gCost >= gCostMap[TileToIndex(neighbourNode.tile)]) {
						continue;
					}

					if (neighbourNode.fCost < currentNode.fCost) {
						parent[TileToIndex(neighbourNode.tile)] = currentNode.tile;
						gCostMap[TileToIndex(neighbourNode.tile)] = neighbourNode.gCost;
						// Early expansion if lower cost than current node
						currentNode = neighbourNode;
					}
				}
			}
		}
	}

	return Path();
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
	// Checks if any buildings are touching where the unit would be in the middle of the tile
	const BWAPI::Unitset& unitsInRect = BWAPI::Broodwar->getUnitsInRectangle(topLeft, bottomRight, BWAPI::Filter::IsBuilding);
	if (!unitsInRect.empty()) {
		return false;
	}

	if (DEBUG_DRAW_GENERATION){
		if (rectCoordinates.size() < 10000) {
			rectCoordinates.push_back(std::make_pair(topLeft, bottomRight));
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
		prevPos = pos;
	}

	BWAPI::Broodwar->drawCircleMap(path.positions.at(0), 5, BWAPI::Colors::Green, true);
	BWAPI::Broodwar->drawCircleMap(path.positions.at(path.positions.size() - 1), 5, BWAPI::Colors::Red, true);
}

double AStar::squaredDistance(BWAPI::Position pos1, BWAPI::Position pos2) {
	return pow((pos2.x - pos1.x), 2) + pow((pos2.y - pos1.y), 2);
}
double AStar::chebyshevDistance(BWAPI::Position pos1, BWAPI::Position pos2) {
	double val = max(abs(pos2.x - pos1.x), abs(pos2.y - pos1.y));
	/*if (pos1.x != pos2.x && pos1.y != pos2.y) {
		return 1.414 * val;
	}*/
	
	return val;
}
double AStar::octileDistance(BWAPI::Position pos1, BWAPI::Position pos2) {
	double dx = abs(pos2.x - pos1.x);
	double dy = abs(pos2.y - pos1.y);
	double D1 = 1.0;
	double D2 = 1.414;
	return (D1 * max(dx, dy)) + ((D2 - D1) * min(dx, dy));
}