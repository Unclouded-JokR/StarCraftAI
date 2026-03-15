#include "BOIDS.h"

// Uses BOIDS algorithm to maintain formation while leader is moving
// leaderVec keeps units close to leader
// cohesionVec keeps units close to each other
// separationVec keeps units from crowding each other
// alignmentVec keeps units moving in same direction
void BOIDS::squadFlock(Squad* squad) {
	VectorPos separationVec = VectorPos(0, 0);
	VectorPos cohesionVec = VectorPos(0, 0);
	VectorPos alignmentVec = VectorPos(0, 0);
	VectorPos leaderVec = VectorPos(0, 0);

	for (const auto& unit : squad->units) {
		if (!unit->exists()) {
			continue;
		}

		// Ignore leader or if unit is attacking
		if (unit == squad->leader || unit->isAttackFrame()) {
			continue;
		}

		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), MIN_NEIGHBOUR_DISTANCE, BWAPI::Colors::Grey);
		//BWAPI::Broodwar->drawCircleMap(unit->getPosition(), MIN_ALIGNMENT_DISTANCE, BWAPI::Colors::Yellow);
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);

		// Need current unit's info to use with neighbor vectors
		const double unitSpeed = unit->getType().topSpeed();
		const VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);
		const VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());

		const VectorPos leaderPos = VectorPos(squad->leader->getPosition().x, squad->leader->getPosition().y);
		leaderVec = leaderPos - unitPos;

		// Grab close neigbors for separation vector
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unitPos, MIN_SEPARATION_DISTANCE, BWAPI::Filter::IsAlly);

		for (const auto& neighbor : neighbors) {
			// If neighbor is the current unit or neighbor is not a unit of the squad, dont process
			if (neighbor == unit || std::find(squad->units.begin(), squad->units.end(), neighbor) == squad->units.end()) {
				continue;
			}

			const VectorPos neighborPos = VectorPos(neighbor->getPosition().x, neighbor->getPosition().y);

			// SEPARATION VECTOR
			const double distance = unitPos.getApproxDistance(neighborPos);
			if (distance < MIN_SEPARATION_DISTANCE) {
				separationVec = separationVec + (unitPos - neighborPos);
			}
		}

#ifdef DEBUG_FLOCKING
		cout << "------ UNIT " << unit->getID() << " ------" << endl;
		cout << "Separation Vector: " << separationVec << endl;
		cout << "Cohesion Vector: " << cohesionVec << endl;
		cout << "Alignment Vector: " << alignmentVec << endl;
		cout << "Leader Vector: " << leaderVec << endl;
#endif

		const VectorPos separationDirection = normalize(separationVec) * SEPARATION_STRENGTH;
		const VectorPos leaderDirection = normalize(leaderVec) * LEADER_STRENGTH;

		VectorPos boidsVector = separationDirection + leaderDirection;
		cout << "Boids Vector: " << boidsVector << endl;

#ifdef DEBUG_FLOCKING
		cout << "Separation Vector (normalized) : " << separationDirection << endl;
		cout << "Cohesion Vector (normalized) : " << cohesionDirection << endl;
		cout << "Alignment Vector (normalized) : " << alignmentDirection << endl;
		cout << "Leader Vector (normalized) : " << leaderDirection << endl;
		cout << "Unit Position: " << unitPos << endl;
		cout << "Final Vector (normalized) : " << boidsVector << endl;
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

		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + boidsVector, BWAPI::Colors::Green);
		unit->attack(unitPos + unitVelocity + boidsVector);
	}
}

VectorPos BOIDS::normalize(VectorPos vector) {
	// Avoid division by 0
	const double magnitude = vector.getDistance(BWAPI::Position(0, 0));

	if (magnitude == 0) {
		return VectorPos(0, 0);
	}

	return VectorPos(vector.x / magnitude, vector.y / magnitude);
}