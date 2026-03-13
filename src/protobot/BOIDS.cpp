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
		if (!unit->exists() || unit == squad->leader) {
			continue;
		}

		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), MIN_NEIGHBOUR_DISTANCE, BWAPI::Colors::Grey);
		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);

		const double unitSpeed = unit->getType().topSpeed();
		const VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);
		const VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());

		const VectorPos leaderPos = VectorPos(squad->leader->getPosition().x, squad->leader->getPosition().y);
		leaderVec = leaderPos - unitPos;

		// Grab all neighbors that are too close
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unitPos, MIN_NEIGHBOUR_DISTANCE, BWAPI::Filter::IsAlly);

		// Cohesion Vector
		const VectorPos neighborAvgPos = VectorPos(neighbors.getPosition().x, neighbors.getPosition().y);

		VectorPos averageVelocity = VectorPos(0, 0);
		for (const auto& neighbor : neighbors) {
			// If neighbor is the current unit or neighbor is not a unit of the squad, dont process
			if (neighbor == unit || std::find(squad->units.begin(), squad->units.end(), neighbor) != squad->units.end()) {
				continue;
			}

			const VectorPos neighborPos = VectorPos(neighbor->getPosition().x, neighbor->getPosition().y);

			// Separation Vector
			const double distance = unitPos.getApproxDistance(neighborPos);
			if (distance < MIN_SEPARATION_DISTANCE) {
				separationVec += unitPos - neighborPos;
			}

			// Alignment Vector
			double neighborvelocity_x = neighbor->getVelocityX();
			double neighborvelocity_y = neighbor->getVelocityY();
#ifdef DEBUG_FLOCKING
			BWAPI::Broodwar->printf("Neighbor Velocity: X: %f, Y: %f", neighborvelocity_x, neighborvelocity_y);
#endif
			averageVelocity += VectorPos(neighborvelocity_x, neighborvelocity_y);
		}

		if (neighbors.size() > 0) {
			// Average out the velocity
			// neighbors.getPosition() is already averaged
			// separationVec is summed
			averageVelocity /= neighbors.size();
		}

		// If no average velocity, set alignment to zero vector
		// Avoids pointing unit towards (0,0)
		/*if (averageVelocity == VectorPos(0, 0)) {
			alignmentVec = VectorPos(0, 0);
		}
		else {*/

		alignmentVec = averageVelocity - unitVelocity;
		if (averageVelocity == VectorPos(0, 0)) {
			alignmentVec = VectorPos(0, 0);
		}

		cohesionVec = neighborAvgPos - unitPos;

		cout << "Separation Vector: " << separationVec << endl;
		cout << "Cohesion Vector: " << cohesionVec << endl;
		cout << "Alignment Vector: " << alignmentVec << endl;
		cout << "Leader Vector: " << leaderVec << endl;

		const VectorPos separationDirection = normalize(separationVec) * SEPARATION_STRENGTH;
		const VectorPos cohesionDirection = normalize(cohesionVec) * COHESION_STRENGTH;
		const VectorPos alignmentDirection = normalize(alignmentVec) * ALIGNMENT_STRENGTH;
		const VectorPos leaderDirection = normalize(leaderVec) * LEADER_STRENGTH;

		const VectorPos boidsVector = separationDirection + cohesionDirection + alignmentDirection + leaderDirection;

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

		unit->attack(unitPos + boidsVector);
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