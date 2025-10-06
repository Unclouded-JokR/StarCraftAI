#pragma once

#include <BWAPI.h>

class State
{
public:
	State() = default;
	virtual void enter() = 0;
	virtual void exit() = 0;
	virtual State* evaluate() = 0;
	virtual ~State() = default;
};


class ContentState : public State
{
public:

	void enter() override
	{
		BWAPI::Broodwar->sendText("Entered Content");
		std::cout << "Content Entered" << '\n';
	}

	void exit() override
	{
		BWAPI::Broodwar->sendText("No longer Content");
	}

	State* evaluate() override
	{
		return NULL;
	}
};


class RageState : public State
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

	void exit() override
	{
		BWAPI::Broodwar->sendText("Rage Ended");
	}

	State* evaluate() override 
	{
		const int frame = BWAPI::Broodwar->getFrameCount();
		const int seconds = frame / (24);

		//Add strategy anlysis here

		if (seconds == rageTime + timeWhenRageEntered) //CHeck if rage levels are still not above expected
			return new ContentState();

		return NULL;
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
