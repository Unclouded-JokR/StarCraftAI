#include "CombatManager.h"
#include "ProtoBotCommander.h"

map<SquadState*, BWAPI::Color> CombatManager::stateColorMap = {
		{&AttackingState::getInstance(), BWAPI::Colors::Red},
		{&DefendingState::getInstance(), BWAPI::Colors::Green},
		{&ReinforcingState::getInstance(), BWAPI::Colors::Yellow},
		{&IdleState::getInstance(), BWAPI::Colors::White}
};

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
	
}

void CombatManager::onStart(){
	AStar::clearPathCache();
	AStar::fillUncachedAreaPairs();
}

void CombatManager::onFrame() {

	if ((BWAPI::Broodwar->getFrameCount() % FRAMES_BETWEEN_CACHING) == 0) {
		AStar::fillAreaPathCache();
	}

	/*for (const auto& squad : Squads) {
		BOIDS::squadFlock(squad);
	}*/

	for (const auto& squad : Squads) {
		squad->onFrame();

#ifdef DEBUG_CM
		int leftMost = 0;
		int topMost = 0;
		int rightMost = 0;
		int bottomMost = 0;

		// Boundary box of the squad
		for (const auto& unit : squad->info.units) {
			int dist = unit->getPosition().getApproxDistance(squad->leader->getPosition());
			if (dist < 500) {
				int x = unit->getPosition().x - squad->leader->getPosition().x;
				int y = unit->getPosition().y - squad->leader->getPosition().y;

				if (x < leftMost) { leftMost = x; }
				if (x > rightMost) { rightMost = x; }
				if (y < topMost) { topMost = y; }
				if (y > bottomMost) { bottomMost = y; }
			}
		}

		// Drawing squad boundary box (color based on squad state)
		const BWAPI::Position topLeft = squad->leader->getPosition() + BWAPI::Position(leftMost, topMost);
		const BWAPI::Position bottomRight = squad->leader->getPosition() + BWAPI::Position(rightMost, bottomMost);
		BWAPI::Broodwar->drawBoxMap(topLeft, bottomRight, stateColorMap[squad->info.currentState]);
#endif 
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
}

void CombatManager::onUnitDestroy(BWAPI::Unit unit) {
	if (isAssigned(unit)) {
		Squad* squad = unitSquadMap[unit];
		squad->removeUnit(unit);
		unitSquadMap.erase(unit);
		allUnits.erase(unit);

		if (squad->info.units.empty()) {
			removeSquad(squad);
		}

		unitSquadMap.erase(unit);
	}
}

void CombatManager::attack(BWAPI::Position position) {
	attacking = true;
	globalAttackPosition = position;
	
	for (const auto& squad : Squads) {
		squad->info.commandPos = position;
		squad->setState(AttackingState::getInstance());
	}
}

void CombatManager::defend(BWAPI::Position position) {
	attacking = false;
	for (auto& squad : IdleSquads) {
		squad->info.currentDefensivePosition = position;
		squad->setState(DefendingState::getInstance());
	}
}

void CombatManager::reinforce(BWAPI::Position position) {
	attacking = false;
	for (const auto& squad : DefendingSquads) {
		if (squad->info.currentDefensivePosition.getApproxDistance(position) > MAX_REINFORCE_DIST) {
			continue;
		}
		else {
			squad->info.commandPos = position;
			squad->setState(ReinforcingState::getInstance());
		}
	}

	for (auto& squad : ReinforcingSquads) {
		squad->info.commandPos = position;
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
	Squads.push_back(newSquad);

	newSquad->setState(IdleState::getInstance());

	if (attacking) {
		newSquad->info.commandPos = globalAttackPosition;
		newSquad->setState(AttackingState::getInstance());
	}

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
		if (cp != nullptr && squad->info.currentDefensivePosition == BWAPI::Position(cp->Center())) {
			commanderReference->strategyManager.PositionsFilled[cp] = false;

#ifdef DEBUG_CM
			cout << "Removed squad from chokepoint position: " << BWAPI::Position(cp->Center()).x << "," << BWAPI::Position(cp->Center()).y << endl;
#endif
		}
	}

#ifdef DEBUG_CM
	BWAPI::Broodwar->printf("Removed Squad %d", squad->info.squadId);
#endif

	delete squad;
	squad = nullptr;
}

// Function called by ProtoBot commander when unit is sent to combat manager

bool CombatManager::assignUnit(BWAPI::Unit unit)
{
	if (!unit->exists()) {
		return false;
	}

	if (commanderReference->scoutingManager.isScout(unit)) {
		return false; // refuse: unit is a scout
	}

	// Assigning to an existing squad if available
	for (auto& squad : Squads) {
		if (squad->info.units.size() < MAX_SQUAD_SIZE) {
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

BWAPI::Unit CombatManager::getAvailableUnit() {
	return getAvailableUnit([](BWAPI::Unit) { return true; });
}

BWAPI::Unit CombatManager::getAvailableUnit(std::function<bool(BWAPI::Unit)> filter) {
	for (auto& squad : Squads) {
		for (auto it = squad->info.units.begin(); it != squad->info.units.end(); ++it) {
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
	if (text == "/attackPosition") {
		cout << "Attacking: " << globalAttackPosition << endl;
	}

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
	if (BWAPI::Broodwar->mapName() == "BOIDSTesting") {
		leftPos = BWAPI::Position(1481, 1842);
		rightPos = BWAPI::Position(2654, 1822);
		upPos = BWAPI::Position(2047, 1505);
		downPos = BWAPI::Position(2047, 2302);
		centerPos = BWAPI::Position(2047, 1855);
	}

	for (const auto& squad : Squads) {
		const BWAPI::Position leaderPos = squad->leader->getPosition();
		const BWAPI::UnitType leaderType = squad->leader->getType();
		vector<BWAPI::Position> tiles;
		if (text == "left") {
			squad->leader->attack(leftPos);
#ifdef ASTAR_COMMANDING
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, leftPos);
#endif
		}
		if (text == "right") {
			squad->leader->attack(rightPos);
#ifdef ASTAR_COMMANDING
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, rightPos);
#endif
		}if (text == "up") {
			squad->leader->attack(upPos);
#ifdef ASTAR_COMMANDING
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, upPos);
#endif
		}if (text == "down") {
			squad->leader->attack(downPos);
#ifdef ASTAR_COMMANDING
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, downPos);
#endif
		}if (text == "center") {
			squad->leader->attack(centerPos);
#ifdef ASTAR_COMMANDING
			squad->currentPathIdx = 0;
			squad->currentPath = AStar::GeneratePath(leaderPos, leaderType, centerPos);
#endif
		}

		vector<BWAPI::Position> positions = squad->info.currentPath.positions;
		const int dist = squad->info.currentPath.distance;
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

	if (squad->info.units.empty()) {
		removeSquad(squad);
	}
	else {
		// Optional: ensure leader is valid after removal
		if (!squad->leader || !squad->leader->exists()) {
			squad->leader = squad->info.units.front();
		}
	}

	unitSquadMap.erase(unit);
	allUnits.erase(unit);
	return true;
}