#include "StrategyManager.h"
#include <BWAPI.h>

StrategyManager::StrategyManager()
{

}

void StrategyManager::onStart()
{
	std::cout << "StrategyManager is a go!" << '\n';
	StrategyManager::currentState = new ContentState();
	currentState->enter();
}

void StrategyManager::onFrame()
{
	const int frame = BWAPI::Broodwar->getFrameCount();
	int previousFrameSecond = 0;
	const int seconds = frame / (24);

	State* state = StrategyManager::currentState->evaluate();
	if (state != NULL)
	{
		StrategyManager::currentState->exit();
		delete StrategyManager::currentState;
		StrategyManager::currentState = state;
		StrategyManager::currentState->enter();
	}

	if (seconds % 5 == 0)
	{
		StrategyManager::boredomMeter += boredomPerSecond;
	}

	StrategyManager::printBoredomMeter();
}