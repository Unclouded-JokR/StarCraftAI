#pragma once
#include "StrategyManager.h"
#include "EconomyManager.h"
#include "InformationManager.h"
#include "ScoutingManager.h"
#include "BuildManager.h"
#include "CombatManager.h"
#include "UnitManager.h"
#include "RequestManager.h"
#include "../../src/starterbot/MapTools.h"
#include "../../src/starterbot/Tools.h"
#include <BWAPI.h>
#include "../../BWEM/src/bwem.h"

#define FRAMES_PER_SECOND 24

using namespace BWEM;

class ProtoBotCommander
{
public:
	MapTools m_mapTools;
	EconomyManager economyManager;
	InformationManager informationManager;
	ScoutingManager scoutingManager;
	BuildManager buildManager;
	CombatManager combatManager; 
	StrategyManager strategyManager;

	ProtoBotCommander();

	/*
	* BWAPI specific methods
	*/
	void onStart();
	void onFrame();
	void onEnd(bool isWinner);
	void onUnitDestroy(BWAPI::Unit unit);
	void onUnitMorph(BWAPI::Unit unit);
	void onSendText(std::string text);
	void onUnitCreate(BWAPI::Unit unit);
	void onUnitComplete(BWAPI::Unit unit);
	void onUnitShow(BWAPI::Unit unit);
	void onUnitHide(BWAPI::Unit unit);
	void onUnitRenegade(BWAPI::Unit unit);
	void drawDebugInformation();

	/*
	* Methods for modules to communicate, Will also need unit set versions of these methods as well.
	*/
	void getUnitToScout();
	BWAPI::Unit getUnitToBuild();
};

