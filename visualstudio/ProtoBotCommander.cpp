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
	bool foundBases = Map::Instance().FindBasesForStartingLocations();
	assert(foundBases);     // make sure we found the bases

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
	*
	*/
	std::string enemyRace = enemyRaceCheck();
	std::cout << enemyRace << '\n';

	/*
	* [TO DO]:
	* Initalize ecconomy manager instance.
	* Assign workers.
	* 
	*/
	const BWAPI::Unitset units = BWAPI::Broodwar->self()->getUnits();
	for (BWAPI::Unit unit : units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus)
		{
			//send ecconmy manager signal to create new instance.
		}
	}

	//Assign 4 workers at the start of the game to the ecconomy manager. 
	for(BWAPI::Unit unit : units)
	{
		if (unit->getType().isWorker())
		{
			economyManager.assignUnit(unit);
		}
	}

	/*
	* Protobot Modules
	*/
	informationManager.onStart();
	strategyManager.onStart();
	//scoutingManager.onStart();
	//building manager on start does nothing as of now.
	//buildManager.onStart();
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
	strategyManager.onFrame();
	buildManager.onFrame();

	//Leaving these in a specific order due to cases like building manager possibly needing units.
	economyManager.OnFrame();

	//Uncomment this once onFrame does not steal a worker.
	//scoutingManager.onFrame();
	
	combatManager.Update();
}

void ProtoBotCommander::onEnd(bool isWinner)
{
	std::cout << "We " << (isWinner ? "won!" : "lost!") << "\n";
}

void ProtoBotCommander::onUnitDestroy(BWAPI::Unit unit)
{
	strategyManager.onUnitDestroy(unit);
}

void ProtoBotCommander::onUnitCreate(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self())
		return;

	//Need to add case if the unit is a nexus
	if (unit->getType().isBuilding())
	{
		buildManager.assignBuilding(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe)
	{
		economyManager.assignUnit(unit);
	}
	else
	{
		combatManager.assignUnit(unit);
	}
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
	BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, 10), currentState.c_str());
	Tools::DrawUnitCommands();
	Tools::DrawUnitBoundingBoxes();
}

BWAPI::Unit ProtoBotCommander::getUnitToBuild()
{
	//Will not check for null, we expect to get a unit that is able to build. We may also be able to add a command once they return a mineral.
	return economyManager.getAvalibleWorker();
}

void ProtoBotCommander::getUnitToScout()
{
	if (((BWAPI::Broodwar->getFrameCount() / FRAMES_PER_SECOND) / 60) >= 5)
	{
		//ask combat manager for a unit to scout.
	}
	else
	{
		BWAPI::Unit unit = economyManager.getAvalibleWorker();
	}

	//scoutingManager.assignScout();
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