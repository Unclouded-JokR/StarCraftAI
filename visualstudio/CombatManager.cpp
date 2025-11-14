#include "CombatManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"
#include "Squad.h"

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
}

void CombatManager::onStart(){
	std::map<int, int> unitSquadMap;
}

void CombatManager::onFrame() {
	// For now, any combat units already owned are assigned to a squad
	for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
		if (unit->getType().isWorker() || unit->getType().isBuilding()) {
			continue;
		}

		if (isAssigned(unit) == false) {
			assignUnit(unit);
		}
	}

	attack();
}

void CombatManager::onUnitDestroy(BWAPI::Unit unit) {
	// Remove unit from its squad
	if (isAssigned(unit)) {
		int squadId = unitSquadIdMap[unit->getID()];

		for (Squad& squad : Squads) {
			if (squad.squadId == squadId) {
				squad.removeUnit(unit);

				// After removing unit, check if squad is empty
				if (squad.units.empty()) {
					removeSquad(squad.squadId);
				}

				break;
			}
		}

		unitSquadIdMap.erase(unit->getID());
	}
}

void CombatManager::attack() {
	for (auto& squad : Squads) {
		squad.attack();
	}
}

Squad& CombatManager::addSquad(){
	// Workaround for bwapi random color issue
	int r = 50 + std::rand() % 200;
	int g = 50 + std::rand() % 200;
	int b = 50 + std::rand() % 200;

	BWAPI::Color randomColor(r, g, b);

	int id = Squads.size() + 1;
	int unitSize = 8;

	Squad newSquad(0, id, randomColor, unitSize);
	Squads.push_back(newSquad);
	return Squads.back();
}

void CombatManager::removeSquad(int squadId){
	Squads.erase(std::remove_if(Squads.begin(), Squads.end(), [&](const Squad& obj)
		{return obj.squadId == squadId; }),
		Squads.end());

	for (auto it = unitSquadIdMap.begin(); it != unitSquadIdMap.end(); ) {
		if (it->second == squadId) {
			it = unitSquadIdMap.erase(it);
		} else {
			++it;
		}
	}

	BWAPI::Broodwar->printf("Removed empty Squad %d", squadId);
}

// Function called by ProtoBot commander when unit is sent to combat manager

Squad CombatManager::assignUnit(BWAPI::Unit unit)
{
	for (auto& squad : Squads) {
		if (squad.units.size() < squad.unitSize) {
			squad.addUnit(unit);
			combatUnits.insert(unit);
			unitSquadIdMap[unit->getID()] = squad.squadId;
			return squad;
		}
	}

	// Unit not assigned = no squads with space (creates a new squad)
	Squad& newSquad = addSquad();
	newSquad.addUnit(unit);
	combatUnits.insert(unit);
	unitSquadIdMap[unit->getID()] = newSquad.squadId;

	return newSquad;
}

void CombatManager::allSquadsMove(BWAPI::Position position) {
	for (auto& squad : Squads) {
		squad.move(position);
	}
}

bool CombatManager::isAssigned(BWAPI::Unit unit) {
	return unitSquadIdMap.find(unit->getID()) != unitSquadIdMap.end();
}

void CombatManager::drawDebugInfo() {
	for (Squad squad : Squads) {
		squad.drawDebugInfo();
	}
}

BWAPI::Unit CombatManager::getAvailableUnit(){
	for (auto& squad : Squads) {
		// If squad is busy, skip
		if (squad.isAttacking) {
			continue;
		}

		for (auto& unit : squad.units) {
			if (unit && unit->exists()) {
				squad.removeUnit(unit);
				return unit;
			}
		}

		return nullptr;
	}
}