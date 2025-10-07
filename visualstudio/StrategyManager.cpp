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

	State* state = StrategyManager::currentState->evaluate();
	if (state != NULL)
	{
		StrategyManager::currentState->exit();
		delete StrategyManager::currentState;
		StrategyManager::currentState = state;
		StrategyManager::currentState->enter();
	}

	//std::cout << "Frame " << frame << ": " << frame << " - " << 24 << " = " << (frame - previousFrameSecond) << "\n";
	if ((frame - previousFrameSecond) == 24)
	{
		previousFrameSecond = frame;
		StrategyManager::boredomMeter += boredomPerSecond;
	}

	StrategyManager::printBoredomMeter();
}