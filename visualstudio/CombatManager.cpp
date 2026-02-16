#include "CombatManager.h"
#include "ProtoBotCommander.h"
#include "Squad.h"

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
}

void CombatManager::onStart(){
	AStar::fillAreaPathCache();
}

void CombatManager::onFrame() {
	for (auto& squad : Squads) {
		squad->onFrame();
	}

	for (auto& tile : closedTiles) {
		BWAPI::Broodwar->drawBoxMap(BWAPI::Position(tile.first), BWAPI::Position(tile.first) + BWAPI::Position(32, 32), BWAPI::Colors::Red);
		BWAPI::Broodwar->drawTextMap(BWAPI::Position(tile.first) + BWAPI::Position(0, 16), "%.2f", tile.second);
	}
	for (auto& tile : earlyExpansionTiles) {
		BWAPI::Broodwar->drawBoxMap(BWAPI::Position(tile), BWAPI::Position(tile) + BWAPI::Position(32, 32), BWAPI::Colors::Yellow);
	}

	drawDebugInfo();
}

void CombatManager::onUnitDestroy(BWAPI::Unit unit) {
	for (auto& squad : Squads) {
		if (squad->squadId == unitSquadIdMap[unit]) {
			squad->removeUnit(unit);
			unitSquadIdMap.erase(unit);
			totalCombatUnits.erase(unit);

			if (squad->units.empty()) {
				removeSquad(squad);
			}
			break;
		}
	}

	unitSquadIdMap.erase(unit);
}

void CombatManager::attack(BWAPI::Position position) {
	for (auto& squad : DefendingSquads) {
		squad->commandPos = position;
		squad->setState(AttackingState::getInstance());

		DefendingSquads.erase(std::remove(DefendingSquads.begin(), DefendingSquads.end(), squad), DefendingSquads.end());
		AttackingSquads.push_back(squad);
	}
	for (auto& squad : IdleSquads) {
		squad->commandPos = position;
		squad->setState(AttackingState::getInstance());

		IdleSquads.erase(std::remove(IdleSquads.begin(), IdleSquads.end(), squad), IdleSquads.end());
		AttackingSquads.push_back(squad);
	}
}

void CombatManager::defend(BWAPI::Position position) {
	for (auto& squad : IdleSquads) {
		IdleSquads.erase(std::remove(IdleSquads.begin(), IdleSquads.end(), squad), IdleSquads.end());
		DefendingSquads.push_back(squad);

		squad->commandPos = position;
		squad->setState(DefendingState::getInstance());
	}
}

Squad* CombatManager::addSquad(BWAPI::Unit leaderUnit) {

	// Workaround for bwapi random color issue
	const int r = 50 + std::rand() % 200;
	const int g = 50 + std::rand() % 200;
	const int b = 50 + std::rand() % 200;

	const BWAPI::Color randomColor(r, g, b);

	const int id = Squads.size() + 1;

	Squad* newSquad = new Squad(leaderUnit, id, randomColor);
	BWAPI::Broodwar->printf("Created new Squad %d with leader Unit %d", id, leaderUnit->getID());
	Squads.push_back(newSquad);
	IdleSquads.push_back(newSquad);

	return newSquad;
}

void CombatManager::removeSquad(Squad* squad) {
	Squads.erase(std::remove(Squads.begin(), Squads.end(), squad), Squads.end());

	BWAPI::Broodwar->printf("Removed empty Squad %d", squad->squadId);
}

// Function called by ProtoBot commander when unit is sent to combat manager

bool CombatManager::assignUnit(BWAPI::Unit unit)
{
	if (commanderReference->scoutingManager.isScout(unit)) {
		return false; // refuse: unit is a scout
	}

	for (auto& squad : Squads) {
		if (squad->units.size() < MAX_SQUAD_SIZE) {
			squad->addUnit(unit);
			totalCombatUnits.insert(unit);
			unitSquadIdMap[unit] = squad->squadId;
			return true;
		}
	}

	// If all squads are full or there are no squads, creates a new squad
	// Assigns first unit as the leader
	Squad* newSquad = addSquad(unit);
	newSquad->addUnit(unit);
	totalCombatUnits.insert(unit);
	unitSquadIdMap[unit] = newSquad->squadId;
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

void CombatManager::drawDebugInfo() {
	for (auto& squad : Squads) {
		squad->drawDebugInfo();
	}
}

BWAPI::Unit CombatManager::getAvailableUnit() {
	return getAvailableUnit([](BWAPI::Unit) { return true; });
}

BWAPI::Unit CombatManager::getAvailableUnit(std::function<bool(BWAPI::Unit)> filter) {
	for (auto& squad : Squads) {
		for (auto it = squad->units.begin(); it != squad->units.end(); ++it) {
			BWAPI::Unit unit = *it;
			if (!unit || !unit->exists()) continue;
			if (commanderReference->scoutingManager.isScout(unit)) continue;
			if (!filter(unit)) continue;

			squad->removeUnit(unit);
			totalCombatUnits.erase(unit);
			unitSquadIdMap.erase(unit);
			return unit;
		}
	}

	return nullptr;
}

void CombatManager::handleTextCommand(std::string text) {
	//PRESET POSITIONS FOR USE IN CUSTOM MAPS
	BWAPI::Position leftPos = BWAPI::Position(0, 0);
	BWAPI::Position rightPos = BWAPI::Position(0, 0);
	BWAPI::Position upPos = BWAPI::Position(0, 0);
	BWAPI::Position downPos = BWAPI::Position(0, 0);
	BWAPI::Position centerPos = BWAPI::Position(0, 0);

	if (BWAPI::Broodwar->mapName() == "pathMaze") {
		leftPos = BWAPI::Position(705, 2626);
		rightPos = BWAPI::Position(3000, 383);
		upPos = BWAPI::Position(831, 450);
		downPos = BWAPI::Position(2944, 2569);
		centerPos = BWAPI::Position(1600, 1505);
	}
	if (BWAPI::Broodwar->mapName() == "pathBuildingTest") {
		leftPos = BWAPI::Position(513, 508);
		rightPos = BWAPI::Position(1056, 192);
		centerPos = BWAPI::Position(690, 362);
	}

	cout << "Map name: " << BWAPI::Broodwar->mapName() << endl;

	for (auto& squad : Squads) {
		const BWAPI::Position leaderPos = squad->leader->getPosition();
		const BWAPI::UnitType leaderType = squad->leader->getType();
		vector<BWAPI::Position> tiles;
		if (text == "left") {
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, leftPos);
		}
		if (text == "right") {
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, rightPos);
		}if (text == "up") {
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, upPos);
		}if (text == "down") {
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, downPos);
		}if (text == "center") {
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, centerPos);
		}

		vector<BWAPI::Position> positions = squad->currentPath.positions;
		const int dist = squad->currentPath.distance;
		if (positions.size() > 1) {
			BWAPI::Broodwar->printf("Current path: (%d, %d) %d", positions.at(positions.size() - 1).x, positions.at(positions.size() - 1).y, dist);
		}
	}
}