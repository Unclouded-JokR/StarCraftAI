#include "CombatManager.h"
#include "ProtoBotCommander.h"

// Initializing static variables
map<SquadState*, BWAPI::Color> CombatManager::stateColorMap = {
		{&AttackingState::getInstance(), BWAPI::Colors::Cyan},
		{&DefendingState::getInstance(), BWAPI::Colors::Green},
		{&ReinforcingState::getInstance(), BWAPI::Colors::Yellow},
		{&IdleState::getInstance(), BWAPI::Colors::White}
};
unordered_map<BWAPI::Unit, Squad*, unitCMHash> CombatManager::unitSquadMap;
vector<Squad*> CombatManager::AttackingSquads;
vector<Squad*> CombatManager::DefendingSquads;
vector<Squad*> CombatManager::ReinforcingSquads;
vector<Squad*> CombatManager::IdleSquads;

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
	
}

void CombatManager::onStart(){
	AStar::clearPathCache();
	AStar::fillUncachedAreaPairs();
	Squads.clear();
	unitSquadMap.clear();
	AttackingSquads.clear();
	DefendingSquads.clear();
	ReinforcingSquads.clear();
	IdleSquads.clear();
}

void CombatManager::onFrame() {
	if ((BWAPI::Broodwar->getFrameCount() % FRAMES_BETWEEN_CACHING) == 0) {
		AStar::fillAreaPathCache();
	}

	/*for (const auto& squad : Squads) {
		BOIDS::squadFlock(squad);
	}*/

	for (const auto& squad : Squads) {

		// If this squad has an available slot for observer, check if it can get one
		if (squad->observer == nullptr) {
			if (!detachedObservers.empty()) {
				squad->addObserver(detachedObservers.front());
				unitSquadMap[detachedObservers.front()] = squad;
				detachedObservers.pop();
			}
			else {
				BWAPI::Unit observer = commanderReference->scoutingManager.getAvaliableDetectors();
				if (observer != nullptr) {
					squad->observer = observer;
				}
			}
		}

		squad->onFrame();

		if (combat_debug_on) {
			squad->drawSquadBox();
		}
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
	for (const auto& squad : Squads) {
		squad->info.commandPos = position;
		squad->setState(AttackingState::getInstance());
	}
}

void CombatManager::defend(BWAPI::Position position) {
	for (auto& squad : IdleSquads) {
		squad->info.currentDefensivePosition = position;
		squad->setState(DefendingState::getInstance());
	}
	for (auto& squad : AttackingSquads) {
		squad->info.currentDefensivePosition = position;
		squad->setState(DefendingState::getInstance());
	}
}


void CombatManager::reinforce(BWAPI::Position position) {
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
	BWAPI::Unit detector = commanderReference->scoutingManager.getAvaliableDetectors();

	Squads.push_back(newSquad);

	newSquad->setState(IdleState::getInstance());
	if (commanderReference->strategyManager.isAttackPhase) {
		for (const auto& attackingSquad : AttackingSquads) {
			if (attackingSquad->info.currentAttackPosition != BWAPI::Positions::Invalid) {
				newSquad->info.commandPos = globalAttackPosition;
				newSquad->setState(AttackingState::getInstance());
				break;
			}
		}
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

	// Check if observer is left
	if (squad->observer != nullptr) {
		unitSquadMap.erase(squad->observer);
		detachedObservers.push(squad->observer);
	}

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

/// <summary>
/// Checks through available squads. 
/// If squad exists that has space remaining, assigns unit to that squad.
/// If no squad exists OR all squads are full, 
/// </summary>
/// <param name="unit"></param>
/// <returns></returns>
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