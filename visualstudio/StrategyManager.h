#pragma once
#include <BWAPI.h>
constexpr int FRAMES_PER_SECOND = 24;

class StrategyManager;

class State {
public:
	virtual void enter(StrategyManager& strategyManager) = 0;
	virtual void exit(StrategyManager& strategyManager) = 0;
	virtual void evaluate(StrategyManager& strategyManager) = 0;
	virtual ~State() = default;
};

/*
*								[Boredom State]
* 
* [Notes]:
* Start making slightly more aggressive moves with the hopes of getting into combat.
* Can optionally make this  that the enemy player is playing a more potentially more defensive or expand style of play.
* 
*/
class BoredomState : public State {
public:
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
class ContentState : public State {
public:
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
class DenialState : public State {
public:
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
class EgoState : public State {
public:
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

/*
*								[Angry State]
*
*[Notes]:
* 
*/
class AngryState : public State {
public:
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

/*
*								[Rage State]
*
*[Notes]:
*/
class RageState : public State {
public:
	int timeWhenRageEntered = 0;
	int rageTime = 30 * FRAMES_PER_SECOND;

	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

class StrategyManager
{
public:
	StrategyManager();
	float boredomMeter = 0.0f; //Value between 0-1
	float angerMeter = 0.0f; //Value between 0-1;
	float egoMeter = 0.0f; //Value between 0-1;

	float boredomPerSecond = 0.01f;
	float angerFromUnitDeath = .005f;
	float egoFromEnemyUnitDeath = .01f;

	void onStart();
	void onFrame();
	void onUnitDestroy(BWAPI::Unit unit); //for buildings and workers
	void printBoredomMeter();
	void printAngerMeter();
	void changeState(State*);
	void battleLost(); //Optionally can makes these two a single function with a bool parameter 
	void battleWon();

	static ContentState contentState;
	static BoredomState boredomState;
	static EgoState egoState;
	static DenialState denialState;
	static RageState rageState;
	static AngryState angryState;
	State* currentState;
};