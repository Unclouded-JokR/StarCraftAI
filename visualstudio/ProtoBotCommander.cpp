#include "ProtoBotCommander.h"

ProtoBotCommander::ProtoBotCommander() : buildManager(this), strategyManager(this), economyManager(this), scoutingManager(this), combatManager(this), informationManager(this)
{

}

void ProtoBotCommander::onStart()
{
	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/

	// Set our BWAPI options here    
	BWAPI::Broodwar->setLocalSpeed(10);
	BWAPI::Broodwar->setFrameSkip(0);

	// Enable the flag that tells BWAPI to let users enter input while bot plays
	BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);

	// Initialize BWEM with BWAPI's game pointer
	Map::Instance().Initialize();

	// Find the bases for the starting locations
	//bool foundBases = Map::Instance().FindBasesForStartingLocations();
	//assert(foundBases);     // make sure we found the bases

	// Call MapTools OnStart
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

	scoutingManager.onStart();

	buildManager.onStart();
}

void ProtoBotCommander::onFrame()
{
	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/

	// Update our MapTools information
	m_mapTools.onFrame();

	// Draw unit health bars, which brood war unfortunately does not do
	Tools::DrawUnitHealthBars();

	// Draw some relevent information to the screen to help us debug the bot
	drawDebugInformation();

	/*
	* Protobot Modules
	*/
	informationManager.onFrame();

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
			if (!scoutingManager.hasScout()) {
				if (BWAPI::Unit u = getUnitToScout()) {
					scoutingManager.assignScout(u);
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
	Tools::updateCount();

	buildManager.onFrame();

	//Leaving these in a specific order due to cases like building manager possibly needing units.
	economyManager.OnFrame();

	//Uncomment this once onFrame does not steal a worker.
	scoutingManager.onFrame();

	combatManager.onFrame();
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

void ProtoBotCommander::onUnitCreate(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self())
		return;

	buildManager.onCreate(unit);
}

//[TODO] Move this to building manager
bool ProtoBotCommander::checkUnitIsBeingWarpedIn(BWAPI::UnitType building)
{
	return buildManager.checkUnitIsBeingWarpedIn(building);
}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self())
		return;

	buildManager.buildingDoneWarping(unit);

	const BWAPI::UnitType unit_type = unit->getType();

	//If unit is a pylon we dont care about the unit really for now.
	if (unit_type == BWAPI::UnitTypes::Protoss_Pylon) return;

	//We will let the Ecconomy Manager exclusivly deal with all ecconomy units (Nexus, Assimilator, Probe).
	if (unit_type == BWAPI::UnitTypes::Protoss_Nexus || unit_type == BWAPI::UnitTypes::Protoss_Assimilator || unit_type == BWAPI::UnitTypes::Protoss_Probe)
	{
		economyManager.assignUnit(unit);
		return;
	}

	//Give all buildings to the Building Manager.
	if (unit_type.isBuilding() && unit_type != BWAPI::UnitTypes::Protoss_Pylon)
	{
		buildManager.assignBuilding(unit);
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

void ProtoBotCommander::onSendText(std::string text)
{
	if (text == "/map")
	{
		m_mapTools.toggleDraw();
	}
}

void ProtoBotCommander::onUnitMorph(BWAPI::Unit unit)
{

}

void ProtoBotCommander::drawDebugInformation()
{
	std::string currentState = "Current State: " + strategyManager.getCurrentStateName() + "\n";
	if(buildManager.isBuildOrderCompleted())
		All::currentBuild = "Completed";
	std::string buildOrderSelectedString = "Selected Build Order: " + All::currentBuild + "\n";
	
	BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, 10), currentState.c_str());
	BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, 20), buildOrderSelectedString.c_str());
	Tools::DrawUnitCommands();
	Tools::DrawUnitBoundingBoxes();
}

BWAPI::Unit ProtoBotCommander::getUnitToBuild()
{
	//Will not check for null, we expect to get a unit that is able to build. We may also be able to add a command once they return a mineral.
	return economyManager.getAvalibleWorker();
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

//[TODO] change this to to ask the economy manager to get a worker that can scout, getAvalibleWorker() is a method that gets a builder
//You can also change the name of this in order to make it clear what "getAvalibleWorker()" is doing.
//Also get rid of the self get Units, eco should return a unit, not a null ptr.
BWAPI::Unit ProtoBotCommander::getUnitToScout()
{
	if (BWAPI::Unit u = economyManager.getAvalibleWorker())
		return u;

	// Fallback: find any completed, non-carrying worker
	for (auto& u : BWAPI::Broodwar->self()->getUnits()) {
		if (!u->exists()) continue;
		if (u->getType().isWorker() && u->isCompleted() &&
			!u->isCarryingMinerals() && !u->isCarryingGas()) {
			return u;
		}
	}
	return nullptr;
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