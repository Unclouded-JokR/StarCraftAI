#include "Squad.h"
#include "BOIDS.h"

Squad::Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor)
{
	this->leader = leader;
	this->info.squadId = squadId;
	this->info.squadColor = squadColor;
}

void Squad::onFrame() {
	// Process current squad state
	info.currentState->Update(this);
	drawDebugInfo();
}

void Squad::setState(SquadState& newState) {
	if (info.currentState != nullptr) {
		info.currentState->Exit(this);
	}
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

void Squad::drawDebugInfo() {
	for (auto& unit : info.units) {
		if (!unit || !unit->exists()) {
			continue;
		}

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