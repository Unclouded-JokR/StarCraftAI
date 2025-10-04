#include "StrategyManager.h"
#include <BWAPI.h>

StrategyManager::StrategyManager()
{

}

void StrategyManager::onStart()
{
	std::cout << "StrategyManager is a go!";
}

void StrategyManager::onFrame()
{
	int frame = BWAPI::Broodwar->getFrameCount();
	int seconds = frame / (24);

	if(seconds % 5 == 0)
		std::cout << seconds << " have passed since last encounter with enemy." << '\n';
}