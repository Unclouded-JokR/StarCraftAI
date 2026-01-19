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
	BWAPI::Position separationVec = BWAPI::Position(0, 0);
	BWAPI::Position cohesionVec = BWAPI::Position(0, 0);
	BWAPI::Position alignmentVec = BWAPI::Position(0, 0);
	BWAPI::Position leaderVec = BWAPI::Position(0, 0);

	BWAPI::Position centerPos = leader->getPosition();

	for(auto& unit : units){
		if (!unit->exists() || unit == leader) {
			continue;
		}

		leaderVec = leader->getPosition() - unit->getPosition();
		// Farther from leader = more strength;
		leaderVec = normalize(leaderVec);

		BWAPI::Position unitPos = unit->getPosition();

		// Grab all neighbors that are too close
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unitPos, minNDistance);
		BWAPI::Broodwar->drawCircleMap(unitPos, minNDistance, BWAPI::Colors::Yellow);
		for (auto& neighbor : neighbors) {
			if (neighbor == unit){
				continue;
			}

			const BWAPI::Position neighborPos = neighbor->getPosition();

			// Separation vector points away from neighbor
			// Farther away neighbors contribute less to separation
			separationVec += unitPos - neighborPos;

			// Cohesion vector points towards center of neighbors
			cohesionVec += neighborPos - unitPos;

			// Get alignment vector by using neighbor's velocity
			int neighborvelocity_x = (int) neighbor->getVelocityX();
			int neighborvelocity_y = (int) neighbor->getVelocityY();
			alignmentVec += BWAPI::Position(neighborvelocity_x, neighborvelocity_y);
		}

		if (neighbors.size() > 0) {
			// Average out the cohesion, separation, and alignment vectors
			separationVec = separationVec / neighbors.size();
			cohesionVec = cohesionVec / neighbors.size();
			alignmentVec = alignmentVec / neighbors.size();
		}

		separationVec = normalize(separationVec);
		cohesionVec = normalize(cohesionVec);
		alignmentVec = normalize(alignmentVec);

		// Flocking strength variables
		double separationStrength = 1.5;
		double cohesionStrength = 1;
		double alignmentStrength = 1;
		double leaderStrength = 2;

		BWAPI::Position flockDirection = (alignmentVec * alignmentStrength
										+ cohesionVec * cohesionStrength
										+ separationVec * separationStrength 
										+ leaderVec * leaderStrength);

		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + separationVec * 20, BWAPI::Colors::Red);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + cohesionVec * 20, BWAPI::Colors::White);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + alignmentVec * 20, BWAPI::Colors::Blue);
		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + leaderVec * 20, BWAPI::Colors::Purple);

		BWAPI::Broodwar->drawLineMap(unitPos, unitPos + flockDirection * 20, BWAPI::Colors::Green);

		unit->attack((flockDirection + unitPos));
		
		BWAPI::Broodwar->printf("Separation strength: %f", getMagnitude(separationVec));
		BWAPI::Broodwar->printf("Cohesion strength: %f", getMagnitude(cohesionVec));
		BWAPI::Broodwar->printf("Alignment strength: %f", getMagnitude(alignmentVec));
		BWAPI::Broodwar->printf("FinalDirection strength: %f", getMagnitude(flockDirection));
		// Now that we have the direction, we'll walk through the path to check for collisions
		int x = 0;
		int y = 0;

		bool negX = flockDirection.x < 0;
		bool negY = flockDirection.y < 0;

		//while (x != flockDirection.x && y != flockDirection.y) {
		//	x = negX ? x - 1 : x + 1;
		//	y = negY ? y - 1 : y + 1;

		//	BWAPI::Position pos = BWAPI::Position(unitPos + x, unitPos + y);
		//	BWAPI::Position rightVector = BWAPI::Position()

		//	// Four cases to check for steering vector
		//	// 1. negX negY
		//	// 2. negX posY
		//	// 3. posX negY
		//	// 4. posX posY
		//	if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))) {
		//		
		//	}
		//}
		
	}
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
			const double dist = getMagnitude(unit->getPosition() - leaderPos);
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
	leader->attack(position);
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

BWAPI::Position Squad::normalize(BWAPI::Position vector) {
	return BWAPI::Position((int) round(vector.x / getMagnitude(vector)), (int) round(vector.y / getMagnitude(vector)));
}

// Returns magnitude of vector using sqrt(x^2 + y^2)
double Squad::getMagnitude(BWAPI::Position vector) {
	return sqrt(pow(vector.x, 2) + pow(vector.y, 2));
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