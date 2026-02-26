#pragma once
#include <BWAPI.h>
#include <variant>
#include <vector>
#include <bwem.h>

#define FRAMES_PER_SECOND 24
#define SUPPLY_THRESHOLD_EARLYGAME 3
#define SUPPLY_THRESHOLD_MIDGAME 5
#define SUPPLY_THRESHOLD_LATEGAME 8
#define MIDGAME_TIME 5
#define LATEGAME_TIME 15
#define MAX_SUPPLY 200

//Adjust this time so enemy bots are not able to continue researching since we wouldnt scale as well as them. Kinda high for an start craft game time.
#define FRAMES_TO_ALL_IN 57600

class ProtoBotCommander;
struct FriendlyBuildingCounter;

enum ProductionFocus { EXPANDING_INFLUENCE, UNIT_PRODUCTION, STENGTHENING_ARMY};
enum ExpansionSate { CURRENTLY_EXPANDING, NO_EXPANSIONS_PLANNED };
enum ProtoBotBlocks { NO_AVALIBLE_BLOCKS, HAVE_BLOCKS };

//All units the strategy manager is able to request
struct ProtoBotReuqestCounter {
	int gateway_requests = 0;
	int pylon_requests = 0;
	int nexus_requests = 0;
	int assimilator_requests = 0;
	int forge_requests = 0;
	int cybernetics_requests = 0;
	int robotics_requests = 0;
	int observatory_requests = 0;
};

struct Action {
	enum ActionType { ACTION_EXPAND, ACTION_SCOUT, ACTION_BUILD, ACTION_ATTACK, ACTION_DEFEND, ACTION_NONE};
	ActionType type = ACTION_NONE;

	BWAPI::UnitType expansionToConstruct = BWAPI::UnitTypes::None;
	BWAPI::UnitType buildingToConstruct = BWAPI::UnitTypes::None;
	BWAPI::Position attackPosition = BWAPI::Positions::Invalid;
	BWAPI::Position defendPosition = BWAPI::Positions::Invalid;
};

class StrategyManager
{
private:
	ProductionFocus ProtoBot_ProductionGoal = ProductionFocus::EXPANDING_INFLUENCE;
	std::vector<int> expansionTimes = { 3, 6, 9, 13, 18 };
	size_t minutesPassedIndex = 0;

	//If we have excess minerals something is most likely wrong.
	int mineralExcessToExpand = 1000;
	int frameSinceLastScout = 0;
	const BWEM::Area* mainArea;

public:
	ProtoBotReuqestCounter requestCounter;
	std::unordered_set<const BWEM::Area*> ProtoBot_Areas;
	std::unordered_set<const BWEM::ChokePoint*> ProtoBotArea_SquadPlacements;
	std::unordered_map<const BWEM::ChokePoint*, bool> PositionsFilled;
	BWAPI::Position startingChoke;

	ProtoBotCommander* commanderReference;

	StrategyManager(ProtoBotCommander* commanderToAsk);
	void onStart();
	std::vector<Action> onFrame();
	void onUnitDestroy(BWAPI::Unit); //for buildings and workers
	void onUnitCreate(BWAPI::Unit);
	void onUnitMorph(BWAPI::Unit);
	void onUnitComplete(BWAPI::Unit);
	void onUnitShow(BWAPI::Unit);

	//Methods to be able to determine how much resources we need to reserve for mineral production.
	int getTotalMineralsNeeded();
	int getTotalGasNeeded();

	BWAPI::Unitset getProtoBotBuildings();
	bool checkTechTree(BWAPI::UnitType, FriendlyBuildingCounter);
	bool shouldGasSteal();
	bool checkAlreadyRequested(BWAPI::UnitType type);
};