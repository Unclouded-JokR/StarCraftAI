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
	int frame = BWAPI::Broodwar->getFrameCount();
	int seconds = frame / (24);

	State* state = StrategyManager::currentState->evaluate();
	if (state != NULL)
	{
		StrategyManager::currentState->exit();
		delete StrategyManager::currentState;
		StrategyManager::currentState = state;
		StrategyManager::currentState->enter();
	}

	/*if(seconds % 5 == 0)
		std::cout << seconds << " have passed since last encounter with enemy." << '\n';*/
}