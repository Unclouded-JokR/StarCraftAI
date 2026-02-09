#include "Squad.h"

Squad::Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor, int unitSize)
{
	this->leader = leader;
	this->squadId = squadId;
	this->squadColor = squadColor;
	this->unitSize = unitSize;
	this->state = IDLE;
}

void Squad::onFrame() {
	if (state == POSITIONING) {
		flockingHandler();
	}
	
	pathHandler();*/

	for (BWAPI::Unit& squadMate : units)
	{
		if (squadMate == leader) continue;

		if (squadMate->isIdle() && !(squadMate->getDistance(leader) < 500))
		{
			squadMate->attack(leader->getPosition());
		}

	}

	BWAPI::Broodwar->drawTextMap(BWAPI::Position(leader->getPosition().x - 25, leader->getPosition().y - 25), "Leader");
}

void Squad::simpleFlock() {
	BWAPI::Position centerPos = BWAPI::Position(0, 0);
	// Calculate center position of group for cohesion vector
	for (auto& unit : units) {
		centerPos += unit->getPosition();
	}
	centerPos /= units.size();

	BWAPI::Position generalVector = leader->getTargetPosition() - centerPos;
	for (auto& unit : units) {
		if (!unit->exists() || unit == leader) {
			continue;
		}
		unit->attack(generalVector + unit->getPosition());
	}

}

// Uses BOIDS algorithm to maintain formation while leader is moving
// leaderVec keeps units close to leader
// cohesionVec keeps units close to each other
// separationVec keeps units from crowding each other
// alignmentVec keeps units moving in same direction
void Squad::flockingHandler() {
	VectorPos separationVec = VectorPos(0, 0);
	VectorPos cohesionVec = VectorPos(0, 0);
	VectorPos alignmentVec = VectorPos(0, 0);
	VectorPos leaderVec = VectorPos(0, 0);

	for (auto& unit : units) {
		if (!unit->exists() || unit == leader) {
			continue;
		}

		const double unitSpeed = unit->getType().topSpeed();
		const VectorPos unitPos = VectorPos(unit->getPosition().x, unit->getPosition().y);
		const VectorPos unitVelocity = VectorPos(unit->getVelocityX(), unit->getVelocityY());

		const VectorPos leaderPos = VectorPos(leader->getPosition().x, leader->getPosition().y);
		leaderVec = leaderPos - unitPos;

		// Grab all neighbors that are too close
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unitPos, minNDistance);
		BWAPI::Broodwar->drawCircleMap(unitPos, minNDistance, BWAPI::Colors::Grey);
		BWAPI::Broodwar->drawCircleMap(unitPos, minSepDistance, BWAPI::Colors::Red);

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
			if (distance < minSepDistance) {
				separationVec += unitPos - neighborPos;
			}

			// Alignment Vector
			double neighborvelocity_x = neighbor->getVelocityX();
			double neighborvelocity_y = neighbor->getVelocityY();
			BWAPI::Broodwar->printf("Neighbor Velocity: X: %f, Y: %f", neighborvelocity_x, neighborvelocity_y);
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

		// Flocking strength variables
		const double separationStrength = 1;
		const double cohesionStrength = 1;
		const double alignmentStrength = 1;
		const double leaderStrength = 1;

		const VectorPos finalDirection = (separationDirection * separationStrength) +
			(cohesionDirection * cohesionStrength) +
			(alignmentDirection * alignmentStrength) +
			(leaderDirection * leaderStrength);

		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + separationVec, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + cohesionVec, BWAPI::Colors::Yellow);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + alignmentVec, BWAPI::Colors::Purple);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + leaderVec, BWAPI::Colors::Blue);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + finalDirection, BWAPI::Colors::Green);
		
		BWAPI::Broodwar->printf("Separation magnitude: %f", BWAPI::Position(0,0).getDistance(separationVec));
		BWAPI::Broodwar->printf("Cohesion magnitude: %f", BWAPI::Position(0,0).getDistance(cohesionVec));
		BWAPI::Broodwar->printf("Alignment magnitude: %f", BWAPI::Position(0,0).getDistance(alignmentVec));
		BWAPI::Broodwar->printf("Leader magnitude: %f", BWAPI::Position(0,0).getDistance(leaderDirection));
		BWAPI::Broodwar->printf("FinalDirection magnitude: %f", BWAPI::Position(0,0).getDistance(finalDirection));

		//unit->attack(unitPos + finalDirection);
	}
}

void Squad::pathHandler() {
	const int distThreshold = 1;
	if (currentPath.positions.empty() == false && currentPathIdx < currentPath.positions.size()) {
		const BWAPI::Position target = BWAPI::Position(currentPath.positions.at(currentPathIdx));
		if (leader->getDistance(target) <= distThreshold){
			currentPathIdx += 1;
		}
		else if (leader->getTargetPosition() != target) {
			leader->attack(target);
		}
	}

	for (pair<BWAPI::Position, BWAPI::Position> rect : rectCoordinates) {
		BWAPI::Broodwar->drawBoxMap(rect.first, rect.second, BWAPI::Colors::Yellow);
	}
	for (pair<BWAPI::TilePosition, double> pair : closedTiles) {
		// Drawing box around tile
		BWAPI::Position pos = BWAPI::Position(pair.first);
		BWAPI::Broodwar->drawBoxMap(BWAPI::Position(pos.x, pos.y), BWAPI::Position(pos.x + 32, pos.y + 32), BWAPI::Colors::Red);
		BWAPI::Broodwar->drawTextMap(BWAPI::Position(pos.x + 8, pos.y + 8), "%.2f", pair.second);
	}

	AStar::drawPath(currentPath);
}

void Squad::removeUnit(BWAPI::Unit unit){
	if (unit == nullptr) {
		return;
	}

	if (unit == leader) {
		const BWAPI::Position leaderPos = unit->getPosition();
		std::erase_if(units, [unit](const BWAPI::Unit& _unit) {
			return unit->getID() == _unit->getID();
			});

		if (units.empty()){
			return;
		}

		// Closest unit to the leader is assigned as the new leader
		double closest = std::numeric_limits<double>::infinity();
		BWAPI::Unit closestUnit;
		for (BWAPI::Unit _unit : units) {
			BWAPI::Position unitPos = unit->getPosition();
			const double dist = unitPos.getDistance(leaderPos);
			if (dist < closest && _unit->exists()) {
				closest = dist;
				closestUnit = _unit;
			}
		}

		// Assign closest unit to leader as the new leader
		if (closestUnit != nullptr) {
			leader = closestUnit;
		}
	}
	else {
		units.erase(std::remove(units.begin(), units.end(), unit), units.end());
	}
}

void Squad::move(BWAPI::Position position) {
	state = POSITIONING;

	if (leader->isIdle() || !(leader->getDistance(position) < 200)) leader->attack(position);
	//leader->attack(position);
}

void Squad::addUnit(BWAPI::Unit unit) {
	if (unit == nullptr) {
		return;
	}

	if (std::find(units.begin(), units.end(), unit) != units.end()) {
		// Avoids adding duplicate units
		return;
	}

	units.push_back(unit);
	BWAPI::Broodwar->printf("Unit %d added to Squad %d", unit->getID(), squadId);
}

// Despite the name, this method is not about attack-moving
// Acts as the move command when attacking/kiting
void Squad::kitingMove(BWAPI::Unit unit, BWAPI::Position position) {

	// If unit or position is invalid, return
	if (!unit) {
		return;
	}

	// If position invalid, check for valid position around it
	if (!position.isValid()) {
		BWAPI::Position newPos;
		int searches = 0;
		const int min = -50;
		const int max = 50;

		// Limit searches to 25 attempts
		while (searches < 25) {
			newPos = position + BWAPI::Position(min + rand() % (max - min + 1), min + rand() % (max - min + 1));
			if (newPos.isValid()) {
				position = newPos;
				break;
			}
			searches += 1;
		}

		// If no valid position found, return (do nothing)
		return;
	}

	// If unit already had a command assigned to it this frame, ignore
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame()) {
		return;
	}

	// Otherwise, move to the position
	unit->move(position);
}

void Squad::attackUnit(BWAPI::Unit unit, BWAPI::Unit target) {

	// If unit or position is invalid, return
	if (!unit || !target) {
		return;
	}

	// If unit already had a command assigned to it this frame, ignore
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame()) {
		return;
	}

	// If unit is already attacking this target, ignore
	if (unit->getOrder() == BWAPI::Orders::AttackUnit && unit->getOrderTarget() == target) {
		return;
	}

	// If the target is cloaked, run back towards base
	if (target->isCloaked()) {
		if (!unit->canAttack(target)) {
			unit->move(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));
		}
		return;
	}

	// If unit is already attacking, ignore
	if (unit->isStartingAttack() || unit->isAttackFrame()) {
		return;
	}

	// Otherwise, attack the target
	unit->attack(target);
}
void Squad::kitingAttack(BWAPI::Unit unit, BWAPI::Unit target) {

	// If unit or target is invalid, return
	if (!unit || !target) {
		return;
	}

	// If unit already had a command assigned to it this frame, ignore
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame()) {
		return;
	}

	// If the target is cloaked, RUN TOWARDS BASE!!!
	if (target->isCloaked()) {
		BWAPI::Unitset selfUnits = BWAPI::Broodwar->self()->getUnits();

		unit->move((BWAPI::Position) BWAPI::Broodwar->self()->getStartLocation());
		return;
	}
	
	// If unit is already attacking, ignore
	if (unit->isStartingAttack() || unit->isAttackFrame()) {
		return;
	}

	const BWAPI::Position kiteVector = unit->getPosition() - target->getPosition();
	const BWAPI::Position kitePosition = unit->getPosition() + kiteVector;

	const double distance = unit->getDistance(target);
	const double speed = unit->getType().topSpeed();
	const double range = unit->getType().groundWeapon().maxRange();

	const double timeToEnterRange = (distance - range) / speed;
	// If weapon cooldown will be ready by the time we enter range, go and attack the target
	// Otherwise, kite
	if (unit->getGroundWeaponCooldown() <= timeToEnterRange || target->getType().isBuilding()) {
		kitingAttack(unit, target);
	}
	else if (unit->getGroundWeaponCooldown() == 0 && distance <= range) {
		kitingAttack(unit, target);
	}
	else {
		kitingMove(unit, kitePosition);
	}
}

double Squad::getMagnitude(BWAPI::Position vector) {
	return sqrt(pow(vector.x, 2) + pow(vector.y, 2));
}

VectorPos Squad::normalize(VectorPos vector) {
	return VectorPos(vector.x / vector.getApproxDistance(BWAPI::Position(0, 0)), 
							vector.y / vector.getApproxDistance(BWAPI::Position(0, 0))
							);
}

void Squad::drawDebugInfo() {
	for (auto& unit : units) {
		if (!unit || !unit->exists()) {
			continue;
		}

		// Draw lines and shapes to debug commands
		const BWAPI::UnitCommand command = unit->getLastCommand();

		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 5, squadColor, true);
		if (unit == leader) {
			BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 2, BWAPI::Colors::White, true);
		}

		if (command.getTargetPosition() != BWAPI::Positions::None) {
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), command.getTargetPosition(), squadColor);
		}
		if (command.getTargetTilePosition() != BWAPI::TilePositions::None) {
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), BWAPI::Position(command.getTargetTilePosition()), squadColor);
		}
		if (command.getTarget() != nullptr) {
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), command.getTarget()->getPosition(), squadColor);
		}

		// Draws squad ID below unit
		const BWAPI::Position textPos(unit->getPosition().x - 20, unit->getPosition().y + 20);
		BWAPI::Broodwar->drawTextMap(textPos, "Squad %d", squadId);
	}
}