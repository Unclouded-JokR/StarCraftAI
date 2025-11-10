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

void Squad::attack() {
	isAttacking = true;
	// Each unit in squad attacks closest enemy unit for now
	for (BWAPI::Unit unit : units) {
		BWAPI::Unitset enemyUnits = BWAPI::Broodwar->enemy()->getUnits();
		BWAPI::Unit closestEnemy = Tools::GetClosestUnitTo(unit, enemyUnits);

		if (!unit->isAttacking() && unit->canAttack()) {
			if (BWAPI::Broodwar->getFrameCount() % 20 == 0) {
				unit->attack(closestEnemy);
			}
		}
	}

	drawDebugInfo();
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