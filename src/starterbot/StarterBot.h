#pragma once

#include "MapTools.h"
#include "../../visualstudio/StrategyManager.h"
#include "../../visualstudio/BuildManager.h"
#include "../../visualstudio/CombatManager.h"
#include "../../visualstudio/InformationManager.h"
#include "../../visualstudio/ScoutingManager.h"
#include "../../visualstudio/EconomyManager.h"



#include <BWAPI.h>

class StarterBot
{
    MapTools m_mapTools;
	StrategyManager strategyManager;
	CombatManager combatManager;
	BuildManager buildManager;
	InformationManager informationManager;
	ScoutingManager scoutingManager;
	EconomyManager economyManager;

public:

    StarterBot();

    // helper functions to get you started with bot programming and learn the API
    void sendIdleWorkersToMinerals();
    void trainAdditionalWorkers();
    void buildAdditionalSupply();
    void drawDebugInformation();

    // functions that are triggered by various BWAPI events from main.cpp
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

	//Custom Functions
	void playerRaceCheck();

};

