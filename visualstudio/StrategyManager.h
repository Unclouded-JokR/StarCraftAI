#pragma once

#include <BWAPI.h>

class State
{
public:
	State() {}
	virtual void enter() {}
	virtual void exit(State* state) {}
	virtual void evaluate(State* state) {}
	virtual ~State() {}
};

class Rage : public State
{
public:
	int timeWhenRageEntered = 0;
	int rageTime = (30 * 24); //30 Seconds

	void enter() override
	{
		const int frame = BWAPI::Broodwar->getFrameCount();
		const int seconds = frame / (24);

		timeWhenRageEntered = seconds;

		BWAPI::Broodwar->sendText("Entered Rage");
	}

	void exit(State* state) override
	{
		BWAPI::Broodwar->sendText("Rage Ended");
		state->enter();
	}

	void evaluate(State* state) override 
	{
		const int frame = BWAPI::Broodwar->getFrameCount();
		const int seconds = frame / (24);

		//Add strategy anlysis here

		if (seconds == rageTime + timeWhenRageEntered)
			exit(state);
	}
};

class Content : public State
{
public:

	void enter() override
	{
		BWAPI::Broodwar->sendText("Entered Content");
		std::cout << "Content Entered" << '\n';
	}

	void exit(State* state) override
	{
		BWAPI::Broodwar->sendText("No longer Content");
	}

	void evaluate(State* state) override
	{

	}
};


class StrategyManager
{
public:
	StrategyManager();

	float boredomPerSecond = 0.0001f;
	float angerFromUnitDeath = .005f;
	float egoFromEnemyUnitDeath = .01f;
	void onStart();
	void onFrame();
	void onUnitDestroy(BWAPI::Unit unit);
	void onEnd(bool isWinner);

private:
	State* currentState; //go over smart pointers
};
