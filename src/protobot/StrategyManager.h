#pragma once
#include <BWAPI.h>
#include <variant>
#include <vector>
#include <bwem.h>
#include <unordered_map>
#include <set>
#include "../starterbot/Tools.h"
#include "SpenderManager.h"

#define FRAMES_PER_SECOND 24
#define SUPPLY_THRESHOLD_EARLYGAME 4
#define SUPPLY_THRESHOLD_MIDGAME 6
#define SUPPLY_THRESHOLD_LATEGAME 9
#define MIDGAME_TIME 5
#define LATEGAME_TIME 15
#define MAX_SUPPLY 200
#define NUM_SQUADS_TO_ATTACK 4


#define MAX_EARLY_ZEALOTS 3
#define MAX_WORKERS 75
#define MAX_DARK_TEMPLARS 2
#define MAX_OBSERVERS_FOR_SCOUTING 4
#define MAX_SINGULARITY_UPGRADES 1
#define MAX_GROUND_ARMOR_UPGRADES 3
#define MAX_GROUND_WEAPONS_UPGRADES 3
#define MAX_PLASMA_SHIELD_UPGRADES 3 
#define MAX_LEG_ENHANCEMENTS_UPGRADES 1

#define FRAMES_FOR_NO_GAS_ZEALOT_PUMP 5000 //Incase new production has to limit # of requests, value should also be harder.


//If we sit at same supply level for 4 minutes, all in with squads to end the game.
#define FRAMES_UNTIL_SUPPLY_ALL_IN 5760

//2:30 minutes
#define FRAMES_BEFORE_FORCE_EXPANDING_INFLUENCE 3600

//1:30 minutes
#define UNIT_PRODUCTION_TIME 2160

//30 seconds
#define DELAY_PER_PRODUCTION 720

//Adjust this time so enemy bots are not able to continue researching since we wouldnt scale as well as them. Kinda high for an start craft game time.
#define FRAMES_TO_ALL_IN 57600

class ProtoBotCommander;
struct FriendlyBuildingCounter;
struct FriendlyUnitCounter;
struct FriendlyUpgradeCounter;
struct ResourceRequest;

enum ProductionFocus { EXPANDING_INFLUENCE, UNIT_PRODUCTION };
enum ExpansionSate { CURRENTLY_EXPANDING, NO_EXPANSIONS_PLANNED };
enum ProtoBotBlocks { NO_AVALIBLE_BLOCKS, HAVE_BLOCKS };

//Different from the information managers friendly unit counter since this include "Created" units as well.
struct ProtoBotProductionCount
{
	//Units
	int created_workers = 0;
	int created_zealots = 0;
	int created_dragoons = 0;
	int created_dark_templars = 0;
	int created_observers = 0;

	//Buildings
	int created_nexus = 0;
	int created_gateway = 0;
	int created_forge = 0;
	int created_cybernetics = 0;
	int created_roboticsFacility = 0;
	int created_observatory = 0;
	int created_citadel = 0;
	int created_templarArchives = 0;
	int created_photonCannons = 0;
	int created_pylons = 0;

	//Upgrades
	bool singularity_requests = false;
	int groundWeapons_requests = 0;
	int groundArmor_requests = 0;
	int plasmaShields_requests = 0;
	int legEnhancements_requests = 0;
};

struct UpgradesInProduction
{
	int singularity_charge = 0;
	int ground_weapons = 0;
	int ground_armor = 0;
	int plasma_shields = 0;
	int leg_enhancements = 0;
};

struct ProductionGoals
{
	int nexusCount = 0;
	int gatewayCount = 0;
	int forgeCount = 0;
	int cyberneticsCount = 0;
	int roboticsCount = 0;
	int observatoryCount = 0;
	int citadelCount = 0;
	int templarArchivesCount = 0;
};

struct IncompleteBuildingCounter
{
	int arbiterTribunal = 0;
	int assimilator = 0;
	int citadelOfAdun = 0;
	int cyberneticsCore = 0;
	int fleetBeacon = 0;
	int forge = 0;
	int gateway = 0;
	int nexus = 0;
	int observatory = 0;
	int photonCannon = 0;
	int pylon = 0;
	int roboticsFacility = 0;
	int roboticsSupportBay = 0;
	int shieldBattery = 0;
	int stargate = 0;
	int templarArchives = 0;
};

struct UnitProductionGameCounter {
	int worker = 0;
	int zealots = 0;
	int dragoons = 0;
	int dark_templars = 0;
	int observers = 0;
};

//Need to check production of units to make sure we are not oversaturating farming.
enum UnitProductionGoals {
	SATURATE_WORKERS, //Max 75 workers.
	EARLY_ZEALOTS, //3 Zealots early.
	DARK_TEMPLAR_ATTEMPT, //2 Dark Templar's early if against Terran or Protoss.
	INFINITE_DRAGOONS,
	OBSERVER_SCOUTS, //4 Observers max if we dont need detectors.

	//Edge case productions
	SOMETHING_WENT_WRONG_GO_INFINITE_ZEALOTS, //Should not have to use this. Covering the case where assimilators arent being made.
	INVISIBLE_UNIT_DETECTED_SQUADS_NEED_OBSERVERS, //Constant production of observers per squad.
	FLYING_UNIT_DETECTED_NEED_CANNONS //Build cannons at bases/spots where there are stations.
};

enum UpgradeProductionGoals {
	RESEARCH_SINGULARITY_CHARGE, //Should have this every game.
	RESEARCH_GROUND_WEAPONS,
	RESEARCH_GROUND_ARMOR,
	RESEARCH_PLASMA_SHIELDS,

	//Edge case research
	SOMETHING_WENT_WRONG_RESEARCH_LEG_ENHANCEMENTS //Same reasoning as Zealots. Should make them stronger if we dont have gas.
};

struct PotentionalConstruct {
	BWAPI::Unit buildingToTrain;

	BWAPI::Unit unitToCreate;
	BWAPI::UpgradeType upgradeToCreate = BWAPI::UpgradeTypes::Unknown;
	BWAPI::Unit buildingToCreate;
};

struct Action {
	enum ActionType { ACTION_SCOUT, ACTION_ATTACK, ACTION_DEFEND, ACTION_REINFORCE, ACTION_NONE };
	ActionType type = ACTION_NONE;

	BWAPI::Position attackPosition = BWAPI::Positions::Invalid;
	BWAPI::Position defendPosition = BWAPI::Positions::Invalid;
	BWAPI::Position reinforcePosition = BWAPI::Positions::Invalid;
};

//Making gateway amount lower than I would like since the BWEB doesnt make a full 16 blocks for gateways sometimes.
const ProductionGoals productionGoalEarly =
{
	2, 4, 1, 1, 0, 0, 0, 0
};

const ProductionGoals productionGoalMid =
{
	3, 8, 1, 1, 1, 1, 0, 0
};

const ProductionGoals productionGoalLate =
{
	4, 8, 3, 1, 1, 1, 1, 1
};

class StrategyManager
{
private:
	ProductionFocus ProtoBot_ProductionFocus = ProductionFocus::UNIT_PRODUCTION;
	std::vector<int> expansionTimes = { 3, 6, 9, 13, 18 };
	std::vector<ProductionGoals> ProtoBot_ProductionGoals = { productionGoalEarly, productionGoalMid, productionGoalLate };
	size_t ProductionGoal_index = 0;
	size_t minutesPassedIndex = 0;
	int timer = 0;

	IncompleteBuildingCounter incompleteBuildings;
	SpenderManager spenderManager;

	//If we have excess minerals something is most likely wrong.
	int mineralExcessToExpand = 1000;
	int frameSinceLastScout = 0;
	const BWEM::Area* mainArea;

	bool metProductionGoal(FriendlyBuildingCounter);
	bool canAfford(BWAPI::UnitType, std::pair<int, int> resources);

	//New stuff I am adding 
	int activeMiners();
	int activeDrillers();
	void checkForOpponentRace();
	bool haveRequiredTech(BWAPI::UnitType);
	void updateUpgradesBeingCreated();

	//Debug methods
	void drawGameUnitProduction(UnitProductionGameCounter& unitProduction, int x, int y, bool background = true);
	void drawUnitProductionGoals();
	void drawUpgradeProductionGoals();

	UnitProductionGameCounter unitProductionCounter;
	ProtoBotProductionCount ProtoBot_createdUnitCount;
	UpgradesInProduction upgradesInProduction;

	BWAPI::Unitset resourceDepots;
	BWAPI::Unitset unitProduction; //Units that can create combat units
	BWAPI::Unitset upgradeProduction; //Units that can research upgrades

	BWAPI::Unitset cybernetics;
	BWAPI::Unitset forges;
	BWAPI::Unitset citadels;

	BWAPI::Unitset workers;

	std::set<UnitProductionGoals> unitProductionGoals;
	std::set<UpgradeProductionGoals> upgradeProductionGoals;

	bool opponentRaceNotKnown = true;

	const double workerIncomePerFrameMinerals = 0.044;
	const double workerIncomePerFrameGas = 0.069;
	double ourIncomePerFrameMinerals = 0.0;
	double outIncomePerFrameGas = 0.0;

public:
	std::unordered_set<const BWEM::Area*> ProtoBot_Areas;
	std::unordered_set<const BWEM::ChokePoint*> ProtoBotArea_SquadPlacements;
	std::unordered_map<const BWEM::ChokePoint*, bool> PositionsFilled;
	BWAPI::Position startingChoke;
	BWAPI::Position lastAttackPos;

	ProtoBotCommander* commanderReference;

	StrategyManager(ProtoBotCommander* commanderToAsk);

	void onStart();
	std::vector<Action> onFrame(std::vector<ResourceRequest>& resourceRequests);
	void onUnitDestroy(BWAPI::Unit); //for buildings and workers
	void onUnitCreate(BWAPI::Unit);
	void onUnitComplete(BWAPI::Unit);

	//New stuff I am adding
	BWAPI::Race opponentRace = BWAPI::Races::Unknown;

	//Have these update active goals.
	void updateUnitProductionGoals();
	void updateUpgradeGoals();

	//Should we pass requests as argument?
	//Make these pass back stuff we should produce
	//Same with buildings and such
	//Make a function that will then process all the requests and filter them out somehow and deny some.
	// These should be unit sets should be a struct that organizes the unit requests to make addressing what we need to do easier.

	//BWAPI::Unitset plannedUnitProduction();
	//BWAPI::UpgradeTypes plannedUpgradeProduction();
	//BWAPI::Unitset plannedBuildingProduction();

	void planUnitProduction(std::vector<ResourceRequest>& resourceRequests);
	void planUpgradeProduction(std::vector<ResourceRequest>& resourceRequests);

	//Not you
	BWAPI::Unitset getProtoBotBuildings();
	bool shouldGasSteal();
	bool checkAlreadyRequested(BWAPI::UnitType type);
};