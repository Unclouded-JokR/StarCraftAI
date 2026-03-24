#include "BOIDS.h"

// Static variables
unordered_map<BWAPI::Unit, Squad*, unitCMHash> CombatManager::unitSquadMap;
unordered_map<BWAPI::Unit, pair<int, int>, unitHash> BOIDS::leaderRadiusMap;
unordered_map<BWAPI::Unit, unordered_map<BWAPI::Unit, double, unitHash>, unitHash> BOIDS::unitDistanceCache;
unordered_map<BWAPI::Unit, VectorPos, unitHash> BOIDS::previousBOIDSMap;;
unordered_map<BWAPI::Unit, int, unitHash> BOIDS::terrainFrameMap;
unordered_map<BWAPI::Unit, VectorPos, unitHash> BOIDS::terrainDirMap;

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
#ifdef DEBUG_FLOCKING
			BWAPI::Broodwar->drawCircleMap(unit->getPosition(), MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);
#endif
			continue;
		}

		// Position info
		const VectorPos unitPos = VectorPos(unit->getPosition());
		const VectorPos leaderPos = VectorPos(squad->leader->getPosition());
		const double distUnitToLeader = unitPos.getApproxDistance(leaderPos);

		// Don't worry about flocking if unit is too far away from the leader
		if (distUnitToLeader > BOIDS_RANGE || unit->isStuck()) {
			unit->attack(leaderPos);
			continue;
		}

		// Velocity info
		VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());
		VectorPos leaderVelocity = VectorPos(squad->leader->getVelocityX(), squad->leader->getVelocityY());
		const bool isZeroUnitVelocity = unitVelocity.getSqDistance() == 0;

		// For drawing radiuses
#ifdef DEBUG_FLOCKING
		BWAPI::Broodwar->drawCircleMap(unitPos, MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawCircleMap(leaderPos, BOIDS_RANGE, BWAPI::Colors::Grey);
#endif

		// LEADER VECTOR
		VectorPos leaderVec = VectorPos(0, 0);
		const double spacing = 30.0;
		double outer_radius;

		// Update leader radii map if needed
		const int squadSize = CombatManager::unitSquadMap[unit]->units.size();

		// Only calculate outer_radius if squad size has changed since the unit's previous calculation
		// Saves performance since sqrt is expensive
		if (squadSize == leaderRadiusMap[squad->leader].first) {
			outer_radius = leaderRadiusMap[squad->leader].second;
		}
		else {
			outer_radius = max(spacing * 1.5, spacing + sqrt(squad->units.size()) * spacing);
			leaderRadiusMap[unit].first = squadSize;
			leaderRadiusMap[unit].second = outer_radius;
		}

		BWAPI::Broodwar->drawCircleMap(leaderPos, outer_radius, BWAPI::Colors::Yellow);

		leaderVec += leaderVelocity * clamp(distUnitToLeader / outer_radius, 0.0, 1.0);

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
				const BWAPI::Unit neighborLeader = CombatManager::unitSquadMap[neighbor]->leader;
				const VectorPos neighborLeaderPos = VectorPos(neighborLeader->getPosition());
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

				const bool sameSquad = CombatManager::unitSquadMap[unit] == CombatManager::unitSquadMap[neighbor];
				const bool isNeighborLeader = CombatManager::unitSquadMap[neighbor]->leader == neighbor;
				const bool neighborInLeaderRadius = inLeaderRadius(neighborPos, leaderPos, outer_radius);
				const bool unitInLeaderRadius = inLeaderRadius(unitPos, leaderPos, outer_radius);

				if (sameSquad) { // same squad
					if (isNeighborLeader) {
						leaderSeparationVec += (unitPos - neighborPos);
					}
					else if (!unitInLeaderRadius && neighborInLeaderRadius) { // unit not in radius but neighbor is
						separationVec += VectorPos(0, 0);
					}
					else if (isZeroUnitVelocity) {
						separationVec += (unitPos - neighborPos);
						continue;
					}
					else {
						separationVec += getSeparationSteering(unitPos, neighborPos, unitVelocity);
					}
				}
				else if (isNeighborLeader) { // neighbor is a leader of different squad
					leaderSeparationVec += (unitPos - neighborPos);
				}
				else {
					if (isZeroUnitVelocity) {
						separationVec += (unitPos - neighborPos);
						continue;
					}
					separationVec += getSeparationSteering(unitPos, neighborPos, unitVelocity);
				}
			}
		}

		VectorPos finalLeaderVec = leaderVec * LEADER_STRENGTH;
		VectorPos finalSeparationVec = (leaderSeparationVec * LEADER_SEPARATION_STRENGTH) + (separationVec * SEPARATION_STRENGTH);

		// TERRAIN VECTOR
		VectorPos terrainVec;
		double terrainSqDistance;
		// Look to see if unit is already avoiding terrain
		if (terrainFrameMap[unit] > 0) {
			terrainVec = terrainDirMap[unit];
			terrainSqDistance = terrainVec.getSqDistance();
			terrainFrameMap[unit]--;
		}
		else {
			terrainVec = getTerrainSteering(unit, unitPos, leaderPos, unitVelocity);
			terrainSqDistance = terrainVec.getSqDistance();
			if (terrainSqDistance) {
				terrainDirMap[unit] = terrainVec;
				terrainFrameMap[unit] = TERRAIN_AVOIDANCE_FRAMES;
			}
		}

		// BOIDS VECTOR
		// Before applying leader vector, check if leaderVector is pulling us into the wall
		// If so, lower strength of finalLeaderVec
		if (terrainSqDistance > 0) {
			VectorPos terrainNorm = terrainVec.normalized();

			if (finalLeaderVec.dot(terrainNorm) < 0) {
				finalLeaderVec *= 0.4;
			}
		}

		VectorPos boidsVector = finalLeaderVec + finalSeparationVec;

		// Applying terrain avoidance after leader + separation to remove terrain component from final boidsVector
		if (terrainSqDistance > 0) {
			const VectorPos terrainNorm = terrainVec.normalized();
			const double dotProduct = boidsVector.dot(terrainNorm);

			if (dotProduct < 0) {
				boidsVector -= terrainNorm * dotProduct * TERRAIN_STRENGTH; // remove component of vector going into the wall
			}
		}

		// At a minimum, scale BOIDS by 75%. Too short was making units too slow.
		const double distScale = max(sqrt(distUnitToLeader / outer_radius), 0.75);
		boidsVector = boidsVector * distScale;

		// If the adjustment is not significant enough, don't move the unit
		const double boidsLength = boidsVector.getLength();
		if (boidsLength < 0.2) {
			previousBOIDSMap[unit] = boidsVector;
			continue;
		}

		// Clamping
		if (boidsLength < MIN_FORCE) {
			boidsVector = boidsVector / boidsLength * MIN_FORCE;
		}
		else if (boidsLength > MAX_FORCE) {
			boidsVector = boidsVector / boidsLength * MAX_FORCE;
		}

		// Cache boidsVector to smoothly apply changes instead of drastic ones (was causing too much jittering)
		if (previousBOIDSMap.find(unit) != previousBOIDSMap.end()) {
			boidsVector = previousBOIDSMap[unit] * 0.7 + boidsVector * 0.3;
			previousBOIDSMap[unit] = boidsVector;
		}
		else {
			previousBOIDSMap[unit] = boidsVector;
		}

		const VectorPos finalTarget = unitPos + unitVelocity + boidsVector;

		unit->attack(finalTarget);

#ifdef DEBUG_FLOCKING
		cout << "Separation Vector : " << finalSeparationVec.getLength() << endl;
		cout << "Leader Vector : " << finalLeaderVec.getLength() << endl;
		cout << "Boids Vector : " << boidsVector.getLength() << endl;
		cout << "Final Vector : " << finalTarget << endl;
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
	VectorPos left = VectorPos(-velocityDir.y, velocityDir.x);
	VectorPos right = VectorPos(velocityDir.y, -velocityDir.x);
	VectorPos dir = crossProduct > 0 ? right : left;

	return dir;
}

VectorPos BOIDS::getTerrainSteering(BWAPI::Unit unit, VectorPos unitPos, VectorPos leaderPos, VectorPos unitVelocity) {
	VectorPos forward;

	if (unitVelocity.getSqDistance() > 0.5) {
		forward = unitVelocity.normalized();
	}
	else {
		forward = (leaderPos - unitPos).normalized();
	}

	const int width = unit->getType().width();
	const int height = unit->getType().height();

	// Unit corners
	const VectorPos topLeft(unitPos.x - width / 2, unitPos.y - height / 2);
	const VectorPos topRight(unitPos.x + width / 2, unitPos.y - height / 2);
	const VectorPos bottomLeft(unitPos.x - width / 2, unitPos.y + height / 2);
	const VectorPos bottomRight(unitPos.x + width / 2, unitPos.y + height / 2);

	const VectorPos topLeftCheck = topLeft + forward * (MAX_FORCE + 10);
	const VectorPos topRightCheck = topRight + forward * (MAX_FORCE + 10);
	const VectorPos bottomLeftCheck = bottomLeft + forward * (MAX_FORCE + 10);
	const VectorPos bottomRightCheck = bottomRight + forward * (MAX_FORCE + 10);

#ifdef DEBUG_FLOCKING
	BWAPI::Broodwar->drawLineMap(topLeft, topLeftCheck, BWAPI::Colors::Green);
	BWAPI::Broodwar->drawLineMap(topRight, topRightCheck, BWAPI::Colors::Green);
	BWAPI::Broodwar->drawLineMap(bottomLeft, bottomLeftCheck, BWAPI::Colors::Green);
	BWAPI::Broodwar->drawLineMap(bottomRight, bottomRightCheck, BWAPI::Colors::Green);
#endif

	VectorPos normal(0, 0);

	// Accumulate normals from blocked corners
	if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(topLeftCheck))
		|| !BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition(topLeftCheck), BWAPI::Filter::IsBuilding).empty()) {
		normal += (unitPos - topLeft).normalized();
	}
	if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(topRightCheck))
		|| !BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition(topRightCheck), BWAPI::Filter::IsBuilding).empty()) {
		normal += (unitPos - topRight).normalized();
	}
	if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(bottomLeftCheck))
		|| !BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition(bottomLeftCheck), BWAPI::Filter::IsBuilding).empty()) {
		normal += (unitPos - bottomLeft).normalized();
	}
	if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(bottomRightCheck))
		|| !BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition(bottomRightCheck), BWAPI::Filter::IsBuilding).empty()) {
		normal += (unitPos - bottomRight).normalized();
	}

	// No terrain detected
	if (normal.getSqDistance() == 0) {
		return VectorPos(0, 0);
	}

	normal = normal.normalized();

#ifdef DEBUG_FLOCKING
	BWAPI::Broodwar->drawLineMap(unitPos, unitPos + normal * 40, BWAPI::Colors::Yellow);
#endif

	return normal;
}

bool BOIDS::inLeaderRadius(VectorPos unitPos, VectorPos leaderPos, double leaderRadius) {
	if (unitPos.getDistance(leaderPos) <= leaderRadius) {
		return true;
	}
	else {
		return false;
	}
}