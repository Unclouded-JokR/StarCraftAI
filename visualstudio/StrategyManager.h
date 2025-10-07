#pragma once

#include <BWAPI.h>

class State;
class StrategyManager
{
public:
	StrategyManager();
	float boredomMeter = 0.0f; //Value between 0-1
	float angerMeter = 0.0f; //Value between 0-1;
	float boredomPerSecond = 0.01f;
	float angerFromUnitDeath = .005f;
	float egoFromEnemyUnitDeath = .01f;

	void onStart();
	void onFrame();
	void onUnitDestroy(BWAPI::Unit unit);
	void onEnd(bool isWinner);
	void printBoredomMeter();
	void printAngerMeter();

private:
	State* currentState; //go over smart pointers
};

class State
{
public:
	State() = default;
	virtual void enter() = 0;
	virtual void exit() = 0;
	virtual State* evaluate(StrategyManager &strategyManager) = 0;
	virtual ~State() = default;
};

class BoredomState : public State
{
public:

	void enter() override
	{
		BWAPI::Broodwar->sendText("Entered Bored");
		std::cout << "Boredom Entered" << '\n';
	}

	void exit() override
	{
		BWAPI::Broodwar->sendText("No longer Bored");
	}

	State* evaluate(StrategyManager &strategyManager) override
	{
		return nullptr;
	}
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

	State* evaluate(StrategyManager& strategyManager) override
	{
		if (strategyManager.boredomMeter >= 1.0f)
		{
			return new BoredomState();
		}

		return nullptr;
	}
};

class DenialState : public State
{
public:

	void enter() override
	{
		BWAPI::Broodwar->sendText("Entered Denial");
		std::cout << "Content Entered" << '\n';
	}

	void exit() override
	{
		BWAPI::Broodwar->sendText("No longer in Denial");
	}

	State* evaluate(StrategyManager& strategyManager) override
	{
		return nullptr;
	}
};

class EgoState : public State
{
public:

	void enter() override
	{
		BWAPI::Broodwar->sendText("Entered Ego");
		std::cout << "Content Entered" << '\n';
	}

	void exit() override
	{
		BWAPI::Broodwar->sendText("No longer Egoing");
	}

	State* evaluate(StrategyManager& strategyManager) override
	{
		return nullptr;
	}
};

class AngryState : public State
{
public:

	void enter() override
	{
		BWAPI::Broodwar->sendText("Entered Angry");
		std::cout << "Content Entered" << '\n';
	}

	void exit() override
	{
		BWAPI::Broodwar->sendText("No longer Angy");
	}

	State* evaluate(StrategyManager& strategyManager) override
	{
		return nullptr;
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

	State* evaluate(StrategyManager &strategyManager) override
	{
		const int frame = BWAPI::Broodwar->getFrameCount();
		const int seconds = frame / (24);

		//Add strategy anlysis here

		if (seconds == rageTime + timeWhenRageEntered) //Check if rage levels are still not above expected
			return new ContentState();

		return nullptr;
	}
};
