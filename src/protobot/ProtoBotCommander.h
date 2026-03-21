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
#include "../starterbot/MapTools.h"
#include "../starterbot/Tools.h"
#include "BWEM/src/bwem.h"

#define FRAMES_PER_SECOND 24

using namespace BWEM;

namespace
{
	auto& theMap = BWEM::Map::Instance();

	// 2:15 in frames
	constexpr int kCombatScoutFrame = 3240;
}

struct ThreatQueryResult;

struct EnemyLocations {
	std::optional<BWAPI::TilePosition> main;
	std::optional<BWAPI::TilePosition> natural;
	int frameLastUpdateMain = -1;
	int frameLastUpdateNat = -1;
};

struct ResourceRequest
{
	enum Type { Unit, Building, Upgrade, Tech };
	Type type;

	//Approved_InProgress only applies to Buildings since this requires a unit to take the time to place it.
	//Add approved killed state. To capture a request that is waiting for a builder.
	enum State { Accepted_Completed, Approved_BeingBuilt, Approved_InProgress, PendingApproval };
	State state = PendingApproval;

	BWAPI::UnitType unit = BWAPI::UnitTypes::None;
	BWAPI::UpgradeType upgrade = BWAPI::UpgradeTypes::None;
	BWAPI::TechType tech = BWAPI::TechTypes::None;

	BWAPI::Unit scoutToPlaceBuilding = nullptr; //Used if a scout requests a gas steal
	bool isCheese = false;

	//Use this to try requests again and see if we need to kill it.
	int framesSinceLastCheck = 0;
	int attempts = 0;
	// Build order / placement helpers
	bool fromBuildOrder = false;
	BWAPI::TilePosition forcedTile = BWAPI::TilePositions::Invalid;
	bool useForcedTile = false;

	int priority;
};

class ProtoBotCommander
{

public:
	MapTools m_mapTools;
	TimerManager timerManager;
	EconomyManager economyManager;
	ScoutingManager scoutingManager;
	BuildManager buildManager;
	CombatManager combatManager;
	StrategyManager strategyManager;

	std::vector<ResourceRequest> resourceRequests;

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

	void removeApprovedRequests();

	//Ecconomy Manager Methods
	BWAPI::Unit getUnitToBuild(BWAPI::Position buildLocation);
	std::vector<NexusEconomy> getNexusEconomies();

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
	void requestCheese(BWAPI::UnitType, BWAPI::Unit);
	bool checkCheeseRequest(BWAPI::Unit);
	bool alreadySentRequest(int unitID);
	bool checkWorkerIsConstructing(BWAPI::Unit);
	int checkAvailableSupply();

	// Scouting Manager Methods
	BWAPI::Unit getUnitToScout();

	// Strategy Manager Methods
	bool shouldGasSteal();

private:
	EnemyLocations enemy_;
};