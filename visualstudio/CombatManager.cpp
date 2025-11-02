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
		int squadId = unitSquadMap[unit->getID()];
		for (Squad& squad : Squads) {
			if (squad.squadId == squadId) {
				squad.removeUnit(unit);
				break;
			}
		}
		unitSquadMap.erase(unit->getID());
		// Check if squad is now empty
		removeEmptySquads();
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
	int unitSize = 2;

	Squad newSquad(0, id, randomColor, unitSize);
	Squads.push_back(newSquad);
	return Squads.back();
}

void CombatManager::removeSquad(int squadId) {
	Squads.erase(std::remove_if(Squads.begin(), Squads.end(), [&](const Squad& obj) 
		{return obj.squadId == squadId; }),
		Squads.end());
}

void CombatManager::removeEmptySquads(){
	int removedSquadId;
	for (auto& squad : Squads){
		if (squad.units.empty()) {
			removedSquadId = squad.squadId;
			removeSquad(squad.squadId);
			BWAPI::Broodwar->printf("Removed empty Squad %d", removedSquadId);

			// Also remove from unitSquadMap
			for (auto it = unitSquadMap.begin(); it != unitSquadMap.end(); ) {
				if (it->second == removedSquadId) {
					it = unitSquadMap.erase(it);
				} else {
					++it;
				}
			}

			break;
		}
	}
}

// Function called by ProtoBot commander when unit is sent to combat manager

Squad CombatManager::assignUnit(BWAPI::Unit unit)
{
	for (auto& squad : Squads) {
		if (squad.units.size() < squad.unitSize) {
			squad.addUnit(unit);
			combatUnits.insert(unit);
			unitSquadMap[unit->getID()] = squad.squadId;
			return squad;
		}
	}

	// Unit not assigned = no squads with space (creates a new squad)
	Squad& newSquad = addSquad();
	newSquad.addUnit(unit);
	combatUnits.insert(unit);
	unitSquadMap[unit->getID()] = newSquad.squadId;
	return newSquad;
}

void CombatManager::allSquadsMove(BWAPI::Position position) {
	for (auto& squad : Squads) {
		squad.move(position);
	}
}

bool CombatManager::isAssigned(BWAPI::Unit unit) {
	return unitSquadMap.find(unit->getID()) != unitSquadMap.end();
}

void CombatManager::drawDebugInfo() {
	for (Squad squad : Squads) {
		squad.drawDebugInfo();
	}
}