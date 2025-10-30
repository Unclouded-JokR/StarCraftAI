#include <BWAPI.h>
#include "Squad.h"
#include "../src/starterbot/Tools.h"

Squad::Squad(int squadType, int squadId, BWAPI::Color squadColor)
{
	this->squadType = squadType;
	this->squadId = squadId;
	this->squadColor = squadColor;
	this->units = BWAPI::Unitset();
}

void Squad::removeUnit(BWAPI::Unit unit) {
	units.erase(unit);
}

void Squad::move(BWAPI::Position position) {
	for (auto& unit : units) {
		unit->move(position);
	}
}

void Squad::addUnit(BWAPI::Unit unit) {
	units.insert(unit);
	BWAPI::Broodwar->printf("Unit %d added to Squad %d", unit->getID(), squadId);
}

void Squad::attack() {
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
	}
}