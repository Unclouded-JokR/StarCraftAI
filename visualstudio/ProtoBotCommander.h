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
};

