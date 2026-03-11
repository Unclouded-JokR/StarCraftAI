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

		const double unitSpeed = unit->getType().topSpeed();
		const VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);
		const VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());

		const VectorPos leaderPos = VectorPos(squad->leader->getPosition().x, squad->leader->getPosition().y);
		leaderVec = leaderPos - unitPos;

		// Grab all neighbors that are too close
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unitPos, MIN_NEIGHBOUR_DISTANCE);
		BWAPI::Broodwar->drawCircleMap(unitPos, MIN_NEIGHBOUR_DISTANCE, BWAPI::Colors::Grey);
		BWAPI::Broodwar->drawCircleMap(unitPos, MIN_SEPARATION_DISTANCE, BWAPI::Colors::Red);

		// Cohesion Vector
		const VectorPos neighborAvgPos = VectorPos(neighbors.getPosition().x, neighbors.getPosition().y);

		VectorPos averageVelocity = VectorPos(0, 0);
		for (auto& neighbor : neighbors) {
			if (neighbor == unit) {
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

		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + separationVec * 20, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + cohesionVec * 20, BWAPI::Colors::White);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + alignmentVec * 20, BWAPI::Colors::Blue);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + leaderVec * 20, BWAPI::Colors::Purple);

		cohesionVec = neighborAvgPos - unitPos;

		const VectorPos separationDirection = normalize(separationVec) * unitSpeed;
		const VectorPos cohesionDirection = normalize(cohesionVec) * unitSpeed;
		const VectorPos alignmentDirection = normalize(alignmentVec) * unitSpeed;
		const VectorPos leaderDirection = normalize(leaderVec) * unitSpeed;

		const VectorPos finalDirection = (separationDirection * SEPARATION_STRENGTH) +
			(cohesionDirection * COHESION_STRENGTH) +
			(alignmentDirection * ALIGNMENT_STRENGTH) +
			(leaderDirection * LEADER_STRENGTH);

		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + separationVec, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + cohesionVec, BWAPI::Colors::Yellow);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + alignmentVec, BWAPI::Colors::Purple);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + leaderVec, BWAPI::Colors::Blue);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + finalDirection, BWAPI::Colors::Green);

#ifdef DEBUG_FLOCKING
		BWAPI::Broodwar->printf("Separation magnitude: %f", BWAPI::Position(0, 0).getDistance(separationVec));
		BWAPI::Broodwar->printf("Cohesion magnitude: %f", BWAPI::Position(0, 0).getDistance(cohesionVec));
		BWAPI::Broodwar->printf("Alignment magnitude: %f", BWAPI::Position(0, 0).getDistance(alignmentVec));
		BWAPI::Broodwar->printf("Leader magnitude: %f", BWAPI::Position(0, 0).getDistance(leaderDirection));
		BWAPI::Broodwar->printf("FinalDirection magnitude: %f", BWAPI::Position(0, 0).getDistance(finalDirection));
#endif

		//unit->attack(unitPos + finalDirection);
	}
}

double BOIDS::getMagnitude(BWAPI::Position vector) {
	return sqrt(pow(vector.x, 2) + pow(vector.y, 2));
}

VectorPos BOIDS::normalize(VectorPos vector) {
	return VectorPos(vector.x / vector.getApproxDistance(BWAPI::Position(0, 0)),
		vector.y / vector.getApproxDistance(BWAPI::Position(0, 0))
	);
}