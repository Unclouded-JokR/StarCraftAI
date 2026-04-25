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

/// <summary>
/// A ResourceRequest is a way to be able to keep an detailed decsription of what ProtoBot is wanting to make and is currently making.\n
/// A resource request goes through 4 states and changes based on the type of ResourceRequest we are dealing with.\n
/// The creation of units and upgrades is something that happenes almost instantly in StarCraft's game engine and doesnt require any other information expect the trainer that will service the ResourceRequest.
/// \n However the construction of a building is more complicated. Due to the possibility of a worker being destroyed on its way to construct a building or the tiles where a building is going to be placed is already occupied. You can compare this type of request as something that is more asynchronous.\n
/// This requires more information and fault tollerance from ProtoBot to be able to full service a ResourceRequest for buildings.
/// </summary>
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

	//Request Identifiers
	BWAPI::Unit requestedBuilding = nullptr;

	//Used for Nexuses so we dont clog queue with one assimlator for other nexuses that need them at the same time.
	BWAPI::Position nexusPositionRef = BWAPI::Positions::Invalid;
	const BWEM::Base* base = nullptr;

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
	bool gotPositionToBuild = false;
	BWAPI::Position placementPos = BWAPI::Positions::Invalid;
	BWAPI::TilePosition tileToPlace = BWAPI::TilePositions::Invalid;
	PlacementInfo placementInfo;

	size_t requestNumber = -1;
};

/// <summary>
/// Counter of the amount of requests of a specific unit type are already in the list of ResourceRequests in ProtoBot.
/// </summary>
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
	int pylons_requests = 0;

	//Upgrades
	int singularity_requests = 0;
	int groundWeapons_requests = 0;
	int groundArmor_requests = 0;
	int plasmaShields_requests = 0;
	int legEnhancements_requests = 0;
};

/// <summary>
/// The ProtoBotCommander is the main way information and BWAPI events are passed to the managers of ProtoBot. The class holds no real intelligence besides passing ResourceRequests to the StrategyManager and BuildManager of ProtoBot. \n
/// The main purpose of this class is to be able to provide a easy way for information to be passed around when needed and for each manager to process events in the defined order specified in each function, updating infromation along the way.
/// 
/// </summary>
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
	bool drawUnitDebug = false;
	bool drawBWEBDebug = false;
	bool drawToolsDebug = false;

	int requestCount = 0;

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
	void requestBuilding(BWAPI::UnitType building, bool fromBuildOrder = false, bool isWall = false, bool isRampPlacement = false, BWAPI::Position nexusPosition = BWAPI::Positions::Invalid, const BWEM::Base* baseLocation = nullptr);
	void requestUnit(BWAPI::UnitType unit, BWAPI::Unit buildingToTrain, bool fromBuildOrder = false);
	void requestUpgrade(BWAPI::Unit unit, BWAPI::UpgradeType upgrade, bool fromBuildOrder = false);
	void requestCheese(BWAPI::UnitType, BWAPI::Unit);
	void checkBuildingAlreadyHasPlan(BWAPI::Unit); //Might not need this if we already have the minerals to spend for units.
	void updateRequestCounter(BWAPI::UnitType unit);
	void drawResourceRequestQueue(int x, int y, bool background = true);
	void drawResouceRequestCount(int x, int y, bool background = true);

	bool upgradeAlreadyRequested(BWAPI::Unit building);
	bool requestedBuilding(BWAPI::UnitType building, BWAPI::Position nexusPosition = BWAPI::Positions::Invalid);
	bool checkUnitIsBeingWarpedIn(BWAPI::UnitType type, const BWEM::Base* nexus = nullptr);
	bool checkUnitIsPlanned(BWAPI::UnitType building, BWAPI::Position nexusPosition = BWAPI::Positions::Invalid);
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
	bool buildOrderCompleted();
	bool checkWorkerIsConstructing(BWAPI::Unit);

	// Scouting Manager Methods
	BWAPI::Unit getUnitToScout();

	// Strategy Manager Methods
	bool shouldGasSteal();

private:
	EnemyLocations enemy_;
};