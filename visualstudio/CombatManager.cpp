#include "CombatManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"
#include "A-StarPathfinding.h"
#include "Squad.h"

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
}

void CombatManager::onStart(){
	std::map<int, int> unitSquadMap;

	// Only for testing on microtesting map
	for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
		if (unit->getType().isWorker() || unit->getType().isBuilding()
			|| unit->getType() == BWAPI::UnitTypes::Protoss_Observer) {
			continue;
		}
		if (commanderReference->scoutingManager.isScout(unit)) {
			continue;
		} 
		if (isAssigned(unit) == false) {
			assignUnit(unit);
		}
	}

	for (auto& squad : Squads) {
		squad.attack(squad.units.getPosition());
	}
	drawDebugInfo();
}

void CombatManager::onFrame() {
	// Only for testing on microtesting map
	for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
		if (unit->getType().isWorker() || unit->getType().isBuilding()
			|| unit->getType() == BWAPI::UnitTypes::Protoss_Observer) {
			continue;
		}
		if (commanderReference->scoutingManager.isScout(unit)) {
			continue;
		}
		if (isAssigned(unit) == false) {
			assignUnit(unit);
		}
	}

	for (auto& squad : Squads) {
		squad.attack(squad.units.getPosition());
	}
	drawDebugInfo();
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

void CombatManager::attack(BWAPI::Position position) {
	for (auto& squad : Squads) {
		squad.attack(position);
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

bool CombatManager::assignUnit(BWAPI::Unit unit)
{
	if (commanderReference->scoutingManager.isScout(unit)) {
		return false; // refuse: unit is a scout
	}

	for (auto& squad : Squads) {
		if (squad.units.size() < squad.unitSize) {
			squad.addUnit(unit);
			combatUnits.insert(unit);
			unitSquadIdMap[unit->getID()] = squad.squadId;
			return true;
		}
	}

	Squad& newSquad = addSquad();
	newSquad.addUnit(unit);
	combatUnits.insert(unit);
	unitSquadIdMap[unit->getID()] = newSquad.squadId;
	return true;
}

void CombatManager::move(BWAPI::Position position) {
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

BWAPI::Unit CombatManager::getAvailableUnit() {
	return getAvailableUnit([](BWAPI::Unit) { return true; });
}

BWAPI::Unit CombatManager::getAvailableUnit(std::function<bool(BWAPI::Unit)> filter) {
	for (auto& squad : Squads) {
		if (squad.isAttacking) continue;
		for (auto it = squad.units.begin(); it != squad.units.end(); ++it) {
			BWAPI::Unit u = *it;                   
			if (!u || !u->exists()) continue;
			if (commanderReference->scoutingManager.isScout(u)) continue;
			if (!filter(u)) continue;

			squad.removeUnit(u);                    
			combatUnits.erase(u);
			unitSquadIdMap.erase(u->getID());
			return u;
		}
	}
	return nullptr;
}