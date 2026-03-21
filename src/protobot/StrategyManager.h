#pragma once
#include <BWAPI.h>
#include <variant>
#include <vector>
#include <bwem.h>
#include "../starterbot/Tools.h"
#include "SpenderManager.h"

#define FRAMES_PER_SECOND 24
#define SUPPLY_THRESHOLD_EARLYGAME 3
#define SUPPLY_THRESHOLD_MIDGAME 5
#define SUPPLY_THRESHOLD_LATEGAME 8
#define MIDGAME_TIME 5
#define LATEGAME_TIME 15
#define MAX_SUPPLY 200
#define NUM_SQUADS_TO_ATTACK 4

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
struct ResourceRequest;

enum ExpansionSate { CURRENTLY_EXPANDING, NO_EXPANSIONS_PLANNED };
enum ProtoBotBlocks { NO_AVALIBLE_BLOCKS, HAVE_BLOCKS };

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

struct Action {
	enum ActionType { ACTION_SCOUT, ACTION_ATTACK, ACTION_DEFEND, ACTION_REINFORCE, ACTION_NONE};
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
	std::vector<ProductionGoals> ProtoBot_ProductionGoals = { productionGoalEarly, productionGoalMid, productionGoalLate};
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

public:
	std::unordered_set<const BWEM::Area*> ProtoBot_Areas;
	std::unordered_set<const BWEM::ChokePoint*> ProtoBotArea_SquadPlacements;
	std::unordered_map<const BWEM::ChokePoint*, bool> PositionsFilled;
	BWAPI::Position startingChoke;
	BWAPI::Position lastAttackPos;

	ProtoBotCommander* commanderReference;

	StrategyManager(ProtoBotCommander* commanderToAsk);

	void onStart();
	std::vector<Action> onFrame(std::vector<ResourceRequest> &resourceRequests);
	void onUnitDestroy(BWAPI::Unit); //for buildings and workers
	void onUnitCreate(BWAPI::Unit);
	void onUnitMorph(BWAPI::Unit);
	void onUnitComplete(BWAPI::Unit);
	void onUnitShow(BWAPI::Unit);

	BWAPI::Unitset getProtoBotBuildings();
	bool checkTechTree(BWAPI::UnitType, FriendlyBuildingCounter);
	bool shouldGasSteal();
	bool checkAlreadyRequested(BWAPI::UnitType type);
};