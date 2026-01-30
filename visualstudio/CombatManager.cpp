#include "CombatManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"
#include "Squad.h"

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
}

void CombatManager::onStart(){
	// Only for testing on microtesting map
	/*for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
		if (unit->getType().isWorker() || unit->getType().isBuilding()
			|| unit->getType() == BWAPI::UnitTypes::Protoss_Observer) {
			continue;
		}
		if (commanderReference->scoutingManager.isScout(unit)) {
			continue;
		} 
		if (isAssigned(unit) == -1) {
			assignUnit(unit);
		}
	}*/
}

void CombatManager::onFrame() {
	for (auto& squad : Squads) {
		squad.onFrame();
	}

	drawDebugInfo();
}

void CombatManager::onUnitDestroy(BWAPI::Unit unit) {
	for (auto& squad : Squads) {
		if (squad.squadId == unitSquadIdMap[unit]) {
			squad.removeUnit(unit);
			unitSquadIdMap.erase(unit);
			combatUnits.erase(unit);

			if (squad.units.empty()) {
				removeSquad(squad);
			}
			break;
		}
	}

	unitSquadIdMap.erase(unit);
}

void CombatManager::attack(BWAPI::Position position) {
	for (auto& squad : Squads) {
		squad.move(position);
	}
}

Squad& CombatManager::addSquad(BWAPI::Unit leaderUnit) {

	// Workaround for bwapi random color issue
	const int r = 50 + std::rand() % 200;
	const int g = 50 + std::rand() % 200;
	const int b = 50 + std::rand() % 200;

	const BWAPI::Color randomColor(r, g, b);

	const int id = Squads.size() + 1;
	const int unitSize = 12;

	Squad newSquad(leaderUnit, id, randomColor, unitSize);
	BWAPI::Broodwar->printf("Created new Squad %d with leader Unit %d", id, leaderUnit->getID());
	Squads.push_back(newSquad);

	return Squads.back();
}

void CombatManager::removeSquad(Squad squad) {
	Squads.erase(std::remove(Squads.begin(), Squads.end(), squad), Squads.end());

	BWAPI::Broodwar->printf("Removed empty Squad %d", squad.squadId);
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
			unitSquadIdMap[unit] = squad.squadId;
			return true;
		}
	}

	// If all squads are full or there are no squads, creates a new squad
	// Assigns first unit as the leader
	Squad& newSquad = addSquad(unit);
	newSquad.addUnit(unit);
	combatUnits.insert(unit);
	unitSquadIdMap[unit] = newSquad.squadId;
	return true;
}

int CombatManager::isAssigned(BWAPI::Unit unit) {
	if (unitSquadIdMap.find(unit) != unitSquadIdMap.end()) {
		return unitSquadIdMap[unit];
	}
	else {
		return -1;
	}
}

void CombatManager::move(BWAPI::Position position) {
	for (Squad& squad : Squads) {
		squad.move(position);
	}
}

void CombatManager::drawDebugInfo() {
	for (Squad& squad : Squads) {
		squad.drawDebugInfo();
	}
}

BWAPI::Unit CombatManager::getAvailableUnit() {
	return getAvailableUnit([](BWAPI::Unit) { return true; });
}

BWAPI::Unit CombatManager::getAvailableUnit(std::function<bool(BWAPI::Unit)> filter) {
	for (auto& squad : Squads) {
		if (squad.state == ATTACK) continue;
		for (auto it = squad.units.begin(); it != squad.units.end(); ++it) {
			BWAPI::Unit unit = *it;
			if (!unit || !unit->exists()) continue;
			if (commanderReference->scoutingManager.isScout(unit)) continue;
			if (!filter(unit)) continue;

			squad.removeUnit(unit);
			combatUnits.erase(unit);
			unitSquadIdMap.erase(unit);
			return unit;
		}
	}

	return nullptr;
}

void CombatManager::handleTextCommand(std::string text) {
	//PRESET POSITIONS FOR CUSTOM MAP ../pathPlayground.scx
	const BWAPI::Position leftPos = BWAPI::Position(224, 450);
	const BWAPI::Position rightPos = BWAPI::Position(1407, 450);
	const BWAPI::Position upPos = BWAPI::Position(832, 64);
	const BWAPI::Position downPos = BWAPI::Position(832, 895);
	const BWAPI::Position centerPos = BWAPI::Position(832, 449);

	for (Squad& squad : Squads) {
		const BWAPI::Position leaderPos = squad.leader->getPosition();
		const BWAPI::UnitType leaderType = squad.leader->getType();
		vector<BWAPI::Position> tiles;
		if (text == "left") {
			squad.currentPathIdx = 0;
			squad.currentPath = AStar::GeneratePath(leaderPos, leaderType, leftPos);
		}
		if (text == "right") {
			squad.currentPathIdx = 0;
			squad.currentPath = AStar::GeneratePath(leaderPos, leaderType, rightPos);
		}if (text == "up") {
			squad.currentPathIdx = 0;
			squad.currentPath = AStar::GeneratePath(leaderPos, leaderType, upPos);
		}if (text == "down") {
			squad.currentPathIdx = 0;
			squad.currentPath = AStar::GeneratePath(leaderPos, leaderType, downPos);
		}if (text == "center") {
			squad.currentPathIdx = 0;
			squad.currentPath = AStar::GeneratePath(leaderPos, leaderType, centerPos);
		}

		vector<BWAPI::Position> positions = squad.currentPath.positions;
		const int dist = squad.currentPath.distance;
		BWAPI::Broodwar->printf("Current path: (%d, %d) %d", positions.at(positions.size()-1).x, positions.at(positions.size()-1).y, dist);
	}
}