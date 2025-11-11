#pragma once
#include <BWAPI.h>
#include <variant>
#define FRAMES_PER_SECOND 24

class StrategyManager;

class StrategyState {
public:
	std::string stringStateName;
	StrategyState(std::string stateName) : stringStateName(stateName) {}

	virtual void enter(StrategyManager& strategyManager) = 0;
	virtual void exit(StrategyManager& strategyManager) = 0;
	virtual void evaluate(StrategyManager& strategyManager) = 0;
	virtual ~StrategyState() = default;

	std::string printStateName()
	{
		return "Current state is: " + stringStateName + ".";
	}
};

/*
*								[Boredom State]
*
* [Notes]:
* Start making slightly more aggressive moves with the hopes of getting into combat.
* Can optionally make this  that the enemy player is playing a more potentially more defensive or expand style of play.
*
*/
class StrategyBoredomState : public StrategyState {
public:
	StrategyBoredomState(std::string stateName) : StrategyState(stateName) {}
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

/*
*								[Content State]
*
*[Notes]:
* Normal play and decision making.
* Will try to pick the best move based on time and enemy considerations.
*
*/
class StrategyContentState : public StrategyState {
public:
	StrategyContentState(std::string stateName) : StrategyState(stateName) {}
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

/*
*								[Denial State]
*
*[Notes]:
* Plays quicker and makes more rash decisions, can make the time to choose the best descision quicker.
*
*/
class StrategyDenialState : public StrategyState {
public:
	StrategyDenialState(std::string stateName) : StrategyState(stateName) {}
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

/*
*								[Ego State]
*
*[Notes]:
* Makes greedier decisions that other states would not choose. Takes more time to decide what decision to make.
*/
class StrategyEgoState : public StrategyState {
public:
	StrategyEgoState(std::string stateName) : StrategyState(stateName) {}
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

/*
*								[Angry State]
*
*[Notes]:
* Plays to kill the enemy and prevent them from getting more of a lead. Will make more aggressive moves with the hopes of winning.
*/
class StrategyAngryState : public StrategyState {
public:
	StrategyAngryState(std::string stateName) : StrategyState(stateName) {}
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

/*
*								[Rage State]
*
*[Notes]:
* Very aggressive moves, with the main goal of killing the enemy units that have killed them (get the score of a unit)
* and with the hopes of killing a enemy expansion or highly valued units like workers and what not.
*
* Might use cheese strats here to make enemy lose workers like dropships.
*/
class StrategyRageState : public StrategyState {
public:
	int timeWhenRageEntered = 0;
	int rageTime = 30; //in seconds

	StrategyRageState(std::string stateName) : StrategyState(stateName) {}
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

class ProtoBotCommander;
struct Action;

class StrategyManager
{
public:
	ProtoBotCommander* commanderReference;
	float boredomMeter = 0.0f; //Value between 0-1
	float angerMeter = 0.0f; //Value between 0-1;
	float egoMeter = 0.0f; //Value between 0-1;

	float boredomPerSecond = 0.05f;
	float angerFromUnitDeath = .005f;
	float egoFromEnemyUnitDeath = .01f;

	StrategyManager(ProtoBotCommander* commanderToAsk);
	std::string onStart();
	Action onFrame();
	void onUnitDestroy(BWAPI::Unit unit); //for buildings and workers
	void printBoredomMeter();
	void printAngerMeter();
	void changeState(StrategyState*);
	void battleLost(); //Optionally can makes these two a single function with a bool parameter 
	void battleWon();
	std::string getCurrentStateName();

	static StrategyContentState contentState;
	static StrategyBoredomState boredomState;
	static StrategyEgoState egoState;
	static StrategyDenialState denialState;
	static StrategyRageState rageState;
	static StrategyAngryState angryState;
	StrategyState* currentState;
};