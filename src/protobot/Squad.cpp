#include "Squad.h"
#include "BOIDS.h"

Squad::Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor)
{
	this->leader = leader;
	this->info.squadId = squadId;
	this->info.squadColor = squadColor;
}

void Squad::onFrame() {
	/*if (state == POSITIONING) {
		flockingHandler();
	}
	
	pathHandler();*/

	// Process current squad state
	info.currentState->Update(this);
}

void Squad::setState(SquadState& newState) {
	if (this == nullptr) {
		return;
	}

	info.currentState->Exit(this);
	info.currentState = &newState;
	info.currentState->Enter(this);
}

void Squad::pathHandler() {
	const int distThreshold = 1;
	if (info.currentPath.positions.empty() == false && info.currentPathIdx < info.currentPath.positions.size()) {
		const BWAPI::Position target = BWAPI::Position(info.currentPath.positions.at(info.currentPathIdx));
		if (leader->getDistance(target) <= distThreshold){
			info.currentPathIdx += 1;
		}
		else if (leader->getTargetPosition() != target) {
			leader->attack(target);
		}
	}

	for (pair<BWAPI::Position, BWAPI::Position> rect : rectCoordinates) {
		BWAPI::Broodwar->drawBoxMap(rect.first, rect.second, BWAPI::Colors::Yellow);
	}

	AStar::drawPath(info.currentPath);
}

void Squad::removeUnit(BWAPI::Unit unit){
	if (unit == nullptr) {
		return;
	}

	if (unit == leader) {
		BOIDS::leaderRadiusMap.erase(unit);
		const BWAPI::Position leaderPos = unit->getPosition();
		std::erase_if(info.units, [unit](const BWAPI::Unit& _unit) {
			return unit->getID() == _unit->getID();
			});

		if (info.units.empty()){
			return;
		}

		// Closest unit to the leader is assigned as the new leader
		double closest = std::numeric_limits<double>::infinity();
		BWAPI::Unit closestUnit = nullptr;
		for (BWAPI::Unit _unit : info.units) {
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
		info.units.erase(std::remove(info.units.begin(), info.units.end(), unit), info.units.end());
	}
}

void Squad::addUnit(BWAPI::Unit unit) {
	if (unit == nullptr) {
		return;
	}

	if (std::find(info.units.begin(), info.units.end(), unit) != info.units.end()) {
		// Avoids adding duplicate units
		return;
	}

	info.units.push_back(unit);

	// Unit should do whatever the squad is currently doing
	if (info.currentState == &DefendingState::getInstance()) {
		unit->attack(info.currentDefensivePosition);
	}
	if (info.currentState == &ReinforcingState::getInstance()) {
		unit->attack(info.currentReinforcePosition);
	}
	if (info.currentState == &AttackingState::getInstance()) {
		unit->attack(info.currentAttackPosition);
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
	for (auto& unit : info.units) {
		if (!unit || !unit->exists()) {
			continue;
		}

		int yOffset = 5;
		int count = 0;

		// Draw lines and shapes to debug commands
		const BWAPI::UnitCommand command = unit->getLastCommand();

		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 5, info.squadColor, true);
		if (unit == leader) {
			BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 2, BWAPI::Colors::White, true);
			BWAPI::Broodwar->drawTextMap(BWAPI::Position(unit->getPosition().x - 12, unit->getPosition().y + 20), "LEADER", BWAPI::Colors::White);

			if (info.currentState == &AttackingState::getInstance()) {
				BWAPI::Broodwar->drawTextMap(BWAPI::Position(unit->getPosition().x - 16, unit->getPosition().y + 25), "-ATTACKING-", BWAPI::Colors::Orange);
			}
			if (info.currentState == &DefendingState::getInstance()) {
				BWAPI::Broodwar->drawTextMap(BWAPI::Position(unit->getPosition().x - 16, unit->getPosition().y + 25), "-DEFENDING-", BWAPI::Colors::Orange);
			}
			if (info.currentState == &IdleState::getInstance()) {
				BWAPI::Broodwar->drawTextMap(BWAPI::Position(unit->getPosition().x - 16, unit->getPosition().y + 25), "-IDLE-", BWAPI::Colors::Orange);
			}
		}

		if (command.getTargetPosition() != BWAPI::Positions::None) {
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), command.getTargetPosition(), info.squadColor);
		}
		if (command.getTargetTilePosition() != BWAPI::TilePositions::None) {
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), BWAPI::Position(command.getTargetTilePosition()), info.squadColor);
		}
		if (command.getTarget() != nullptr) {
			BWAPI::Broodwar->drawLineMap(unit->getPosition(), command.getTarget()->getPosition(), info.squadColor);
		}

		// Draws squad ID below unit
		const BWAPI::Position textPos(unit->getPosition().x - 20, unit->getPosition().y + 20);
		BWAPI::Broodwar->drawTextMap(textPos, "%d", info.squadId);
	}
}