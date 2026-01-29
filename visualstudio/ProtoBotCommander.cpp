#include "ProtoBotCommander.h"
#include "ScoutPolicy.h"

ProtoBotCommander::ProtoBotCommander() : buildManager(this), strategyManager(this), economyManager(this), scoutingManager(this), combatManager(this), informationManager(this)
{

}

#pragma region BWAPI EVENTS
void ProtoBotCommander::onStart()
{
	std::cout << "============================\n";
	std::cout << "Initializing Modules\n";

	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/

	// Set our BWAPI options here    
	BWAPI::Broodwar->setLocalSpeed(10);
	BWAPI::Broodwar->setFrameSkip(0);

	// Enable the flag that tells BWAPI to let users enter input while bot plays
	BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);

	static bool mapInitialized = false;

	std::cout << "Map initialization...\n";

	//theMap = BWEM::Map::Instance();
	theMap.Initialize();
	theMap.EnableAutomaticPathAnalysis();
	bool startingLocationsOK = theMap.FindBasesForStartingLocations();
	assert(startingLocationsOK);

	BWEB::Map::onStart();
	BWEB::Blocks::findBlocks();

	m_mapTools.onStart();


	/*
	* Use this to query the BuildManager to randomly select a build order based on the enemy race.
	* We can change the return for this method to be a int to avoid an interupt to OS if std::string causes that.
	*
	* 100 for Protoss, 200 for Terran, 300 for Zerg
	*
	* the next two digits would be the id of the build order we would randomly select.
	* 0 - 99 (max would be 99 but there is no way we would need that many)
	*
	* We could reduce these digits to tens instead to decrease memeory usage.
	*/

	/*
	* [TO DO]:
	* Create code to select opening randomly for avalible openings.
	* Have functions that can ask building manager how many openings we have.
	*/
	std::string enemyRace = enemyRaceCheck();
	std::cout << "Enemy Race " << enemyRace << '\n';

	//[TODO] Need build order structure to be implemented.
	//vector<BuildOrder> build_orders = buildManager.getBuildOrders(enemyRace);

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
	informationManager.onStart();

	//Replace with commented out line when multiple build orders are in place.
	buildOrderSelected = strategyManager.onStart();

	//buildOrder buildOrderSelection = strategyManager.onStart(build_orders);

	economyManager.onStart();

	combatManager.onStart();

	scoutingManager.onStart();

	buildManager.onStart();

	std::cout << "============================\n";
	std::cout << "Agent Start\n";
}

void ProtoBotCommander::onFrame()
{
	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/
	timerManager.startTimer(TimerManager::All);

	// Update our MapTools information
	timerManager.startTimer(TimerManager::MapTools);
	m_mapTools.onFrame();
	timerManager.stopTimer(TimerManager::MapTools);

	/*for (const Area& area : theMap.Areas())
	{
		for (const Base& base : area.Bases())
		{
			if (BWEM::utils::MapDrawer::showBases && BWAPI::Broodwar->canBuildHere(base.Location(), BWAPI::UnitTypes::Protoss_Nexus))
			{
				BWAPI::Broodwar->drawBoxMap(BWAPI::Position(base.Location()),
					BWAPI::Position(base.Location() + BWAPI::UnitType(BWAPI::UnitTypes::Protoss_Nexus).tileSize()),
					BWEM::utils::MapDrawer::Color::bases);
			}
		}
	}*/

	/*
	* Protobot Modules
	*/
	timerManager.startTimer(TimerManager::Information);
	informationManager.onFrame();
	timerManager.stopTimer(TimerManager::Information);

	timerManager.startTimer(TimerManager::Strategy);
	Action action = strategyManager.onFrame();
	//std::cout << action.type << "\n";

	switch (action.type)
	{
		case ActionType::Action_Expand:
		{
			const Expand value = get<Expand>(action.commanderAction);
			requestBuild(value.unitToBuild);
			break;
		}
		case ActionType::Action_Build:
		{
			const Build value = get<Build>(action.commanderAction);
			requestBuild(value.unitToBuild);
			break;
		}
		case ActionType::Action_Scout:
		{
			std::cout << "Reuqesting scout!\n";
			if (!scoutingManager.hasScout())
			{
				if (BWAPI::Unit u = getUnitToScout()) {
					scoutingManager.assignScout(u);
					std::cout << "Got unit to scout!\n";
				}
			}
			break;
		}
		case ActionType::Action_Attack:
		{
			break;
		}
		case ActionType::Action_Defend:
		{
			break;
		}
		default:
		{
			break;
		}
	}
	timerManager.stopTimer(TimerManager::Strategy);

	//Get rid of this line since this should be information.
	Tools::updateCount();

	timerManager.startTimer(TimerManager::Build);
	buildManager.onFrame();
	timerManager.stopTimer(TimerManager::Build);

	//Leaving these in a specific order due to cases like building manager possibly needing units.
	timerManager.startTimer(TimerManager::Economy);
	economyManager.onFrame();
	timerManager.stopTimer(TimerManager::Economy);

	//Uncomment this once onFrame does not steal a worker.
	timerManager.startTimer(TimerManager::Scouting);
	scoutingManager.onFrame();
	timerManager.stopTimer(TimerManager::Scouting);

	timerManager.startTimer(TimerManager::Combat);
	combatManager.onFrame();
	timerManager.stopTimer(TimerManager::Combat);

	timerManager.stopTimer(TimerManager::All);

	// Draw unit health bars, which brood war unfortunately does not do
	Tools::DrawUnitHealthBars();

	BWEB::Map::draw();

	// Draw some relevent information to the screen to help us debug the bot
	drawDebugInformation();
}

void ProtoBotCommander::onEnd(bool isWinner)
{
	std::cout << "We " << (isWinner ? "won!" : "lost!") << "\n";
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

void ProtoBotCommander::onUnitDiscover(BWAPI::Unit unit)
{
	buildManager.onUnitDiscover(unit);

	//add information manager here.
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
}

void ProtoBotCommander::onUnitCreate(BWAPI::Unit unit)
{
	buildManager.onUnitCreate(unit);
	informationManager.onUnitCreate(unit);
}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{
	//Need to call on create again for the case of an assimilator not CREATING a new "unit"
	buildManager.onUnitCreate(unit);
	informationManager.onUnitComplete(unit);

	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	buildManager.buildingDoneWarping(unit);

	const BWAPI::UnitType unit_type = unit->getType();

	//We will let the Ecconomy Manager exclusivly deal with all ecconomy units (Nexus, Assimilator, Probe).
	if (unit_type == BWAPI::UnitTypes::Protoss_Nexus || unit_type == BWAPI::UnitTypes::Protoss_Assimilator || unit_type == BWAPI::UnitTypes::Protoss_Probe)
	{
		economyManager.assignUnit(unit);
		return;
	}

	if (unit_type.isBuilding())
	{
		buildManager.onUnitComplete(unit);
		return;
	}

	//Gone through all cases assume it is a combat unit 
	combatManager.assignUnit(unit);
}

void ProtoBotCommander::onUnitShow(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitHide(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitRenegade(BWAPI::Unit unit)
{

}

void ProtoBotCommander::drawDebugInformation()
{
	std::string currentState = "Current State: " + strategyManager.getCurrentStateName() + "\n";
	if (buildManager.isBuildOrderCompleted())
		All::currentBuild = "Completed";
	std::string buildOrderSelectedString = "Selected Build Order: " + All::currentBuild + "\n";

	BWAPI::Broodwar->drawTextScreen(0, 0, currentState.c_str());
	BWAPI::Broodwar->drawTextScreen(0, 10, buildOrderSelectedString.c_str());

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

bool ProtoBotCommander::buildOrderCompleted()
{
	return buildManager.isBuildOrderCompleted();
}

bool ProtoBotCommander::requestedBuilding(BWAPI::UnitType building)
{
	return buildManager.requestedBuilding(building);
}

std::string ProtoBotCommander::enemyRaceCheck()
{
	BWAPI::Race enemyRace = BWAPI::Broodwar->enemy()->getRace();

	if (enemyRace == BWAPI::Races::Protoss)
	{
		return "Protoss_";
	}
	else if (enemyRace == BWAPI::Races::Terran)
	{
		return "Terran_";
	}
	else
	{
		return "Zerg_";
	}
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
	auto isValidUnit = [](BWAPI::Unit u) {
		return u && u->exists() && u->getPlayer() == BWAPI::Broodwar->self();
		};

	const int frame = BWAPI::Broodwar->getFrameCount();

	// Switch to combat scouting at/after the threshold exactly once
	if (frame >= kCombatScoutFrame && !scoutingManager.combatScoutingStarted()) {
		scoutingManager.setCombatScoutingStarted(true);
	}

	// 1) Before combat-scouting: keep exactly 1 worker scout
	if (!scoutingManager.combatScoutingStarted()) {
		if (scoutingManager.canAcceptWorkerScout()) {
			if (BWAPI::Unit w = economyManager.getUnitScout(); isValidUnit(w)) {
				scoutingManager.assignScout(w);
				std::cout << "Assigned worker scout " << w->getID() << "\n";
			}
		}
		return nullptr;
	}

	// 2) After combat-scouting starts: never request worker scouts again.
	// Fill up to max combat scouts using combat units.
	while (scoutingManager.canAcceptCombatScout(BWAPI::UnitTypes::Protoss_Zealot))
	{
		BWAPI::Unit u = combatManager.getAvailableUnit(
			[](BWAPI::Unit x) {
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Zealot;
			}
		);
		if (!isValidUnit(u)) break;
		scoutingManager.assignScout(u);
		std::cout << "Assigned combat scout (Zealot) " << u->getID() << "\n";
	}

	while (scoutingManager.canAcceptCombatScout(BWAPI::UnitTypes::Protoss_Dragoon))
	{
		BWAPI::Unit u = combatManager.getAvailableUnit(
			[](BWAPI::Unit x) {
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Dragoon;
			}
		);
		if (!isValidUnit(u)) break;
		scoutingManager.assignScout(u);
		std::cout << "Assigned combat scout (Dragoon) " << u->getID() << "\n";
	}

	while (scoutingManager.canAcceptObserverScout())
	{
		BWAPI::Unit u = combatManager.getAvailableUnit
		(
			[](BWAPI::Unit x)
			{
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Observer;
			}
		);
		if (!u) break;
		scoutingManager.assignScout(u);
		std::cout << "Assigned observer scout " << u->getID() << "\n";
	}

	return nullptr;
}

// ------- Enemy Locations ----------
void ProtoBotCommander::onEnemyMainFound(const BWAPI::TilePosition& tp) {
	enemy_.main = tp;
	enemy_.frameLastUpdateMain = BWAPI::Broodwar->getFrameCount();
	BWAPI::Broodwar->printf("[Commander] Enemy main set to (%d,%d)", tp.x, tp.y);

	// StrategyManager.onEnemyMain(tp);
}

void ProtoBotCommander::onEnemyNaturalFound(const BWAPI::TilePosition& tp) {
	enemy_.natural = tp;
	enemy_.frameLastUpdateNat = BWAPI::Broodwar->getFrameCount();
	BWAPI::Broodwar->printf("[Commander] Enemy natural set to (%d,%d)", tp.x, tp.y);

	// StrategyManager.onEnemyNaturalFound(tp)
}