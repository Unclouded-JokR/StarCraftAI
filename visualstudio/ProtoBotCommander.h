#pragma once
#include "StrategyManager.h"
#include "EconomyManager.h"
#include "InformationManager.h"
#include "ScoutingManager.h"
#include "BuildManager.h"
#include "CombatManager.h"
#include "UnitManager.h"
#include "RequestManager.h"
#include <BWAPI.h>

class ProtoBotCommander
{
private:
	UnitManager unitManager;
	RequestManager requestManager;
	//Add spenderManager?

public:
	EconomyManager economyManager;
	InformationManager informationManager;
	ScoutingManager scoutingManager;
	BuildManager buildManager;
	CombatManager combatManager; 
	StrategyManager strategyManager;

	//Standard bot methods
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

	//Dont know the right implementation for this but will have these methods for now
	BWAPI::Unit requestUnit(BWAPI::UnitType type);
	BWAPI::Unitset& requestUnits(BWAPI::UnitType type, int numUnits);
};

ProtoBotCommander commander;
