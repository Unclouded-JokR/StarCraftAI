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
void Squad::flockingHandler() {
	BWAPI::Broodwar->printf("In flock function");

	BWAPI::Position separationVec = BWAPI::Position(0, 0);
	BWAPI::Position cohesionVec = BWAPI::Position(0, 0);
	BWAPI::Position alignmentVec = BWAPI::Position(0, 0);

	BWAPI::Position centerPos = BWAPI::Position(0, 0);

	for(auto& unit : units){
		if (!unit->exists() || unit == leader) {
			continue;
		}

		// Grab all neighbors that are too close
		const BWAPI::Unitset neighbors = BWAPI::Broodwar->getUnitsInRadius(unit->getPosition(), minNDistance);
		for (auto& neighbor : neighbors) {
			if (neighbor == unit){
				continue;
			}

			// Separation vector points away from neighbor
			separationVec += unit->getPosition() - neighbor->getPosition();

			// Get alignment vector by using neighbor's velocity
			double neighborvelocity_x = neighbor->getVelocityX();
			double neighborvelocity_y = neighbor->getVelocityY();
			alignmentVec += normalize(BWAPI::Position(neighborvelocity_x, neighborvelocity_y));

			// Cohesion vector points towards center of mass of neighbors
			cohesionVec += neighbor->getPosition();
		}

		if (neighbors.size() > 0) {
			separationVec /= neighbors.size();
			alignmentVec /= neighbors.size();
			cohesionVec /= neighbors.size();
			cohesionVec = cohesionVec - unit->getPosition();
		}

		// Flocking strength variables
		double separationStrength = 0.5;
		double cohesionStrength = 0.1;
		double alignmentStrength = 0.5;

		// Add to unit position since bwapi only uses positive coordinates
		BWAPI::Position attackPos = (alignmentVec * alignmentStrength
									+ cohesionVec * cohesionStrength
									+ separationVec * separationStrength) 
									+ unit->getPosition();
		
		BWAPI::Broodwar->printf("Attack position: (%d, %d)", attackPos.x, attackPos.y);

		unit->attack(attackPos);
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

		if (units.empty()) {
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
	return vector / getMagnitude(vector);
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