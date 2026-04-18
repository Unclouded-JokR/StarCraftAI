#pragma once
#include <BWAPI.h>
#include <variant>
#include <vector>
#include <bwem.h>
#include <unordered_map>
#include <set>
#include "../starterbot/Tools.h"
#include "SpenderManager.h"
#include "Hashes.h"

#define FRAMES_PER_SECOND 24
#define SUPPLY_THRESHOLD_EARLYGAME 2
#define SUPPLY_THRESHOLD_MIDGAME 7
#define SUPPLY_THRESHOLD_LATEGAME 15
#define MIDGAME_TIME 5
#define LATEGAME_TIME 15
#define MAX_SUPPLY 200
#define NUM_SQUADS_TO_ATTACK 4
#define MINIMUM_SUPPLY_TO_ALL_IN 150


#define MAX_EARLY_ZEALOTS 3
#define MAX_WORKERS 75
#define MAX_DARK_TEMPLARS 2
#define MAX_OBSERVERS_FOR_SCOUTING 4
#define MAX_OBSERVERS_FOR_INVIS_UNITS 4
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
	OBSERVER_SCOUTS, //4 Observers max if we dont need detectors.
	INFINITE_DRAGOONS,

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
	2, 4, 1, 1, 2, 0, 0, 0
};

const ProductionGoals productionGoalMid =
{
	2, 8, 1, 1, 2, 1, 0, 0
};

const ProductionGoals productionGoalLate =
{
	3, 8, 2, 1, 2, 1, 1, 1
};

struct PossibleUnitRequest {
	BWAPI::UnitType unit;
	BWAPI::Unit trainer;
};

struct PossibleUpgradeRequest {
	BWAPI::UpgradeType upgrade;
	BWAPI::Unit trainer;
};

struct PossibleBuildingRequest {
	BWAPI::UnitType building;

	//Used for assimilators
	BWAPI::Position nexusPosition = BWAPI::Positions::Invalid;
	const BWEM::Base* base = nullptr;
};

struct PossibleRequests
{
	//When units is populated with things we can create it automatically orders them in a priority based on the UnitProductionGoals list.
	std::vector<PossibleUnitRequest> units;
	std::vector<PossibleBuildingRequest> supplyBuildings;

	std::vector<PossibleUpgradeRequest> upgrades;

	//This should only be one request so we can project forward
	PossibleBuildingRequest buildings;
};

class StrategyManager
{
private:
	ProductionFocus ProtoBot_ProductionFocus = ProductionFocus::UNIT_PRODUCTION;
	std::vector<int> expansionTimes = { 5, 10, 15, 17, 20, 25, 30, 35, 40, 50};
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
	void drawUnitProductionGoals();
	void drawUpgradeProductionGoals();

	ProtoBotProductionCount ProtoBot_createdUnitCount;
	UpgradesInProduction upgradesInProduction;

	//Buildings that can create units
	BWAPI::Unitset resourceDepots;
	BWAPI::Unitset unitProduction; //Units that can create combat units
	BWAPI::Unitset upgradeProduction; //Units that can research upgrades

	//Buildings that can research upgrades
	BWAPI::Unitset cybernetics;
	BWAPI::Unitset forges;
	BWAPI::Unitset citadels;

	//Workers
	BWAPI::Unitset workers;

	std::set<UnitProductionGoals> unitProductionGoals;
	std::set<UpgradeProductionGoals> upgradeProductionGoals;

	bool opponentRaceNotKnown = true;

	int supplyThreshold = SUPPLY_THRESHOLD_EARLYGAME;

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

	// Variables for attack phase logic
	bool isAttackPhase = false;
	bool drawStrategyDebug = false;

	std::unordered_set<BWAPI::Position, PositionHash> phantomPositions; // set of positions of phantom units (weird starcraft bug where unit exists after death)
	ProtoBotCommander* commanderReference;

	StrategyManager(ProtoBotCommander* commanderToAsk);

	void onStart();
	std::vector<Action> onFrame(std::vector<ResourceRequest>& resourceRequests);
	void onUnitDestroy(BWAPI::Unit); //for buildings and workers
	void onUnitCreate(BWAPI::Unit);
	void onUnitComplete(BWAPI::Unit);

	//New stuff I am adding
	UnitProductionGameCounter unitProductionCounter;

	BWAPI::Race opponentRace = BWAPI::Races::Unknown;

	//Have these update active goals.
	void updateUnitProductionGoals();
	void updateUpgradeGoals();

	void planUnitProduction(PossibleRequests&);
	void planUpgradeProduction(PossibleRequests&);
	void planBuildingProduction(std::vector<ResourceRequest>& resourceRequests, PossibleRequests&);
	void finalizeProductionPlan(std::vector<ResourceRequest>& resourceRequests, PossibleRequests&);
	const BWEM::Base& getBaseReference(BWAPI::Unit nexus);

	void drawGameUnitProduction(UnitProductionGameCounter& unitProduction, int x, int y, bool background = true);

	//Not you
	bool shouldGasSteal();
	
	//Assimilators require special case.
	bool checkAlreadyRequested(BWAPI::UnitType type);
};