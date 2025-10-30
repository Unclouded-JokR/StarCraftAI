#include "CombatManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"
#include "Squad.h"

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
}

void CombatManager::onStart(){
	// Initializing one squad and the unit to squad map
	addSquad();
	std::map<int, int> unitSquadMap;
}

void CombatManager::onFrame() {
	update();
}

void CombatManager::onUnitDestroy(BWAPI::Unit unit) {
	// Remove unit from its squad
	if (isAssigned(unit)) {
		int squadId = unitSquadMap[unit->getID()];
		for (Squad& squad : Squads) {
			if (squad.squadId == squadId) {
				BWAPI::Broodwar->printf("Unit %d sent to Squad %d removal", unit->getID(), squadId);
				squad.removeUnit(unit);
				break;
			}
		}
		unitSquadMap.erase(unit->getID());
	}
}

void CombatManager::update() {
	// For now, any combat units already owned are assigned to a squad
	for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
		if (unit->getType().isWorker() || unit->getType().isBuilding()) {
			continue;
		}
		else if (isAssigned(unit) == false) {
			assignUnit(unit);
		}
	}

	attack();
	
	// More later
}

void CombatManager::attack() {
	for (Squad squad : Squads) {
		squad.attack();
	}
}

void CombatManager::addSquad(){
	std::vector<BWAPI::Color> palette = {
		BWAPI::Colors::Red, BWAPI::Colors::Blue, BWAPI::Colors::Green,
		BWAPI::Colors::Yellow, BWAPI::Colors::Purple, BWAPI::Colors::Cyan,
		BWAPI::Colors::Orange, BWAPI::Colors::Brown, BWAPI::Colors::White
	};
	BWAPI::Color randomColor = palette[std::rand() % palette.size()];

	int id = Squads.size() + 1;

	Squad newSquad(0, id, randomColor);
	Squads.push_back(newSquad);
}

void CombatManager::removeSquad(int squadId) {
	Squads.erase(std::remove_if(Squads.begin(), Squads.end(), [&](const Squad& obj) 
		{return obj.squadId == squadId; }),
		Squads.end());
}

// Function called by ProtoBot commander when unit is sent to combat manager

void CombatManager::assignUnit(BWAPI::Unit unit)
{
	for (auto& squad : Squads) {
		if (squad.units.size() < 12) {
			squad.addUnit(unit);
			unitSquadMap[unit->getID()] = squad.squadId;
			return;
		}
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