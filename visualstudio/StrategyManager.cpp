#include "StrategyManager.h"
#include <BWAPI.h>

StrategyManager::StrategyManager()
{

}
int previousFrameSecond = 0;

#pragma region StateDefinitions
void BoredomState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Bored");
	std::cout << "Boredom Entered" << std::endl;
	strategyManager.boredomMeter = 0.0f;
}

void BoredomState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer Bored");
}

void BoredomState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.boredomMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::angryState;
		enter(strategyManager);
		return;
	}
	else if (strategyManager.angerMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::angryState;
		enter(strategyManager);
		return;
	}
	else if (strategyManager.egoMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::egoState;
		enter(strategyManager);
		return;
	}


}

void ContentState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Content");
	std::cout << "Content Entered" << std::endl;

	//Set Defaults
	strategyManager.angerMeter = 0.0f;
	strategyManager.boredomMeter = 0.0f;
}

void ContentState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer Content");
}

void ContentState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.boredomMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::boredomState;
		enter(strategyManager);
		return;
	}
	else if (strategyManager.angerMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::denialState;
		enter(strategyManager);
		return;
	}
	else if (strategyManager.egoMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::egoState;
		enter(strategyManager);
		return;
	}
}

void DenialState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Denial");
	std::cout << "Content Entered" << std::endl;
	strategyManager.angerMeter = 0.0f;
}

void DenialState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer in Denial");
}

void DenialState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.boredomMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::boredomState;
		enter(strategyManager);
		return;
	}
	else if (strategyManager.angerMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::angryState;
		enter(strategyManager);
		return;
	}
	else if (strategyManager.egoMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::egoState;
		enter(strategyManager);
		return;
	}

}

void EgoState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Ego");
	std::cout << "Content Entered" << std::endl;
}

void EgoState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer Egoing");
}

void EgoState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.boredomMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::boredomState;
		enter(strategyManager);
		return;
	}
	else if (strategyManager.angerMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::denialState;
		enter(strategyManager);
		return;
	}


}

void AngryState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Angry");
	std::cout << "Content Entered" << '\n';
}

void AngryState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer Angy");
}

void AngryState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.egoMeter >= 1.0f)
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::egoState;
		enter(strategyManager);
		return;
	}


}

void RageState::enter(StrategyManager& strategyManager)
{
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / (24);

	timeWhenRageEntered = seconds;

	BWAPI::Broodwar->sendText("Entered Rage");
}

void RageState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Rage Ended");

	strategyManager.boredomMeter = 0.0f;
	strategyManager.angerMeter = 0.0f;
	strategyManager.egoMeter = 0.0f;
}

void RageState::evaluate(StrategyManager& strategyManager)
{
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / (24);

	//Add strategy anlysis here

	if (seconds == rageTime + timeWhenRageEntered) //Check if rage levels are still not above expected
	{
		exit(strategyManager);
		strategyManager.currentState = &StrategyManager::contentState;
		enter(strategyManager);
		return;
	}

}
#pragma endregion

ContentState StrategyManager::contentState;
BoredomState StrategyManager::boredomState;
EgoState StrategyManager::egoState;
DenialState StrategyManager::denialState;
RageState StrategyManager::rageState;
AngryState StrategyManager::angryState;


void StrategyManager::onStart()
{
	std::cout << "StrategyManager is a go!" << '\n';
	StrategyManager::currentState = new ContentState();
	currentState->enter(*this);
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

	StrategyManager::currentState->evaluate(*this);

	StrategyManager::printBoredomMeter();
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