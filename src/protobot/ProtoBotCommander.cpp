#include "ProtoBotCommander.h"
#include "ScoutPolicy.h"

int Overall_Wins = 0;
int Overall_Loses = 0;

int versusProtoss_Wins = 0;
int versusProtoss_Loses = 0;
int versusZerg_Wins = 0;
int versusZerg_Loses = 0;
int versusTerran_Wins = 0;
int versusTerran_Loses = 0;


ProtoBotCommander::ProtoBotCommander() : buildManager(this), strategyManager(this), economyManager(this), scoutingManager(this), combatManager(this), informationManager(this)
{

}

#pragma region BWAPI EVENTS
void ProtoBotCommander::onStart()
{
	//std::cout << "============================\n";
	//std::cout << "Initializing Modules\n";

	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/

	// Set our BWAPI options here    
	BWAPI::Broodwar->setLocalSpeed(10);
	BWAPI::Broodwar->setFrameSkip(0);

	// Enable the flag that tells BWAPI to let users enter input while bot plays
	BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);

	static bool mapInitialized = false;

	//std::cout << "Map initialization...\n";

	//theMap = BWEM::Map::Instance();
	theMap.Initialize();
	theMap.EnableAutomaticPathAnalysis();
	bool startingLocationsOK = theMap.FindBasesForStartingLocations();
	assert(startingLocationsOK);

	BWEB::Map::onStart();
	BWEB::Blocks::findBlocks();

	m_mapTools.onStart();

	const BWAPI::Unitset units = BWAPI::Broodwar->self()->getUnits();

	//Get nexus and create a new instace of a NexusEconomy
	//Need to do this because when units are created at the beggining of the game a nexus economy does not exist.
	for (BWAPI::Unit unit : units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus)
		{
			economyManager.assignUnit(unit);
		}
	}

	/*
	* Protobot Modules
	*/
	//informationManager.onStart();
	//strategyManager.onStart();
	//economyManager.onStart();
	//scoutingManager.onStart();
	//buildManager.onStart();

	combatManager.onStart();

	//std::cout << "============================\n";
	//std::cout << "Agent Start\n";
}

void ProtoBotCommander::onFrame()
{
	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/
	//timerManager.startTimer(TimerManager::All);

	// Update our MapTools information
	//timerManager.startTimer(TimerManager::MapTools);
	//m_mapTools.onFrame();
	//timerManager.stopTimer(TimerManager::MapTools);

	/*
	* Protobot Modules
	*/

	//timerManager.startTimer(TimerManager::Information);
	//informationManager.onFrame();
	//timerManager.stopTimer(TimerManager::Information);

	//timerManager.startTimer(TimerManager::Strategy);
	/*std::vector<Action> actions = strategyManager.onFrame();

	bool issuedScoutThisFrame = false;

	for (const Action& action : actions)
	{
		switch (action.type)
		{
		case Action::ACTION_EXPAND:
			requestBuild(action.expansionToConstruct);
			break;
		case Action::ACTION_BUILD:
			requestBuild(action.buildingToConstruct);
			break;
		case Action::ACTION_SCOUT:
			if (!issuedScoutThisFrame)
			{
				BWAPI::Unit scout = getUnitToScout();
				if (scout)
				{
					issuedScoutThisFrame = true;
				}
			}
			break;
		case Action::ACTION_ATTACK:
			combatManager.attack(action.attackPosition);
			break;
		case Action::ACTION_DEFEND:
			combatManager.defend(action.defendPosition);
			break;
		case Action::ACTION_REINFORCE:
			combatManager.reinforce(action.reinforcePosition);
			break;
		}
	}*/
	//timerManager.stopTimer(TimerManager::Strategy);

	//timerManager.startTimer(TimerManager::Build);
	//buildManager.onFrame();
	//timerManager.stopTimer(TimerManager::Build);

	//Leaving these in a specific order due to cases like building manager possibly needing units.
	//timerManager.startTimer(TimerManager::Economy);
	//economyManager.onFrame();
	//timerManager.stopTimer(TimerManager::Economy);

	//Uncomment this once onFrame does not steal a worker.
	//timerManager.startTimer(TimerManager::Scouting);
	//scoutingManager.onFrame();
	//timerManager.stopTimer(TimerManager::Scouting);

	//timerManager.startTimer(TimerManager::Combat);
	combatManager.onFrame();
	//timerManager.stopTimer(TimerManager::Combat);

	//timerManager.stopTimer(TimerManager::All);

	// Draw unit health bars, which brood war unfortunately does not do
	//Tools::DrawUnitHealthBars();

	// Draw some relevent information to the screen to help us debug the bot
	//drawDebugInformation();
}

void ProtoBotCommander::onEnd(bool isWinner)
{
	//std::cout << "We " << (isWinner ? "won!" : "lost!") << "\n";
	//(isWinner ? Overall_Wins++ : Overall_Loses++);

	/*
	switch(BWAPI::Broodwar->enemy()->getRace())
	{
		case BWAPI::Races::Protoss: (isWinner ? versusProtoss_Wins++ : versusProtoss_Loses++); break;
		case BWAPI::Races::Zerg: (isWinner ? versusZerg_Wins++ : versusZerg_Loses++); break;
		case BWAPI::Races::Terran:(isWinner ? versusTerran_Wins++ : versusTerran_Loses++); break;
	}
	*/
	//const float totalGames = float(Overall_Wins + Overall_Loses);

	//std::cout << "Win Percentage: " << float(Overall_Wins) / totalGames << "\n";
	//std::cout << "Wins: " << Overall_Wins << "\n";
	//std::cout << "Loses: " << Overall_Loses << "\n";

	/*std::cout << "Versus Protoss Wins: " << versusProtoss_Wins << "\n";
	std::cout << "Versus Protoss Loses: " << versusProtoss_Loses << "\n";
	std::cout << "Versus Zerg Wins: " << versusZerg_Wins << "\n";
	std::cout << "Versus Zeg Loses: " << versusZerg_Loses << "\n";
	std::cout << "Versus Terran Wins: " << versusTerran_Wins << "\n";
	std::cout << "Versus Terran Loses: " << versusTerran_Loses << "\n";*/


	// Important: Clear all caches BWEB pointers upon game end, otherwise invalid = crash
	BWEB::Map::onEnd();
}

/*
* When a unit is destroyed send a broadcast out to all modules.
*
* Each module will check if the unit is a assigned to their module. If the unit is, remove the unit. Others drop the message.
*/
void ProtoBotCommander::onUnitDestroy(BWAPI::Unit unit)
{
	//Managers that deal with unit assignments
	economyManager.onUnitDestroy(unit);
	combatManager.onUnitDestroy(unit);
	scoutingManager.onUnitDestroy(unit);

	//Managers that deal with unit information updates
	strategyManager.onUnitDestroy(unit);
	informationManager.onUnitDestroy(unit);
	buildManager.onUnitDestroy(unit);
}

void ProtoBotCommander::onUnitMorph(BWAPI::Unit unit)
{
	informationManager.onUnitMorph(unit);
	buildManager.onUnitMorph(unit);
}

void ProtoBotCommander::onSendText(std::string text)
{
	if (text == "/map")
	{
		m_mapTools.toggleDraw();
	}

	combatManager.handleTextCommand(text);
}

void ProtoBotCommander::onUnitCreate(BWAPI::Unit unit)
{
	buildManager.onUnitCreate(unit);
	informationManager.onUnitCreate(unit);
	strategyManager.onUnitCreate(unit);
}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{
	//std::cout << "ProtoBot onUnitComplete: " << unit->getType() << "\n";

	strategyManager.onUnitComplete(unit);
	informationManager.onUnitComplete(unit);

	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	const BWAPI::UnitType unit_type = unit->getType();

	//We will let the Ecconomy Manager exclusivly deal with all ecconomy units (Nexus, Assimilator, Probe).
	if (unit_type == BWAPI::UnitTypes::Protoss_Nexus || unit_type == BWAPI::UnitTypes::Protoss_Assimilator || unit_type == BWAPI::UnitTypes::Protoss_Probe)
	{
		//std::cout << "Calling onComplete for " << unit_type << " " << unit->getID() << "\n";
		economyManager.assignUnit(unit);
		buildManager.onUnitComplete(unit);
		return;
	}

	if (unit_type.isBuilding())
	{
		//std::cout << "Calling onComplete for " << unit_type << " " << unit->getID() << "\n";
		buildManager.onUnitComplete(unit);
		return;
	}

	if (unit_type == BWAPI::UnitTypes::Protoss_Observer)
	{
		if (scoutingManager.canAcceptObserverScout())
		{
			scoutingManager.assignScout(unit);
			//BWAPI::Broodwar->printf("[Commander] Assigned Observer %d to Scouting", unit->getID());
			return;
		}
	}

	/*
	if (unit_type == BWAPI::UnitTypes::Protoss_Zealot || unit_type == BWAPI::UnitTypes::Protoss_Dragoon)
	{
		if (scoutingManager.canAcceptCombatScout(unit_type))
		{
			scoutingManager.assignScout(unit);
			BWAPI::Broodwar->printf("[Commander] Assigned %s %d to Scouting", unit_type.c_str(), unit->getID());
			return;
		}
	}*/

	if (unit_type == BWAPI::UnitTypes::Protoss_Observer)
	{
		if (scoutingManager.canAcceptObserverScout())
		{
			scoutingManager.assignScout(unit);
			//BWAPI::Broodwar->printf("[Commander] Assigned %s %d to Scouting", unit_type.c_str(), unit->getID());
			return;
		}
	}

	//Gone through all cases assume it is a combat unit 
	combatManager.assignUnit(unit);
}

void ProtoBotCommander::onUnitShow(BWAPI::Unit unit)
{
	buildManager.onUnitDiscover(unit);
}

void ProtoBotCommander::onUnitHide(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitRenegade(BWAPI::Unit unit)
{

}

void ProtoBotCommander::drawDebugInformation()
{
	// Display the game frame rate as text in the upper left area of the screen
	BWAPI::Broodwar->drawTextScreen(0, 20, "FPS: %d", BWAPI::Broodwar->getFPS());
	BWAPI::Broodwar->drawTextScreen(0, 30, "Average FPS: %f", BWAPI::Broodwar->getAverageFPS());

	/*BWAPI::Broodwar->drawTextScreen(0, 40, "Elapsed Time (Real time): %02d:", BWAPI::Broodwar->elapsedTime() / 60);
	BWAPI::Broodwar->drawTextScreen(142, 40, "%02d", BWAPI::Broodwar->elapsedTime() % 60);*/

	Tools::DrawUnitCommands();
	Tools::DrawUnitBoundingBoxes();

	timerManager.displayTimers(490, 225);
}
#pragma endregion

bool ProtoBotCommander::checkUnitIsBeingWarpedIn(BWAPI::UnitType building)
{
	return buildManager.checkUnitIsBeingWarpedIn(building);
}

BWAPI::Unit ProtoBotCommander::getUnitToBuild(BWAPI::Position buildLocation)
{
	//Will not check for null, we expect to get a unit that is able to build. We may also be able to add a command once they return a mineral.
	return economyManager.getAvalibleWorker(buildLocation);
}

void ProtoBotCommander::requestBuild(BWAPI::UnitType building)
{
	buildManager.buildBuilding(building);
}

void ProtoBotCommander::requestUnitToTrain(BWAPI::UnitType worker, BWAPI::Unit building)
{
	buildManager.trainUnit(worker, building);
}

const std::set<BWAPI::Unit>& ProtoBotCommander::getKnownEnemyUnits()
{
	return informationManager.getKnownEnemies();
}

const std::map<BWAPI::Unit, EnemyBuildingInfo>& ProtoBotCommander::getKnownEnemyBuildings()
{
	return informationManager.getKnownEnemyBuildings();
}

void ProtoBotCommander::requestCheese(BWAPI::UnitType building, BWAPI::Unit unit)
{
	buildManager.buildBuilding(building, unit);
}

bool ProtoBotCommander::checkCheeseRequest(BWAPI::Unit unit)
{
	return buildManager.cheeseIsApproved(unit);
}

bool ProtoBotCommander::buildOrderCompleted()
{
	return buildManager.isBuildOrderCompleted();
}

bool ProtoBotCommander::requestedBuilding(BWAPI::UnitType building)
{
	return buildManager.requestedBuilding(building);
}

bool ProtoBotCommander::alreadySentRequest(int unitID)
{
	return buildManager.alreadySentRequest(unitID);
}

bool ProtoBotCommander::checkUnitIsPlanned(BWAPI::UnitType building)
{
	return buildManager.checkUnitIsPlanned(building);
}

std::vector<NexusEconomy> ProtoBotCommander::getNexusEconomies()
{
	return economyManager.getNexusEconomies();
}

bool ProtoBotCommander::checkWorkerIsConstructing(BWAPI::Unit unit)
{
	return buildManager.checkWorkerIsConstructing(unit);
}

int ProtoBotCommander::checkAvailableSupply()
{
	return buildManager.checkAvailableSupply();
}

//[TODO] change this to to ask the economy manager to get a worker that can scout, getAvalibleWorker() is a method that gets a builder
//You can also change the name of this in order to make it clear what "getAvalibleWorker()" is doing.
//Also get rid of the self get Units, eco should return a unit, not a null ptr.
BWAPI::Unit ProtoBotCommander::getUnitToScout()
{
	auto isValidUnit = [](BWAPI::Unit u)
		{
			return u && u->exists() && u->getPlayer() == BWAPI::Broodwar->self();
		};

	const int frame = BWAPI::Broodwar->getFrameCount();

	if (frame >= kCombatScoutFrame && !scoutingManager.combatScoutingStarted())
	{
		scoutingManager.setCombatScoutingStarted(true);
	}

	// Before combat scouting: keep exactly one worker scout
	if (!scoutingManager.combatScoutingStarted())
	{
		if (scoutingManager.canAcceptWorkerScout())
		{
			BWAPI::Unit w = economyManager.getUnitScout();
			if (isValidUnit(w))
			{
				scoutingManager.assignScout(w);
				return w;
			}
		}

		return nullptr;
	}

	// After combat scouting starts, assign at most one scout per call.

	if (scoutingManager.canAcceptCombatScout(BWAPI::UnitTypes::Protoss_Zealot))
	{
		BWAPI::Unit u = combatManager.getAvailableUnit(
			[](BWAPI::Unit x)
			{
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Zealot;
			}
		);

		if (isValidUnit(u))
		{
			scoutingManager.assignScout(u);
			return u;
		}
	}

	if (scoutingManager.canAcceptCombatScout(BWAPI::UnitTypes::Protoss_Dragoon))
	{
		BWAPI::Unit u = combatManager.getAvailableUnit(
			[](BWAPI::Unit x)
			{
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Dragoon;
			}
		);

		if (isValidUnit(u))
		{
			scoutingManager.assignScout(u);
			return u;
		}
	}

	if (scoutingManager.canAcceptObserverScout())
	{
		BWAPI::Unit u = combatManager.getAvailableUnit(
			[](BWAPI::Unit x)
			{
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Observer;
			}
		);

		if (isValidUnit(u))
		{
			scoutingManager.assignScout(u);
			return u;
		}
	}

	return nullptr;
}

// ------- Enemy Locations ----------
void ProtoBotCommander::onEnemyMainFound(const BWAPI::TilePosition& tp) {
	enemy_.main = tp;
	enemy_.frameLastUpdateMain = BWAPI::Broodwar->getFrameCount();
	//BWAPI::Broodwar->printf("[Commander] Enemy main set to (%d,%d)", tp.x, tp.y);

	// StrategyManager.onEnemyMain(tp);
}

void ProtoBotCommander::onEnemyNaturalFound(const BWAPI::TilePosition& tp) {
	enemy_.natural = tp;
	enemy_.frameLastUpdateNat = BWAPI::Broodwar->getFrameCount();
	//BWAPI::Broodwar->printf("[Commander] Enemy natural set to (%d,%d)", tp.x, tp.y);

	// StrategyManager.onEnemyNaturalFound(tp)
}

int ProtoBotCommander::getEnemyGroundThreatAt(BWAPI::Position p) const {
	return informationManager.getEnemyGroundThreatAt(p);
}

int ProtoBotCommander::getEnemyDetectionAt(BWAPI::Position p) const {
	return informationManager.getEnemyDetectionAt(p);
}

ThreatQueryResult ProtoBotCommander::queryThreatAt(const BWAPI::Position& pos) const
{
	return informationManager.queryThreatAt(pos);
}

bool ProtoBotCommander::isAirThreatened(const BWAPI::Position& pos, int threshold) const
{
	const auto r = queryThreatAt(pos);
	return r.airThreat >= threshold;
}

bool ProtoBotCommander::isDetectorThreatened(const BWAPI::Position& pos) const
{
	const auto r = queryThreatAt(pos);
	return r.detectorThreat > 0;
}

bool ProtoBotCommander::shouldGasSteal()
{
	bool const enable = strategyManager.shouldGasSteal();

	return enable;
}