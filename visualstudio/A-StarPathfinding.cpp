#include "A-StarPathfinding.h"
#include <queue>


// Global variables used to debug path generation
vector<std::pair<BWAPI::Position, BWAPI::Position>> rectCoordinates;
vector<std::pair<BWAPI::TilePosition, double>> closedTiles;
vector<BWAPI::TilePosition> earlyExpansionTiles;

// Initializing static variables 
map<pair<const BWEM::Area::id, const BWEM::Area::id>, const Path> AStar::AreaPathCache;
static map<pair<const BWAPI::TilePosition, const BWAPI::TilePosition>, const Path> BasePathCache;
map<pair<const BWEM::ChokePoint*, const BWEM::ChokePoint*>, Path> AStar::ChokepointPathCache;
vector<pair<const BWAPI::WalkPosition, const BWAPI::WalkPosition>> AStar::UncachedAreaPairs;
vector<BWAPI::Position> precachedPositions;


// Generates path from start to end using A* pathfinding algorithm
// IF YOU WANT PATH TO AN INTERACTABLE UNIT (Constructing a geyser, mining minerals, etc.), SET isInteractableEndpoint AS TRUE
Path AStar::GeneratePath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint) {
	Timer totalTimer = Timer();
	totalTimer.start();
#ifdef DEBUG_PATH
	rectCoordinates.clear();
	closedTiles.clear();
	earlyExpansionTiles.clear();
#endif

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
		finalDistance = _start.getApproxDistance(_end);
		return Path(positions, finalDistance);
	}

	BWAPI::TilePosition start = BWAPI::TilePosition(_start);
	const BWAPI::TilePosition end = BWAPI::TilePosition(_end);

	// Check for precached route before generating
	const BWEM::Area* area1 = bwem_map.GetArea(start);
	const BWEM::Area* area2 = bwem_map.GetArea(end);

	bool precached = false;
	Path precachedPath = Path();
	if (area1 != nullptr && area2 != nullptr && area1->Id() != area2->Id()) {
		if ((AreaPathCache.contains(make_pair(area1->Id(), area2->Id())) || AreaPathCache.contains(make_pair(area2->Id(), area1->Id()))) && area1->AccessibleFrom(area2)) {
			// Two cases: 
			// - area1 and area2 are not neighbours (get shortest path of chokepoints from 1 to)
			// - area1 and area2 are neighbours (have to find closest path that goes to the chokepoint between area1 and area2)

			// If area1 and area2 are not neighbours, simply path through the entire cached path
			if (!contains(area1->AccessibleNeighbours(), area2)) {
				pair<const BWEM::Area*, const BWEM::Area*> pair = make_pair(area1, area2);
				precachedPath = AreaPathCache[make_pair(area1->Id(), area2->Id())];

				// Since cached paths go from smaller ID to larger ID, if area1 has the larger ID, flip the positions in the precachedPath
				if (area1->Id() > area2->Id()) {
					precachedPath.flip();
				}

				if (precachedPath.positions.size() > 0) {
					Path startPath = generateSubPath(_start, unitType, precachedPath.positions.at(0));
					Path endPath = generateSubPath(precachedPath.positions.at(precachedPath.positions.size() - 1), unitType, _end);

					vector<BWAPI::Position> finalVec;
					// Attatching start
					finalVec.insert(finalVec.end(), startPath.positions.begin(), startPath.positions.end());
					// Attatching precache
					finalVec.insert(finalVec.end(), precachedPath.positions.begin(), precachedPath.positions.end());
					// Attatching end
					finalVec.insert(finalVec.end(), endPath.positions.begin(), endPath.positions.end());
					const int finalDist = startPath.distance + precachedPath.distance + endPath.distance;

#ifdef DEBUG_PATH
					cout << "Using cached path: " << "start size: " << startPath.positions.size() << " | " << "precache size: " << precachedPath.positions.size() << " | " << "end size: " << endPath.positions.size() << endl;
#endif
					totalTimer.stop();
					if (finalVec.size() > 0) {
						return Path(finalVec, finalDist);
					}
				}
			}
			// If they are neigbours, try to find a nearby cached path to the end area that we can attach to
			else {
				Path subPath = closestCachedPath(_start, _end);
				Path startPath = GeneratePath(_start, unitType, subPath.positions.at(0));
				Path endPath = GeneratePath(subPath.positions.at(subPath.positions.size() - 1), unitType, _end);

				vector<BWAPI::Position> finalVec;
				// Attatching start
				finalVec.insert(finalVec.end(), startPath.positions.begin(), startPath.positions.end());
				// Attatching precache
				finalVec.insert(finalVec.end(), precachedPath.positions.begin(), precachedPath.positions.end());
				// Attatching end
				finalVec.insert(finalVec.end(), endPath.positions.begin(), endPath.positions.end());
				const int finalDist = startPath.distance + subPath.distance + endPath.distance;

#ifdef DEBUG_PATH
				cout << "Connected to nearby cached path: " << startPath.positions.size() << " | " << precachedPath.positions.size() << " | " << endPath.positions.size() << endl;
#endif
				totalTimer.stop();
				if (finalVec.size() > 0) {
					return Path(finalVec, finalDist);
				}
			}
		}
	}

	// Optimization using priority_queue for open set
	// The priority_queue will always have the node with the lowest fCost at the top
	// No need for O(N) lookup time :)
	priority_queue<Node, vector<Node>, greater<Node>> openSet;

	unordered_set<BWAPI::TilePosition, TilePositionHash> openSetNodes;

	// Optimization using array for closed set
	// O(1) lookup time :)
	const int mapWidth = BWAPI::Broodwar->mapWidth();
	const int mapHeight = BWAPI::Broodwar->mapHeight();
	vector<bool> closedSet(mapWidth * mapHeight, false); // Default: no closed positions
	vector<int> gCostMap(mapWidth * mapHeight, INT_MAX); // Default: all gCosts are as large as possible

	// Store parents in an unordered_map for reconstructing path later
	unordered_map<int, BWAPI::TilePosition> parent;

	// Very first node to be added is the starting tile position
	Node firstNode = Node(start, start, 0, 0, 0);
	openSet.push(firstNode);
	openSetNodes.insert(start);
	gCostMap[TileToIndex(start)] = 0;
	Node currentNode = Node();

	bool earlyExpansion = false;
	while (openSet.size() > 0) {
		// Time limit for path generations
		if (TIME_LIMIT_ENABLED && totalTimer.getElapsedTimeInMilliSec() > TIME_LIMIT_MS) {
			return Path();
		}

		if (!earlyExpansion) {
			currentNode = openSet.top();
			openSet.pop();
			openSetNodes.erase(currentNode.tile);
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


			// Since we're pushing to the tile vector from end to start, we need to reverse it afterwards
			reverse(positions.begin(), positions.end());

#ifdef DEBUG_PATH
			endTimer.stop();
			cout << "Time spent backtracking + smoothing path: " << endTimer.getElapsedTimeInMilliSec() << endl;
			totalTimer.stop();
			cout << "Milliseconds spent generating path: " << totalTimer.getElapsedTimeInMilliSec() << " ms" << endl;
#endif
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

					// CHANGE: Using integers for gCost and hCost for efficiency while sacrificing some accuracy across long distances
					// Makes use of how optimized BWAPI's getApproxDistance is
					const int gCost = currentNode.gCost + currentNode.tile.getApproxDistance(neighbourTile) * ((x != 0 && y != 0) ? 14 : 10);
					// If neighbour is revisited but doesn't have a lower cost than what was previously evaluated, skip it
					if (closedSet[TileToIndex(neighbourTile)] && gCost >= gCostMap[TileToIndex(neighbourTile)]) {
						continue;
					}

					if (!openSetNodes.contains(neighbourTile) || gCost < gCostMap[TileToIndex(neighbourTile)]) {
						const int hCost = neighbourTile.getApproxDistance(end) * ((x != 0 && y != 0) ? 14 : 10);
						const double fCost = gCost + (HEURISTIC_WEIGHT * hCost);

						//cout << "Tile costs: " << gCost << " | " << hCost << " | " << fCost << endl;
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

						if (!openSetNodes.contains(neighbourTile)) {
							openSet.push(neighbourNode);
							openSetNodes.insert(neighbourTile);
						}
						parent[TileToIndex(neighbourTile)] = currentNode.tile;
						gCostMap[TileToIndex(neighbourTile)] = gCost;
					}
				}
			}
		}
		neighbourTimer.stop();
	}

	return Path();
}

// Behaves similarly to GeneratePath but does not check for subpaths. Meant to generate paths in-between chokepoints.
Path AStar::generateSubPath(BWAPI::Position _start, BWAPI::UnitType unitType, BWAPI::Position _end, bool isInteractableEndpoint) {
	Timer subTimer = Timer();
	subTimer.start();

	vector<BWAPI::Position> positions = vector<BWAPI::Position>();
	vector<BWAPI::Position> subPathPositions = vector<BWAPI::Position>();
	int finalDistance = 0;
	// Checks if either the starting or ending tile positions are invalid
	// If so, returns early with no positions added to the path
	if (!_start.isValid() || !_end.isValid()) {
		cout << "ERROR: Starting point or ending point not valid" << endl;
		subTimer.stop();
		return Path();
	}

	// Flying units can move directly to the end position without pathfinding
	if (unitType.isFlyer()) {
		positions.push_back(_end);
		finalDistance = _start.getApproxDistance(_end);
		subTimer.stop();
		return Path(positions, finalDistance);
	}

	BWAPI::TilePosition start = BWAPI::TilePosition(_start);
	const BWAPI::TilePosition end = BWAPI::TilePosition(_end);

	// Optimization using priority_queue for open set
	// The priority_queue will always have the node with the lowest fCost at the top
	// No need for O(N) lookup time :)
	priority_queue<Node, vector<Node>, greater<Node>> openSet;

	unordered_set<BWAPI::TilePosition, TilePositionHash> openSetNodes;

	// Optimization using array for closed set
	// O(1) lookup time :)
	const int mapWidth = BWAPI::Broodwar->mapWidth();
	const int mapHeight = BWAPI::Broodwar->mapHeight();
	vector<bool> closedSet(mapWidth * mapHeight, false); // Default: no closed positions
	vector<int> gCostMap(mapWidth * mapHeight, INT_MAX); // Default: all gCosts are as large as possible

	// Store parents in an unordered_map for reconstructing path later
	unordered_map<int, BWAPI::TilePosition> parent;

	// Very first node to be added is the starting tile position
	Node firstNode = Node(start, start, 0, 0, 0);
	openSet.push(firstNode);
	openSetNodes.insert(start);
	gCostMap[TileToIndex(start)] = 0;
	Node currentNode = Node();

	bool earlyExpansion = false;
	while (openSet.size() > 0) {
		// Time limit for path generations
		if (TIME_LIMIT_ENABLED && subTimer.getElapsedTimeInMilliSec() > TIME_LIMIT_MS) {
			subTimer.stop();
			cout << "TIME LIMIT REACHED: Empty path returned." << endl;
			return Path();
		}

		if (!earlyExpansion) {
			currentNode = openSet.top();
			openSet.pop();
			openSetNodes.erase(currentNode.tile);
		}
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

				finalDistance += currentPos.getApproxDistance(prevPos);

				// Don't smooth cached paths since units need to grab the nearest path that will take them to the chokepoint of the area they want to go to

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

#ifdef DEBUG_SUBPATH
			subTimer.stop();
			cout << "Milliseconds spent generating SUB path: " << subTimer.getElapsedTimeInMilliSec() << " ms" << endl;
#endif
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

					// CHANGE: Using integers for gCost and hCost for efficiency while sacrificing some accuracy across long distances
					// Makes use of how optimized BWAPI's getApproxDistance is
					const int gCost = currentNode.gCost + currentNode.tile.getApproxDistance(neighbourTile) * ((x != 0 && y != 0) ? 14 : 10);
					// If neighbour is revisited but doesn't have a lower cost than what was previously evaluated, skip it
					if (closedSet[TileToIndex(neighbourTile)] && gCost >= gCostMap[TileToIndex(neighbourTile)]) {
						continue;
					}

					if (!openSetNodes.contains(neighbourTile) || gCost < gCostMap[TileToIndex(neighbourTile)]) {
						const int hCost = neighbourTile.getApproxDistance(end) * ((x != 0 && y != 0) ? 14 : 10);
						const double fCost = gCost + (HEURISTIC_WEIGHT * hCost);

						//cout << "Tile costs: " << gCost << " | " << hCost << " | " << fCost << endl;
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

						if (!openSetNodes.contains(neighbourTile)) {
							openSet.push(neighbourNode);
							openSetNodes.insert(neighbourTile);
						}
						parent[TileToIndex(neighbourTile)] = currentNode.tile;
						gCostMap[TileToIndex(neighbourTile)] = gCost;
					}
				}
			}
		}
		neighbourTimer.stop();
	}

	return Path();
}

void AStar::fillUncachedAreaPairs() {
	const vector<BWEM::Area>& areas = bwem_map.Areas();
	for (int i = 0; i < areas.size(); i++) {
		for (int j = i + 1; j < areas.size(); j++) {
			// Have to do this to since bwem_map.Areas() doesn't return pointers so you can't make pairs of pointers with them
			const BWEM::Area& area1 = *bwem_map.GetArea(BWAPI::WalkPosition(areas.at(i).Top()));
			const BWEM::Area& area2 = *bwem_map.GetArea(BWAPI::WalkPosition(areas.at(j).Top()));

			// Avoid areas that cannot reach each other or areas that are already next to each other
			const vector<const BWEM::Area*> neighbours = area1.AccessibleNeighbours();
			if (!area1.AccessibleFrom(&area2) || find(neighbours.begin(), neighbours.end(), &area2) != neighbours.end()) {
				continue;
			}

			// pair isn't working with const BWEM::Area& for some reason so switching to walkpositions so you can get area from bwem_map.GetArea(wp)
			if (area1.Id() < area2.Id()) {
				pair<const BWAPI::WalkPosition, const BWAPI::WalkPosition> areaPair = make_pair(area1.Top(), area2.Top());
				UncachedAreaPairs.push_back(areaPair);
			}
			else {
				pair<const BWAPI::WalkPosition, const BWAPI::WalkPosition> areaPair = make_pair(area2.Top(), area1.Top());
				UncachedAreaPairs.push_back(areaPair);
			}
		}
	}
}

void AStar::fillAreaPathCache() {
#ifdef DEBUG_PRECACHE
	Timer pathCacheTimer = Timer();
	pathCacheTimer.start();
#endif

	if (UncachedAreaPairs.empty()) {
		return;
	}

	const BWEM::Area& area1 = *bwem_map.GetArea(UncachedAreaPairs.at(UncachedAreaPairs.size() - 1).first);
	const BWEM::Area& area2 = *bwem_map.GetArea(UncachedAreaPairs.at(UncachedAreaPairs.size() - 1).second);
	UncachedAreaPairs.pop_back();

	const vector<const BWEM::ChokePoint*> chokepoints1 = area1.ChokePoints();
	const vector<const BWEM::ChokePoint*> chokepoints2 = area2.ChokePoints();

	if (chokepoints1.empty() || chokepoints2.empty()) {
#ifdef DEBUG_PRECACHE
		cout << "No checkpoints found between areas" << endl;
#endif
		return;
	}

	int closest = INT_MAX;
	pair<const BWEM::ChokePoint*, const BWEM::ChokePoint*> closestCPPair;

	for (const BWEM::ChokePoint* cp1 : chokepoints1) {
		if (cp1->Blocked()) {
			continue;
		}

		for (const BWEM::ChokePoint* cp2 : chokepoints2) {
			if (cp2->Blocked()) {
				continue;
			}

			const BWAPI::WalkPosition cp1_center = cp1->Center();
			const BWAPI::WalkPosition cp2_center = cp2->Center();

			const int dist = cp1_center.getApproxDistance(cp2_center);
			if (dist < closest) {
				closest = dist;
				closestCPPair = make_pair(cp1, cp2);
			}
		}
	}

	// Group paths bewteen the shortest path of chokepoints between the chosen chokepoints of both areas
	const BWEM::CPPath smallestCPPath = bwem_map.GetPath(BWAPI::Position(closestCPPair.first->Center()), BWAPI::Position(closestCPPair.second->Center()));

	vector<BWAPI::Position> finalPositions;
	int finalDistance = 0;
	for (int k = 0; k < smallestCPPath.size() - 1 && smallestCPPath.size() > 1; k++) {
		const BWEM::ChokePoint* cp1 = smallestCPPath.at(k);
		const BWEM::ChokePoint* cp2 = smallestCPPath.at(k + 1);

		if (cp1 == nullptr || cp2 == nullptr) {
			return;
		}

		// Caches the paths between chokepoints so that other paths between Areas don't generate them again
		Path subPath;
		pair<const BWEM::ChokePoint*, const BWEM::ChokePoint*> cpPair = make_pair(cp1, cp2);
		if (ChokepointPathCache.find(make_pair(cp1, cp2)) != ChokepointPathCache.end()) {
			subPath = ChokepointPathCache[make_pair(cp1, cp2)];
		}
		else {
			subPath = generateSubPath(BWAPI::Position(cp1->Center()), BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(cp2->Center()));
			ChokepointPathCache[make_pair(cp1, cp2)] = subPath;

#ifdef DEBUG_PRECACHE
			precachedPositions.insert(precachedPositions.end(), subPath.positions.begin(), subPath.positions.end());
#endif
		}

		finalPositions.insert(finalPositions.end(), subPath.positions.begin(), subPath.positions.end());
		finalDistance += subPath.distance;
	}

	pair<const BWEM::Area::id, const BWEM::Area::id> finalPair = make_pair(area1.Id(), area2.Id());
	const Path finalPath = Path(finalPositions, finalDistance);
	AreaPathCache.insert(make_pair(finalPair, finalPath));

#ifdef DEBUG_PRECACHE
	pathCacheTimer.stop();
	cout << "Time spent precaching path of size (" << finalPositions.size() << "): " << pathCacheTimer.getElapsedTimeInMilliSec() << endl;
#endif
}

void AStar::clearPathCache() {
	AreaPathCache.clear();
	ChokepointPathCache.clear();
	UncachedAreaPairs.clear();
}

// Assumes you've already checked if the area that these two positions are in are neighbours
Path AStar::closestCachedPath(BWAPI::Position pos, BWAPI::Position goal) {
	const BWEM::Area* startArea = bwem_map.GetArea(BWAPI::WalkPosition(pos));
	const BWEM::Area* endArea = bwem_map.GetArea(BWAPI::WalkPosition(goal));

	const vector<const BWEM::Area*> neighbours = startArea->AccessibleNeighbours();

	Path closestPath = Path();
	int closestDist = INT_MAX;
	int closestIdx = -1;
	for (const auto& neighbour : neighbours) {
		if (neighbour->Id() == endArea->Id()
			|| !AreaPathCache.contains(make_pair(startArea->Id(), neighbour->Id()))
			|| !AreaPathCache.contains(make_pair(neighbour->Id(), startArea->Id()))) {
			continue;
		}

		Path precachedPath;
		if (neighbour->Id() < endArea->Id()) {
			if (!AreaPathCache.contains(make_pair(neighbour->Id(), startArea->Id()))) {
				continue;
			}
			precachedPath = AreaPathCache[make_pair(neighbour->Id(), endArea->Id())];
		}
		else {
			if (!AreaPathCache.contains(make_pair(startArea->Id(), neighbour->Id()))) {
				continue;
			}
			precachedPath = AreaPathCache[make_pair(endArea->Id(), neighbour->Id())];
		}

		for (int i = 0; i < precachedPath.positions.size(); i++) {
			const BWAPI::Position pathPos = precachedPath.positions.at(i);
			const int dist = pos.getApproxDistance(pathPos);
			if (dist < closestDist) {
				closestPath = precachedPath;
				closestDist = dist;
				closestIdx = i;
			}
		}
	}

	// Need to chop off part of the closestPath thats not needed
	vector<BWAPI::Position> finalVec(closestPath.positions.begin() + closestIdx, closestPath.positions.end());
	int finalDist = 0;
	for (int i = 0; i < finalVec.size(); i++) {
		if (i == 0) {
			continue;
		}
		finalDist += finalVec.at(i).getApproxDistance(finalVec.at(i-1));
	}

	Path subPath = Path(finalVec, finalDist);

	return subPath;
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
	const BWAPI::Unitset& unitsInRect = BWAPI::Broodwar->getUnitsInRectangle(topLeft, bottomRight, BWAPI::Filter::IsBuilding || (BWAPI::Filter::IsNeutral && !BWAPI::Filter::IsCritter));
	if (!unitsInRect.empty()) {
		return false;
	}

#ifdef DEBUG_PATH
	if (rectCoordinates.size() < 10000) {
		rectCoordinates.push_back(std::make_pair(topLeft, bottomRight));
	}
#endif

	return true;
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