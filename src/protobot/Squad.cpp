#include "Squad.h"
#include "BOIDS.h"

Squad::Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor)
{
	this->leader = leader;
	this->squadId = squadId;
	this->squadColor = squadColor;
}

void Squad::onFrame() {
	/*if (state == POSITIONING) {
		flockingHandler();
	}
	
	pathHandler();*/

	// Process current squad state
	currentState->Update(this);
}

void Squad::setState(SquadState& newState) {
	if (this == nullptr) {
		return;
	}

	currentState->Exit(this);
	currentState = &newState;
	currentState->Enter(this);
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

	AStar::drawPath(currentPath);
}

void Squad::removeUnit(BWAPI::Unit unit){
	if (unit == nullptr) {
		return;
	}

	if (unit == leader) {
		BOIDS::leaderRadiusMap.erase(unit);

		const BWAPI::Position leaderPos = unit->getPosition();
		std::erase_if(units, [unit](const BWAPI::Unit& _unit) {
			return unit->getID() == _unit->getID();
			});

		if (units.empty()){
			return;
		}

		// Closest unit to the leader is assigned as the new leader
		double closest = std::numeric_limits<double>::infinity();
		BWAPI::Unit closestUnit = nullptr;
		for (BWAPI::Unit _unit : units) {
			if (!_unit || !_unit->exists()) {
				continue;
			}

			const BWAPI::Position unitPos = _unit->getPosition();
			const double dist = unitPos.getDistance(leaderPos);

			if (dist < closest) {
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

void Squad::addUnit(BWAPI::Unit unit) {
	if (unit == nullptr) {
		return;
	}

	if (std::find(units.begin(), units.end(), unit) != units.end()) {
		// Avoids adding duplicate units
		return;
	}

	units.push_back(unit);

	// Unit should do whatever the squad is currently doing
	if (currentState == &DefendingState::getInstance()) {
		unit->attack(currentDefensivePosition);
	}
	if (currentState == &ReinforcingState::getInstance()) {
		unit->attack(currentReinforcePosition);
	}
	if (currentState == &AttackingState::getInstance()) {
		unit->attack(currentAttackPosition);
	}
#ifdef DEBUG_SQUAD
	BWAPI::Broodwar->printf("Unit %d added to Squad %d", unit->getID(), squadId);
#endif
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

void Squad::drawDebugInfo() {
	for (auto& unit : units) {
		if (!unit || !unit->exists()) {
			continue;
		}

		int yOffset = 5;
		int count = 0;

		// Draw lines and shapes to debug commands
		const BWAPI::UnitCommand command = unit->getLastCommand();

		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 5, squadColor, true);
		if (unit == leader) {
			BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 2, BWAPI::Colors::White, true);
			BWAPI::Broodwar->drawTextMap(BWAPI::Position(unit->getPosition().x - 12, unit->getPosition().y + 20), "LEADER", BWAPI::Colors::White);

			if (currentState == &AttackingState::getInstance()) {
				BWAPI::Broodwar->drawTextMap(BWAPI::Position(unit->getPosition().x - 16, unit->getPosition().y + 25), "-ATTACKING-", BWAPI::Colors::Orange);
			}
			if (currentState == &DefendingState::getInstance()) {
				BWAPI::Broodwar->drawTextMap(BWAPI::Position(unit->getPosition().x - 16, unit->getPosition().y + 25), "-DEFENDING-", BWAPI::Colors::Orange);
			}
			if (currentState == &IdleState::getInstance()) {
				BWAPI::Broodwar->drawTextMap(BWAPI::Position(unit->getPosition().x - 16, unit->getPosition().y + 25), "-IDLE-", BWAPI::Colors::Orange);
			}
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
		BWAPI::Broodwar->drawTextMap(textPos, "%d", squadId);
	}
}