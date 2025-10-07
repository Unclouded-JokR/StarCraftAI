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

class BoredomState : public State
{
public:

	void enter() override
	{
		BWAPI::Broodwar->sendText("Entered Bored");
		std::cout << "Content Entered" << '\n';
	}

	void exit() override
	{
		BWAPI::Broodwar->sendText("No longer Bored");
	}

	State* evaluate() override
	{
		return NULL;
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

	State* evaluate() override
	{
		return NULL;
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

	State* evaluate() override
	{
		return NULL;
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
	float boredomMeter = 0.0f; //Value between 0-1
	float angerMeter = 0.0f; //Value between 0-1;

	float boredomPerSecond = 0.01f;
	float angerFromUnitDeath = .005f;
	float egoFromEnemyUnitDeath = .01f;
	void onStart();
	void onFrame();
	void onUnitDestroy(BWAPI::Unit unit);
	void onEnd(bool isWinner);

	void printBoredomMeter()
	{
		std::cout << "Boredom Meter = [";

		for (float i = 0.1f; i < 1.0f; i += .1f)
		{
			if (boredomMeter > i)
			{
				std::cout << "=";
			}
			else
			{
				std::cout << "-";
			}
		}

		std::cout << "] ";

		std::cout << (boredomMeter * 100.0f) << "%\n";
	}

	void printAngerMeter()
	{
		std::cout << "Anger Meter = [";

		for (float i = 0.1f; i < 1.0f; i += .1f)
		{
			if (boredomMeter > i)
			{
				std::cout << "=";
			}
			else
			{
				std::cout << "-";
			}
		}

		std::cout << "] ";

		std::cout << (angerMeter * 100.0f) << "%\n";
	}

private:
	State* currentState; //go over smart pointers
};
