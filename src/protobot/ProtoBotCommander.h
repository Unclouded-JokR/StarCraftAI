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
#define MAX_QUEUE_ITEMS_TO_DRAW 10

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

	int priority = 1;

	//Approved_InProgress only applies to Buildings since this requires a unit to take the time to place it.
	//Add approved killed state. To capture a request that is waiting for a builder.
	enum State { Accepted_Completed, Approved_BeingBuilt, Approved_InProgress, PendingApproval };
	State state = PendingApproval;

	BWAPI::UnitType unit = BWAPI::UnitTypes::None;
	BWAPI::UpgradeType upgrade = BWAPI::UpgradeTypes::None;
	BWAPI::TechType tech = BWAPI::TechTypes::None;

	BWAPI::Unit scoutToPlaceBuilding = nullptr; //Used if a scout requests a gas steal
	bool isCheese = false;

	//Minimum frame to start producing a building by
	int frameToStartBuilding = -1;

	//For now buildings will request to make units but we should remove this later
	//The strategy manager should request certain units and upgrades and the build manager should find open buildings that can trian them.
	BWAPI::Unit requestedBuilding = nullptr;

	//New stuff for Debuging
	int frameRequestCreated = -1;
	int frameRequestApproved = -1;
	int frameRequestServiced = -1;

	//Use this to try requests again and see if we need to kill it.
	int framesSinceLastCheck = 0;
	int attempts = 0;

	// Build order / placement helpers
	bool fromBuildOrder = false;
	bool isWall = false;
	bool isRampPlacement = false;
};

struct ProtoBotRequestCounter {
	//Combat units
	int worker_requests = 0;
	int zealots_requests = 0;
	int dragoons_requests = 0;
	int observers_requests = 0;
	int dark_templars_requests = 0;

	//Buildings
	int gateway_requests = 0;
	int nexus_requests = 0;
	int forge_requests = 0;
	int cybernetics_requests = 0;
	int robotics_requests = 0;
	int observatory_requests = 0;
	int citadel_requests = 0;
	int templarArchives_requests = 0;
	int photon_cannon_requests = 0;

	//Upgrades
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
	ProtoBotRequestCounter requestCounter;

	//Need to have some sort of debug class that can control what we are seeing on our screen for showing information.
	bool drawUnitDebug = true;

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

	//Debug Statements
	void drawDebugInformation();
	void drawBuildingCount(FriendlyBuildingCounter, int x, int y, bool background = true);
	void drawUnitCount(FriendlyUnitCounter, int x, int y, bool background = true);
	void drawUpgradeCount(FriendlyUpgradeCounter, int x, int y, bool background = true);
	void drawBwapiResourceInfo(int x, int y, bool background = true);

	//Resource Requests Methods
	void removeApprovedRequests();
	void requestBuilding(BWAPI::UnitType building, bool fromBuildOrder = false, bool isWall = false, bool isRampPlacement = false);
	void requestUnit(BWAPI::UnitType unit, BWAPI::Unit buildingToTrain, bool fromBuildOrder = false);
	void requestUpgrade(BWAPI::Unit unit, BWAPI::UpgradeType upgrade, bool fromBuildOrder = false);
	void requestCheese(BWAPI::UnitType, BWAPI::Unit);
	void checkBuildingAlreadyHasPlan(BWAPI::Unit); //Might not need this if we already have the minerals to spend for units.
	void updateRequestCounter(BWAPI::UnitType unit);
	void drawResourceRequestQueue(int x, int y, bool background = true);
	void drawResouceRequestCount(int x, int y, bool background = true);

	bool upgradeAlreadyRequested(BWAPI::Unit building);
	bool requestedBuilding(BWAPI::UnitType building);
	bool checkCheeseRequest(BWAPI::Unit);
	bool alreadySentRequest(int unitID);

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
	bool checkWorkerIsConstructing(BWAPI::Unit);

	// Scouting Manager Methods
	BWAPI::Unit getUnitToScout();

	// Strategy Manager Methods
	bool shouldGasSteal();

private:
	EnemyLocations enemy_;
};