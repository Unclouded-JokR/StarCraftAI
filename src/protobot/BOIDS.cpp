#include "BOIDS.h"

// Static variables
map<BWAPI::Unit, Squad*> CombatManager::unitSquadMap;
unordered_map<BWAPI::Unit, double, unitHash> BOIDS::leaderRadiusMap;

// Uses BOIDS algorithm to maintain formation while leader is moving
// leaderVec keeps units surround the leader
// separationVec keeps units from crowding too tightly
// terrainVec keeps units away from terrain
void BOIDS::squadFlock(Squad* squad) {
	for (const auto& unit : squad->units) {
		if (!unit->exists()) {
			continue;
		}

		// Ignore leader or if unit is attacking
		if (unit == squad->leader || unit->isAttackFrame()) {
			BWAPI::Broodwar->drawCircleMap(unit->getPosition(), MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);
			continue;
		}

		if (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < FRAMES_BETWEEN_BOIDS) {
			continue;
		}

		// Position info
		const VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);
		const VectorPos leaderPos = VectorPos(squad->leader->getPosition().x, squad->leader->getPosition().y);
		const double distUnitToLeader = unitPos.getDistance(leaderPos);

		// Don't worry about flocking if unit is too far away from the leader
		if (unitPos.getApproxDistance(leaderPos) > BOIDS_RANGE) {
			unit->attack(leaderPos);
			continue;
		}

		// Velocity info
		const VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());
		const VectorPos leaderVelocity = VectorPos(squad->leader->getVelocityX(), squad->leader->getVelocityY());

		// For drawing radiuses
		BWAPI::Broodwar->drawCircleMap(unitPos, MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);

		// LEADER VECTOR
		VectorPos leaderVec = VectorPos(0, 0);
		const double spacing = 25.0;
		const double outer_radius = max(spacing * 1.5, spacing + sqrt(squad->units.size()) * spacing);

		// Update leader radii map if needed
		if (leaderRadiusMap.find(squad->leader) == leaderRadiusMap.end() || leaderRadiusMap[squad->leader] != outer_radius) {
			leaderRadiusMap[squad->leader] = outer_radius;
		}

		// if unit too far, pull in
		if (distUnitToLeader > outer_radius) {
			leaderVec = leaderVelocity + (leaderPos - unitPos);
		}

		leaderVec = leaderVec.normalize() * LEADER_STRENGTH;

		//#ifdef DEBUG_FLOCKING
		BWAPI::Broodwar->drawCircleMap(leaderPos, BOIDS_RANGE, BWAPI::Colors::Grey);
		BWAPI::Broodwar->drawCircleMap(leaderPos, outer_radius, BWAPI::Colors::Yellow);
		//#endif

				// SEPARATION VECTOR
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unitPos, MIN_SEPARATION_DISTANCE, BWAPI::Filter::IsAlly);
		VectorPos separationVec = VectorPos(0, 0);

		for (const auto& neighbor : neighbors) {
			// If neighbor is the current unit, dont process
			if (neighbor == unit) {
				continue;
			}

			const VectorPos neighborPos = VectorPos(neighbor->getPosition().x, neighbor->getPosition().y);

			// Don't apply separation to self if neighbor is a unit settling into their leader's radius
			// If neighbor is a leader, apply separation to self 
			if (CombatManager::unitSquadMap.find(neighbor) != CombatManager::unitSquadMap.end()) { // neighbor has a squad
				if (neighbor != CombatManager::unitSquadMap[neighbor]->leader) { // neighbor is not a leader
					BWAPI::Unit neighborLeader = CombatManager::unitSquadMap[neighbor]->leader;
					const double distToNeighbor = unitPos.getDistance(neighborPos);

					// if both units have the same leader and are both in the radius, calculate separation normally
					if (inLeaderRadius(neighbor, neighborLeader) && !inLeaderRadius(unit, squad->leader)) {
						separationVec += VectorPos(0, 0);
					}
					else {
						separationVec += (unitPos - neighborPos) * 1 / distToNeighbor;
					}
				}
			}
		}

		separationVec = separationVec.normalize() * SEPARATION_STRENGTH;

		// TERRAIN VECTOR
		VectorPos terrainVec = getTerrainVector(unit, squad->leader) * TERRAIN_STRENGTH;

		// FINAL BOIDS VECTOR
		VectorPos boidsVector = leaderVec + separationVec + terrainVec;

		// normalize final boids vector if not VectorPos(0,0)
		boidsVector = boidsVector.normalize();

		// If the adjustment is not significant enough, don't move the unit
		if (boidsVector.getDistance(VectorPos(0, 0)) < 0.2) {
			continue;
		}

		// unit speed is pixels per frame so its scaled by frames skipped between boids calculations
		const int minDistance = 10;
		cout << "Unit speed: " << unit->getType().topSpeed() << endl;
		const VectorPos finalTarget = unitPos + (boidsVector * unit->getType().topSpeed() * (FRAMES_BETWEEN_BOIDS + 1) * minDistance);

		unit->attack(finalTarget);

#ifdef DEBUG_FLOCKING
		//cout << "Separation Vector : " << separationVec << endl;
		//cout << "Terrain Vector : " << terrainVec << endl;
		//cout << "Leader Vector : " << leaderVec << endl;
		//cout << "Boids Vector : " << boidsVector << endl;
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

VectorPos BOIDS::getTerrainVector(BWAPI::Unit unit, BWAPI::Unit leader) {
	VectorPos terrainVec = VectorPos(0, 0);
	VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);

	const VectorPos forward = unitPos.normalize();
	const VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());
	const VectorPos forwardCheck = unitPos + unitVelocity + forward * TERRAIN_LOOKAHEAD_LENGTH; // lookahead check for terrain
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

bool BOIDS::inLeaderRadius(BWAPI::Unit unit, BWAPI::Unit leader) {
	if (unit == nullptr) {
		return false;
	}

	if (unit->getPosition().getApproxDistance(leader->getPosition()) <= leaderRadiusMap[leader]) {
		return true;
	}
	else {
		return false;
	}
}