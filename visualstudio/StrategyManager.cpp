#include "StrategyManager.h"
#include <BWAPI.h>

StrategyManager::StrategyManager()
{

}
int previousFrameSecond = 0;

void StrategyManager::onStart()
{
	std::cout << "StrategyManager is a go!" << '\n';
	StrategyManager::currentState = new ContentState();
	currentState->enter();
}

void StrategyManager::onFrame()
{
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / (24);

	if ((frame - previousFrameSecond) == 24)
	{
		previousFrameSecond = frame;
		StrategyManager::boredomMeter += boredomPerSecond;
	}

	State* state = StrategyManager::currentState->evaluate(*this);
	if (state != nullptr)
	{
		StrategyManager::currentState->exit();
		delete StrategyManager::currentState;
		StrategyManager::currentState = state;
		StrategyManager::currentState->enter();
	}

	//StrategyManager::printBoredomMeter();
}

void StrategyManager::onUnitDestroy(BWAPI::Unit unit)
{
	BWAPI::Player owner = unit->getPlayer();

	if (owner == BWAPI::Broodwar->self())
	{
		std::cout << "Our unit died" << std::endl;
	}
	else if(owner == BWAPI::Broodwar->enemy())
	{
		std::cout << "Enemy unit died" << std::endl;
	}
}

void StrategyManager::printBoredomMeter()
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

void StrategyManager::printAngerMeter()
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