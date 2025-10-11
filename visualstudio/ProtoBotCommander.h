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
public:
	EconomyManager economyManager;
	InformationManager informationManager;
	ScoutingManager scoutingManager;
	BuildManager buildManager;
	//Change this to be a class instead of namespace
	//CombatManager combatManager; 
	StrategyManager strategyManager;

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
	void requestUnit(BWAPI::Unit unit);
	void requestUnit(BWAPI::Unitset &unit);
	void requestUnit(BWAPI::UnitType type);

private:
	UnitManager unitManager;
	RequestManager requestManager;
	//Add spenderManager?
};

