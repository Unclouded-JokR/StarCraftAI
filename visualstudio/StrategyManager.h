#pragma once
#include <BWAPI.h>

class StrategyManager;

class State {
public:
	virtual void enter(StrategyManager& strategyManager) = 0;
	virtual void exit(StrategyManager& strategyManager) = 0;
	virtual void evaluate(StrategyManager& strategyManager) = 0;
	virtual ~State() = default;
};

class BoredomState : public State {
public:
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

class ContentState : public State {
public:
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

class DenialState : public State {
public:
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

class EgoState : public State {
public:
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

class AngryState : public State {
public:
	void enter(StrategyManager& strategyManager) override;
	void exit(StrategyManager& strategyManager) override;
	void evaluate(StrategyManager& strategyManager) override;
};

class RageState : public State {
public:
	int timeWhenRageEntered = 0;
	int rageTime = 30 * 24;

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
	void onUnitDestroy(BWAPI::Unit unit);
	void printBoredomMeter();
	void printAngerMeter();
	void changeState(State*);

	static ContentState contentState;
	static BoredomState boredomState;
	static EgoState egoState;
	static DenialState denialState;
	static RageState rageState;
	static AngryState angryState;
	State* currentState;
};