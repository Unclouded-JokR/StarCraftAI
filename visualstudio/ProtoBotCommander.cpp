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
	* Protobot Modules
	*/
	strategyManager.onStart();
	buildManager.onStart();
	//informationManager.onStart();
	combatManager.onStart();

	//Shouldnt need to check this but will leave this here just in case.
	//playerRaceCheck();
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
	//informationManager.onFrame();
	strategyManager.onFrame();
	buildManager.onFrame();

	//Leaving these in a specific order due to cases like building manager possibly needing units.
	economyManager.OnFrame();

	//Uncomment this once onFrame does not steal a worker.
	//scoutingManager.onFrame();
	
	combatManager.onFrame();
}

void ProtoBotCommander::onEnd(bool isWinner)
{
	std::cout << "We " << (isWinner ? "won!" : "lost!") << "\n";
}

void ProtoBotCommander::onUnitDestroy(BWAPI::Unit unit)
{
	strategyManager.onUnitDestroy(unit);
	combatManager.onUnitDestroy(unit);
}

void ProtoBotCommander::onUnitCreate(BWAPI::Unit unit)
{
}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self())
		return;

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
	BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, 10), "Hello, World!\n");
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

void ProtoBotCommander::testPrint(std::string moduleName)
{
	std::cout << moduleName << std::endl;
}