#pragma once
#include "StrategyManager.h"
#include "EconomyManager.h"
#include "InformationManager.h"
#include "ScoutingManager.h"
#include "BuildManager.h"
#include "CombatManager.h"
#include "SpenderManager.h"
#include "../../src/starterbot/MapTools.h"
#include "../../src/starterbot/Tools.h"
#include <BWAPI.h>
#include <vector> 
#include <cstdlib>
#include <variant>
#include "../../BWEM/src/bwem.h"

#define FRAMES_PER_SECOND 24

using namespace BWEM;

struct EnemyLocations {
	std::optional<BWAPI::TilePosition> main;
	std::optional<BWAPI::TilePosition> natural;
	int frameLastUpdateMain = -1;
	int frameLastUpdateNat = -1;
};

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

	/*
	* Used for Debugging
	*/
	std::string buildOrderSelected;

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
	bool checkUnitIsBeingWarpedIn(BWAPI::UnitType type);
	bool checkUnitIsPlanned(BWAPI::UnitType building);

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
	const EnemyLocations& enemy() const { return enemy_; }
	EnemyLocations& enemy() { return enemy_; }
	void onEnemyMainFound(const BWAPI::TilePosition& tp);
	void onEnemyNaturalFound(const BWAPI::TilePosition& tp);

	//Build Manager Methods
	bool buildOrderCompleted();
	bool requestedBuilding(BWAPI::UnitType building);
	void requestUnitToTrain(BWAPI::UnitType worker, BWAPI::Unit building);
	void requestBuild(BWAPI::UnitType building);
	bool alreadySentRequest(int unitID);

	//Scouting
	BWAPI::Unit getUnitToScout();

private:
	EnemyLocations enemy_;
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