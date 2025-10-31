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
#include <vector> 
#include <cstdlib>
#include <variant>
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
	std::string enemyRaceCheck();

	//Ecconomy Manager Methods
	BWAPI::Unit getUnitToBuild();
	//BWAPI::Unitset getAllUnitsAssignedToNexus();
	
	//Information Manager Methods
	const std::set<BWAPI::Unit>& getKnownEnemyUnits();
	const std::map<BWAPI::Unit, EnemyBuildingInfo>& getKnownEnemyBuildings();

	//Build Manager Methods
	bool buildOrderCompleted();

	void getUnitToScout();
};

enum ActionType {
	Action_Expand,
	Action_Scout,
	Action_Build,
	Action_Attack,
	Action_Defend,
	Action_None
};

struct Expand 
{
	BWAPI::UnitType unitToBuild;
};

struct Scout {

};

struct Build
{
	BWAPI::UnitType unitToBuild;
};

struct Attack
{
	BWAPI::TilePosition position;
};

struct Defend
{
	BWAPI::TilePosition position;
};

struct None
{

};

struct Action {
	std::variant<Expand, Scout, Build, Attack, Defend, None> commanderAction;
	ActionType type;
};