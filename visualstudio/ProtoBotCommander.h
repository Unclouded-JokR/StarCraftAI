#pragma once
#include <BWAPI.h>
#include <vector> 
#include <cstdlib>
#include <variant>

#include "StrategyManager.h"
#include "EconomyManager.h"
#include "InformationManager.h"
#include "ScoutingManager.h"
#include "CombatManager.h"
#include "TimerManager.h"
#include "BuildManager.h"
#include "../../src/starterbot/MapTools.h"
#include "../../src/starterbot/Tools.h"
#include "../../BWEM/src/bwem.h"

#define FRAMES_PER_SECOND 24

using namespace BWEM;

namespace
{
	auto& theMap = BWEM::Map::Instance();

	// 2:15 in frames
	constexpr int kCombatScoutFrame = 3240;
}

struct EnemyLocations {
	std::optional<BWAPI::TilePosition> main;
	std::optional<BWAPI::TilePosition> natural;
	int frameLastUpdateMain = -1;
	int frameLastUpdateNat = -1;
};

struct ThreatQueryResult;

class ProtoBotCommander
{
public:
	MapTools m_mapTools;
	TimerManager timerManager;
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
	void onUnitDiscover(BWAPI::Unit unit);
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
	BWAPI::Unit getUnitToBuild(BWAPI::Position buildLocation);
	std::vector<NexusEconomy> getNexusEconomies();
	//BWAPI::Unitset getAllUnitsAssignedToNexus();

	//Information Manager Methods
	const std::set<BWAPI::Unit>& getKnownEnemyUnits();
	const std::map<BWAPI::Unit, EnemyBuildingInfo>& getKnownEnemyBuildings();
	const EnemyLocations& enemy() const { return enemy_; }
	EnemyLocations& enemy() { return enemy_; }
	void onEnemyMainFound(const BWAPI::TilePosition& tp);
	void onEnemyNaturalFound(const BWAPI::TilePosition& tp);
	int getEnemyGroundThreatAt(BWAPI::Position p) const;
	int getEnemyDetectionAt(BWAPI::Position p) const;
	ThreatQueryResult queryThreatAt(const BWAPI::Position& pos) const;
	bool isAirThreatened(const BWAPI::Position& pos, int threshold) const;
	bool isDetectorThreatened(const BWAPI::Position& pos) const;

	//Build Manager Methods
	bool checkUnitIsBeingWarpedIn(BWAPI::UnitType type);
	bool checkUnitIsPlanned(BWAPI::UnitType building);
	bool buildOrderCompleted();
	bool requestedBuilding(BWAPI::UnitType building);
	void requestUnitToTrain(BWAPI::UnitType worker, BWAPI::Unit building);
	void requestBuild(BWAPI::UnitType building);
	bool alreadySentRequest(int unitID);
	bool checkWorkerIsConstructing(BWAPI::Unit);
	int checkAvailableSupply();

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