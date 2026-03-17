#include "BOIDS.h"

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
			continue;
		}

		if (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame() < FRAMES_BETWEEN_BOIDS) {
			continue;
		}

		// Position info
		const VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);
		const VectorPos leaderPos = VectorPos(squad->leader->getPosition().x, squad->leader->getPosition().y);

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
		const double dist = unit->getDistance(leaderPos);
		const double outer_radius = max(INNER_LEADER_RADIUS * 1.5, MIN_SEPARATION_DISTANCE + sqrt(squad->units.size()) * MIN_SEPARATION_DISTANCE);

		// Update leader radii map if needed
		if (leaderRadiusMap.find(squad->leader) == leaderRadiusMap.end() || leaderRadiusMap[squad->leader] != outer_radius) {
			leaderRadiusMap[squad->leader] = outer_radius;
		}

		// if unit too far, pull in
		// if unit too close, push away
		if (dist > outer_radius) {
			leaderVec = leaderPos - unitPos;
		}
		else if (dist < INNER_LEADER_RADIUS){
			leaderVec = unitPos - leaderPos;
		}

		leaderVec = normalize(leaderVelocity) + normalize(leaderVec) * LEADER_STRENGTH;

//#ifdef DEBUG_FLOCKING
		BWAPI::Broodwar->drawCircleMap(leaderPos, BOIDS_RANGE, BWAPI::Colors::Grey);
		BWAPI::Broodwar->drawCircleMap(leaderPos, INNER_LEADER_RADIUS, BWAPI::Colors::Green);
		BWAPI::Broodwar->drawCircleMap(leaderPos, outer_radius, BWAPI::Colors::Yellow);
//#endif

		// SEPARATION VECTOR
		// Grab close neigbors for separation vector
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unitPos, MIN_SEPARATION_DISTANCE, BWAPI::Filter::IsAlly);
		VectorPos separationVec = VectorPos(0, 0);

		for (const auto& neighbor : neighbors) {
			// If neighbor is the current unit, dont process
			if (neighbor == unit) {
				continue;
			}

			// Don't apply separation to self if neighbor is a unit settling into their leader's radius
			BWAPI::Unit neighborLeader = CombatManager::unitSquadMap[neighbor]->leader;
			int dist = neighbor->getPosition().getApproxDistance(neighborLeader->getPosition());
			if (dist > INNER_LEADER_RADIUS && dist < leaderRadiusMap[neighborLeader]) {
				separationVec = VectorPos(0, 0);
				continue;
			}

			const VectorPos neighborPos = VectorPos(neighbor->getPosition().x, neighbor->getPosition().y);

			// Need to scale separation strength by how close/far the neighbor is
			const double distance = unitPos.getDistance(neighborPos);
			const double scale = 1 / distance;
			separationVec += (unitPos - neighborPos) * scale;
		}

		separationVec = normalize(separationVec);

		// TERRAIN VECTOR
		VectorPos terrainVec = getTerrainVector(unit, squad->leader) * TERRAIN_STRENGTH;

		// FINAL BOIDS VECTOR
		VectorPos boidsVector = leaderVec + separationVec + terrainVec;

		// normalize final boids vector if not VectorPos(0,0)
		boidsVector = normalize(boidsVector);

		// If the adjustment is not significant enough, don't move the unit
		if (boidsVector.getDistance(VectorPos(0, 0)) < 0.2) {
			continue;
		}

		VectorPos finalTarget = unitPos + (unitVelocity * VELOCITY_DAMPENING) + (boidsVector * unit->getType().topSpeed() * FRAMES_BETWEEN_BOIDS);
		unit->attack(finalTarget);

#ifdef DEBUG_FLOCKING
		cout << "Separation Vector : " << separationVec << endl;
		cout << "Terrain Vector : " << terrainVec << endl;
		cout << "Leader Vector : " << leaderVec << endl;
		cout << "Boids Vector : " << boidsVector << endl;
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

VectorPos BOIDS::getTerrainVector(BWAPI::Unit unit, BWAPI::Unit leader) {
	VectorPos terrainVec = VectorPos(0, 0);

	const VectorPos forward = normalize(VectorPos(unit->getVelocityX(), unit->getVelocityY()));
	const VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);
	VectorPos forwardCheck = unitPos + forward * TERRAIN_LOOKAHEAD_LENGTH; // where unit will be

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

VectorPos BOIDS::normalize(VectorPos vector) {
	// Avoid division by 0
	const double magnitude = vector.getDistance(BWAPI::Position(0, 0));

	if (magnitude < 1 && magnitude > 0) {
		return vector;
	}

	if (magnitude == 0) {
		return VectorPos(0, 0);
	}

	return vector / magnitude;
}