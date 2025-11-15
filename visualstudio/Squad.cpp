#include <BWAPI.h>
#include "Squad.h"
#include "../src/starterbot/Tools.h"

Squad::Squad(int squadType, int squadId, BWAPI::Color squadColor, int unitSize)
{
	this->squadType = squadType;
	this->squadId = squadId;
	this->squadColor = squadColor;
	this->unitSize = unitSize;
	this->units = BWAPI::Unitset();
}

void Squad::removeUnit(BWAPI::Unit unit) {
	units.erase(unit);
}

void Squad::move(BWAPI::Position position) {
	units.attack(position);
}

void Squad::addUnit(BWAPI::Unit unit) {
	if (std::find(units.begin(), units.end(), unit) != units.end()) {
		// Avoids adding duplicate units
		return;
	}

	units.insert(unit);
	BWAPI::Broodwar->printf("Unit %d added to Squad %d", unit->getID(), squadId);
}

void Squad::attack(BWAPI::Position initialAttackPos) {
	BWAPI::Position squadPos = units.getPosition();
	BWAPI::Unitset enemies = BWAPI::Broodwar->enemy()->getUnits();
	BWAPI::Unit closestEnemy = Tools::GetClosestUnitTo(squadPos, enemies);
	if (!closestEnemy) { return; }

	for (auto& unit : units) {
		closestEnemy = Tools::GetClosestUnitTo(unit->getPosition(), enemies);
		if (!closestEnemy) {
			unit->attack(initialAttackPos);
		}
		
		// If units are the same UnitType, attack normally (same range, same firerate, etc. makes kiting useless for now)
		// Also, if unit is melee/short-range, attack normally
		if (closestEnemy->getType() == unit->getType() || unit->getType().groundWeapon().maxRange() <= 32) {
			attackUnit(unit, closestEnemy);
		}
		else {
			attackKite(unit, closestEnemy);
		}
	}
}

// Despite the name, this method is not about attack-moving
// Acts as the move command when attacking/kiting
void Squad::attackMove(BWAPI::Unit unit, BWAPI::Position position) {

	// If unit or position is invalid, return
	if (!unit) {
		return;
	}

	// If position invalid, check for valid position around it
	if (!position.isValid()) {
		BWAPI::Position newPos;
		int searches = 0;
		int min = -50;
		int max = 50;

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

	// If the target is cloaked, RUN TOWARDS BASE!!!
	if (target->isCloaked()) {
		BWAPI::Unitset selfUnits = BWAPI::Broodwar->self()->getUnits();
		BWAPI::Position kitePosition;

		// If cloaked enemy detected and can't attack it, run towards Nexus
		for (auto& unit : selfUnits) {
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus) {
				kitePosition = unit->getPosition();
				break;
			}
		}
		unit->move(kitePosition);
		return;
	}

	// If unit is already attacking, ignore
	if (unit->isStartingAttack() || unit->isAttackFrame()) {
		return;
	}

	// Otherwise, attack the target
	unit->attack(target);
}
void Squad::attackKite(BWAPI::Unit unit, BWAPI::Unit target) {

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
		BWAPI::Position kitePosition;

		// If cloaked enemy detected and can't attack it, run towards Nexus
		for (auto& unit : selfUnits) {
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus) {
				kitePosition = unit->getPosition();
				break;
			}
		}
		unit->move(kitePosition);
		return;
	}
	
	// If unit is already attacking, ignore
	if (unit->isStartingAttack() || unit->isAttackFrame()) {
		return;
	}

	BWAPI::Position kiteVector = unit->getPosition() - target->getPosition();
	BWAPI::Position kitePosition = unit->getPosition() + kiteVector;

	double distance = unit->getDistance(target);
	double speed = unit->getType().topSpeed();
	double range = unit->getType().groundWeapon().maxRange();

	double timeToEnterRange = (distance - range) / speed;
	// If weapon cooldown will be ready by the time we enter range, go and attack the target
	// Otherwise, kite
	if (unit->getGroundWeaponCooldown() <= timeToEnterRange || target->getType().isBuilding()) {
		attackUnit(unit, target);
	}
	else if (unit->getGroundWeaponCooldown() == 0 && distance <= range) {
		attackUnit(unit, target);
	}
	else {
		attackMove(unit, kitePosition);
	}
}

void Squad::drawDebugInfo() {
	for (auto & unit : units) {
		if (!unit || !unit->exists()) {
			continue;
		}

		// Draw lines and shapes to debug commands
		BWAPI::UnitCommand command = unit->getLastCommand();

		BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 5, squadColor, true);
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
		BWAPI::Position textPos(unit->getPosition().x - 20, unit->getPosition().y + 20);
		BWAPI::Broodwar->drawTextMap(textPos, "Squad %d", squadId);
	}
}