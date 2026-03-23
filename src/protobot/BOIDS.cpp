#include "BOIDS.h"

// Static variables
map<BWAPI::Unit, Squad*> CombatManager::unitSquadMap;
unordered_map<BWAPI::Unit, double, unitHash> BOIDS::leaderRadiusMap;
unordered_map<BWAPI::Unit, unordered_map<BWAPI::Unit, double, unitHash>, unitHash> BOIDS::unitDistanceCache;
unordered_map<BWAPI::Unit, VectorPos, unitHash> BOIDS::previousBOIDSMap;;

// Uses BOIDS algorithm to maintain formation while leader is moving
// leaderVec keeps units surround the leader
// separationVec keeps units from crowding too tightly
// terrainVec keeps units away from terrain
void BOIDS::squadFlock(Squad* squad) {
	unitDistanceCache.clear();

	for (const auto& unit : squad->units) {
		if (!unit->exists()) {
			continue;
		}

		// Ignore leader or if unit is attacking
		if (unit == squad->leader || unit->isAttackFrame()) {
			BWAPI::Broodwar->drawCircleMap(unit->getPosition(), MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);
			continue;
		}

		// Position info
		const VectorPos unitPos = VectorPos(unit->getPosition());
		const VectorPos leaderPos = VectorPos(squad->leader->getPosition());
		const double distUnitToLeader = unitPos.getApproxDistance(leaderPos);

		// Don't worry about flocking if unit is too far away from the leader
		if (distUnitToLeader > BOIDS_RANGE) {
			unit->attack(leaderPos);
			continue;
		}

		// Velocity info
		VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());
		VectorPos leaderVelocity = VectorPos(squad->leader->getVelocityX(), squad->leader->getVelocityY());

		// For drawing radiuses
		BWAPI::Broodwar->drawCircleMap(unitPos, MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawCircleMap(leaderPos, BOIDS_RANGE, BWAPI::Colors::Grey);

		// LEADER VECTOR
		VectorPos leaderVec = VectorPos(0, 0);
		const double spacing = 30.0;
		const double outer_radius = max(spacing * 1.5, spacing + sqrt(squad->units.size()) * spacing);
		BWAPI::Broodwar->drawCircleMap(leaderPos, outer_radius, BWAPI::Colors::Yellow);

		// Update leader radii map if needed
		if (leaderRadiusMap.find(squad->leader) == leaderRadiusMap.end() || leaderRadiusMap[squad->leader] != outer_radius) {
			leaderRadiusMap[squad->leader] = outer_radius;
		}

		leaderVec += leaderVelocity;

		// leaderVec stronger when farther from leader + includes velocity for grouping while moving
		if (distUnitToLeader > outer_radius) {
			leaderVec += (leaderPos - unitPos) * (1 / distUnitToLeader);
		}

		// SEPARATION VECTOR
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unitPos, MIN_SEPARATION_DISTANCE, BWAPI::Filter::IsAlly);
		VectorPos separationVec = VectorPos(0, 0);
		VectorPos leaderSeparationVec = VectorPos(0, 0);
		for (const auto& neighbor : neighbors) {
			// If neighbor is the current unit, dont process
			if (neighbor == unit) {
				continue;
			}

			const VectorPos neighborPos = VectorPos(neighbor->getPosition().x, neighbor->getPosition().y);

			// Don't apply separation to self if neighbor is a unit settling into their leader's radius
			// If neighbor is a leader, apply separation to self 
			if (CombatManager::unitSquadMap.find(neighbor) != CombatManager::unitSquadMap.end()) { // neighbor has a squad
				BWAPI::Unit neighborLeader = CombatManager::unitSquadMap[neighbor]->leader;
				VectorPos neighborLeaderPos = VectorPos(neighborLeader->getPosition());
				double distToNeighbor;

				// unitDistanceCache keeps distances between units for this frame.
				// If neighbor has not seen another unit yet or if the neighbor has not seen this unit, then this unit should store the distance first
				// Otherwise the neighbor has seen this unit before and has tracked the distance already so use that distance.
				if (unitDistanceCache.find(neighbor) != unitDistanceCache.end()) {
					if (unitDistanceCache[neighbor].find(unit) != unitDistanceCache[neighbor].end()) {
						distToNeighbor = unitDistanceCache[neighbor][unit];
					}
					else {
						distToNeighbor = unitPos.getDistance(neighborPos);
						unitDistanceCache[unit][neighbor] = distToNeighbor;
					}
				}
				else {
					distToNeighbor = unitPos.getDistance(neighborPos);
					unitDistanceCache[unit][neighbor] = distToNeighbor;
				}

				if (CombatManager::unitSquadMap[unit] == CombatManager::unitSquadMap[neighbor]) { // same squad
					if (CombatManager::unitSquadMap[neighbor]->leader == neighbor 
						|| !inLeaderRadius(neighborPos, leaderPos, leaderRadiusMap[squad->leader])){
						separationVec += (unitPos - neighborPos) * (1 / distToNeighbor);
					}
					else if (!inLeaderRadius(unitPos, leaderPos, leaderRadiusMap[squad->leader])
						&& inLeaderRadius(neighborPos, leaderPos, leaderRadiusMap[squad->leader])) { // unit not in radius but neighbor is
						separationVec += VectorPos(0, 0);
					}
					else {
						if (unitVelocity.getLength() == 0) {
							separationVec += (unitPos - neighborPos) * (1 / distToNeighbor);
							continue;
						}

						separationVec += getSeparationSteering(unitPos, neighborPos, unitVelocity);
					}
				}
				else if (CombatManager::unitSquadMap[neighbor]->leader == neighbor) { // neighbor is a leader of different squad
					leaderSeparationVec += (unitPos - neighborPos) * (1 / distToNeighbor);
				}
				else {
					if (unitVelocity.getLength() == 0) {
						separationVec += (unitPos - neighborPos) * (1 / distToNeighbor);
						continue;
					}
					separationVec += getSeparationSteering(unitPos, neighborPos, unitVelocity);
				}
			}
		}

		const VectorPos finalLeaderVec = leaderVec * LEADER_STRENGTH;
		const VectorPos finalSeparationVec = (leaderSeparationVec * LEADER_SEPARATION_STRENGTH) + (separationVec * SEPARATION_STRENGTH);

		// TERRAIN VECTOR
		const VectorPos finalTerrainVec = VectorPos(0, 0); //getTerrainVector(unit, squad->leader) * TERRAIN_STRENGTH;

		// BOIDS VECTOR
		VectorPos boidsVector = finalLeaderVec + finalSeparationVec + finalTerrainVec;

		// If the adjustment is not significant enough, don't move the unit
		const double boidsLength = boidsVector.getLength();
		if (boidsLength < 0.2) {
			continue;
		}

		// unit speed is pixels per frame so its scaled by frames skipped between boids calculations
		// At a minimum, scale by 50% of the distance. Too short was making units too slow.
		const double distScale = max(sqrt(distUnitToLeader / outer_radius), 0.75);
		boidsVector = boidsVector * distScale;

		// Clamping
		if (boidsLength < MIN_FORCE) {
			boidsVector = boidsVector.normalized() * MIN_FORCE;
		}
		else if (boidsLength > MAX_FORCE) {
			boidsVector = boidsVector.normalized() * MAX_FORCE;
		}

		// Cache boidsVector to smoothly apply changes instead of drastic ones (was causing too much jittering)
		if (previousBOIDSMap.find(unit) != previousBOIDSMap.end()) {
			boidsVector = previousBOIDSMap[unit] * 0.7 + boidsVector * 0.3;
			previousBOIDSMap[unit] = boidsVector;
		}
		else {
			previousBOIDSMap[unit] = boidsVector;
		}

		const VectorPos finalTarget = unitPos + boidsVector;

		unit->move(finalTarget);

#ifdef DEBUG_FLOCKING
		//cout << "Separation Vector : " << finalSeparationVec.getLength() << endl;
		//cout << "Terrain Vector : " << finalTerrainVec << endl;
		//cout << "Leader Vector : " << finalLeaderVec.getLength() << endl;
		//cout << "Boids Vector : " << boidsVector.getLength() << endl;
		//cout << "Final Vector : " << finalTarget << endl;
		//BWAPI::Broodwar->drawLineMap(unitPos, unitPos + separationVec * 10, BWAPI::Colors::Red);
		//BWAPI::Broodwar->drawLineMap(unitPos, unitPos + cohesionVec * 10, BWAPI::Colors::Yellow);
		//BWAPI::Broodwar->drawLineMap(unitPos, unitPos + alignmentVec * 10, BWAPI::Colors::Purple);
		//BWAPI::Broodwar->drawLineMap(unitPos, unitPos + leaderVec * 10, BWAPI::Colors::Blue);
		//BWAPI::Broodwar->drawLineMap(unitPos, unitPos + finalDirection * 10, BWAPI::Colors::Green);
		//BWAPI::Broodwar->printf("Separation magnitude: %f", BWAPI::Position(0, 0).getDistance(separationVec));
		//BWAPI::Broodwar->printf("Cohesion magnitude: %f", BWAPI::Position(0, 0).getDistance(cohesionVec));
		//BWAPI::Broodwar->printf("Alignment magnitude: %f", BWAPI::Position(0, 0).getDistance(alignmentVec));
		//BWAPI::Broodwar->printf("Leader magnitude: %f", BWAPI::Position(0, 0).getDistance(leaderDirection));
		//BWAPI::Broodwar->printf("FinalDirection magnitude: %f", unitPos.getDistance(finalDirection));
#endif
	}
}

VectorPos BOIDS::getSeparationSteering(VectorPos unitPos, VectorPos neighborPos, VectorPos unitVelocity) {
	const VectorPos toNeighbor = (neighborPos - unitPos).normalized();
	const VectorPos velocityDir = unitVelocity.normalized();

	const double crossProduct = velocityDir.x * toNeighbor.y - velocityDir.y * toNeighbor.x;
	// If crossProduct > 0, neighbor on left so need to go right
	// vice versa.
	VectorPos dir = crossProduct > 0 ? VectorPos(velocityDir.y, -velocityDir.x) : VectorPos(-velocityDir.y, velocityDir.x);

	return dir;
}

VectorPos BOIDS::getTerrainVector(BWAPI::Unit unit, BWAPI::Unit leader) {
	VectorPos terrainVec = VectorPos(0, 0);
	VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);

	const VectorPos forward = unitPos.normalized();
	const VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());
	const VectorPos forwardCheck = unitPos + unitVelocity + (forward * TERRAIN_LOOKAHEAD_LENGTH); // lookahead check for terrain
	BWAPI::Broodwar->drawLineMap(unitPos, forwardCheck, BWAPI::Colors::Green);

	if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(forwardCheck))) {
		// Rotate forward 90 degrees for side position
		const VectorPos sidePos = VectorPos(-forwardCheck.y, forwardCheck.x);

		// left and right probe aren't left and right of the unit but rather left and right of the forward vector
		// basically diagonals
		const VectorPos left = forward - sidePos;
		const VectorPos right = forward + sidePos;
		const VectorPos leftCheck = unitPos + left * TERRAIN_LOOKAHEAD_LENGTH;
		const VectorPos rightCheck = unitPos + right * TERRAIN_LOOKAHEAD_LENGTH;

		BWAPI::Broodwar->drawCircleMap(leftCheck, 15, BWAPI::Colors::Yellow, true);
		BWAPI::Broodwar->drawCircleMap(rightCheck, 15, BWAPI::Colors::Yellow, true);

		// Checks each probe for walkability. If neither are walkable, pick the side thats closer to the leader
		if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(leftCheck)) && !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(rightCheck))) {
			terrainVec = right;
		}
		else if (BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(leftCheck))) {
			terrainVec = left;
		}
		else {
			terrainVec = right;
		}
	}

	return terrainVec;
}

bool BOIDS::inLeaderRadius(VectorPos unitPos, VectorPos leaderPos, double leaderRadius) {
	if (unitPos.getDistance(leaderPos) <= leaderRadius) {
		return true;
	}
	else {
		return false;
	}
}