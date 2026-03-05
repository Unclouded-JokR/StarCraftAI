#include "CombatManager.h"
#include "ProtoBotCommander.h"
#include "Squad.h"

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
}

void CombatManager::onStart(){
	AStar::clearPathCache();
	AStar::fillUncachedAreaPairs();
}

void CombatManager::onFrame() {

	for (auto& squad : Squads) {
		squad->onFrame();
	}

	if ((BWAPI::Broodwar->getFrameCount() % FRAMES_BETWEEN_CACHING) == 0) {
		AStar::fillAreaPathCache();
	}


#ifdef DRAW_PRECACHE //In A-StarPathfinding.h
	MapTools map_tool = MapTools();
	if (!precachedPositions.empty()) {
		BWAPI::Position prevPos = precachedPositions.at(0);
		for (const auto& pos : precachedPositions) {
			map_tool.drawTile(BWAPI::TilePosition(pos).x, BWAPI::TilePosition(pos).y, BWAPI::Colors::Red);
			prevPos = pos;
		}
	}
#endif

#ifdef DEBUG_CM
	drawDebugInfo();
#endif
}

void CombatManager::onUnitDestroy(BWAPI::Unit unit) {
	if (isAssigned(unit)) {
		Squad* squad = unitSquadMap[unit];
		squad->removeUnit(unit);
		unitSquadMap.erase(unit);
		allUnits.erase(unit);

		if (squad->units.empty()) {
			removeSquad(squad);
		}

		unitSquadMap.erase(unit);
	}
}

void CombatManager::attack(BWAPI::Position position) {
	// Only process new attack if command position is a new position
	for (const auto& squad : Squads) {
		squad->commandPos = position;
		squad->setState(AttackingState::getInstance());
	}
}

void CombatManager::defend(BWAPI::Position position) {
	for (auto& squad : IdleSquads) {
		squad->currentDefensivePosition = position;
		squad->setState(DefendingState::getInstance());
	}
}

void CombatManager::reinforce(BWAPI::Position position) {
	for (auto& squad : DefendingSquads) {
		if (reinforceOutOfRange(squad, position)) {
			continue;
		}

		squad->commandPos = position;
		squad->setState(ReinforcingState::getInstance());
	}

	for (auto& squad : ReinforcingSquads) {
		if (reinforceOutOfRange(squad, position)) {
			squad->setState(DefendingState::getInstance());
			continue;
		}

		squad->commandPos = position;
	}
}

// Returns true if:
// - Current defensive position too far from reinforce position
// - Squad leader is too far from defensive position
bool CombatManager::reinforceOutOfRange(Squad* squad, BWAPI::Position reinforcePos) {
	if (squad->currentDefensivePosition.getApproxDistance(reinforcePos) > MAX_REINFORCE_DIST
		|| squad->currentDefensivePosition.getApproxDistance(squad->leader->getPosition()) > MAX_REINFORCE_DIST) {
		return true;
	}

	return false;
}

Squad* CombatManager::addSquad(BWAPI::Unit leaderUnit) {

	// Workaround for bwapi random color issue
	const int r = 50 + std::rand() % 200;
	const int g = 50 + std::rand() % 200;
	const int b = 50 + std::rand() % 200;

	const BWAPI::Color randomColor(r, g, b);

	const int id = Squads.size() + 1;

	Squad* newSquad = new Squad(leaderUnit, id, randomColor);
	Squads.push_back(newSquad);
	IdleSquads.push_back(newSquad);

#ifdef DEBUG_CM
	BWAPI::Broodwar->printf("Created new Squad %d with leader Unit %d", id, leaderUnit->getID());
#endif

	return newSquad;
}

void CombatManager::removeSquad(Squad* squad) {
	Squads.erase(remove(Squads.begin(), Squads.end(), squad), Squads.end());
	IdleSquads.erase(remove(IdleSquads.begin(), IdleSquads.end(), squad), IdleSquads.end());
	DefendingSquads.erase(remove(DefendingSquads.begin(), DefendingSquads.end(), squad), DefendingSquads.end());
	ReinforcingSquads.erase(remove(ReinforcingSquads.begin(), ReinforcingSquads.end(), squad), ReinforcingSquads.end());
	AttackingSquads.erase(remove(AttackingSquads.begin(), AttackingSquads.end(), squad), AttackingSquads.end());

	// Handling filled chokepoint locations in strategy manager
	for (const auto& cp : commanderReference->strategyManager.ProtoBotArea_SquadPlacements) {
		if (cp != nullptr && squad->currentDefensivePosition == BWAPI::Position(cp->Center())) {
			commanderReference->strategyManager.PositionsFilled[cp] = false;

#ifdef DEBUG_CM
			cout << "Removed squad from chokepoint position: " << BWAPI::Position(cp->Center()).x << "," << BWAPI::Position(cp->Center()).y << endl;
#endif
		}
	}

#ifdef DEBUG_CM
	BWAPI::Broodwar->printf("Removed Squad %d", squad->squadId);
#endif

	delete squad;
	squad = nullptr;
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
			allUnits.insert(unit);
			unitSquadMap[unit] = squad;
			return true;
		}
	}

	// If all squads are full or there are no squads, creates a new squad
	// Assigns first unit as the leader
	Squad* newSquad = addSquad(unit);
	if (newSquad == nullptr) {
		return false;
	}

	newSquad->addUnit(unit);
	allUnits.insert(unit);
	unitSquadMap[unit] = newSquad;
	return true;
}

bool CombatManager::isAssigned(BWAPI::Unit unit) {
	if (unitSquadMap.find(unit) != unitSquadMap.end()) {
		return true;
	}
	else {
		return false;
	}
}

void CombatManager::drawDebugInfo() {
	for (auto& squad : Squads) {
		squad->drawDebugInfo();
	}

	//for (auto& tile : closedTiles) {
	//	map_tool = MapTools();
	//	map_tool.drawTile(tile.first.x, tile.first.y, BWAPI::Colors::Red);
	//	BWAPI::Broodwar->drawTextMap(BWAPI::Position(tile.first) + BWAPI::Position(0, 16), "%.2f", tile.second);
	//}
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
			allUnits.erase(unit);
			unitSquadMap.erase(unit);
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


// Adding this in to strip scout units from squads if they somehow make it in -Marshall
bool CombatManager::detachUnit(BWAPI::Unit unit) {
	if (!unit) {
		return false;
	}

	auto itMap = unitSquadMap.find(unit);
	if (itMap == unitSquadMap.end()) {
		allUnits.erase(unit);
		return false;
	}

	Squad* squad = itMap->second;

	squad->removeUnit(unit);
	unitSquadMap.erase(unit);
	allUnits.erase(unit);

	if (squad->units.empty()) {
		removeSquad(squad);
	}
	else {
		// Optional: ensure leader is valid after removal
		if (!squad->leader || !squad->leader->exists()) {
			squad->leader = squad->units.front();
		}
	}

	unitSquadMap.erase(unit);
	allUnits.erase(unit);
	return true;
}