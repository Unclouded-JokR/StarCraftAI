#include "StrategyManager.h"
#include "ProtoBotCommander.h"
#include <BWAPI.h>

int previousFrameSecond = 0;

#pragma region StateDefinitions
void StrategyBoredomState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Bored");
	std::cout << "Boredom Entered" << std::endl;

	strategyManager.boredomMeter = 0.0f;
}

void StrategyBoredomState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer Bored");

	strategyManager.boredomMeter = 0.0f;
}

void StrategyBoredomState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.boredomMeter >= 1.0f || strategyManager.angerMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::angryState);
		return;
	}
	else if (strategyManager.egoMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::egoState);
		return;
	}
}

void StrategyContentState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Content");
	std::cout << "Content Entered" << std::endl;

	//After rage or at the start of the game. All emotional meters will be 0.
	strategyManager.angerMeter = 0.0f;
	strategyManager.boredomMeter = 0.0f;
	strategyManager.egoMeter = 0.0f;
}

void StrategyContentState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer Content");
}

void StrategyContentState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.boredomMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::boredomState);
		return;
	}
	else if (strategyManager.angerMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::denialState);
		return;
	}
	else if (strategyManager.egoMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::egoState);
		return;
	}
}

void StrategyDenialState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Denial");
	std::cout << "Content Entered" << std::endl;
	strategyManager.angerMeter = 0.0f;
}

void StrategyDenialState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer in Denial");

	strategyManager.angerMeter = 0.0f;
}

void StrategyDenialState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.boredomMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::boredomState);
		return;
	}
	else if (strategyManager.angerMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::angryState);
		return;
	}
	else if (strategyManager.egoMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::egoState);
		return;
	}

}

void StrategyEgoState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Ego");
	std::cout << "Content Entered" << std::endl;
}

void StrategyEgoState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer Egoing");

	strategyManager.egoMeter = 0.0f;
}

void StrategyEgoState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.boredomMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::boredomState);
		return;
	}
	else if (strategyManager.angerMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::denialState);
		return;
	}


}

void StrategyAngryState::enter(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Entered Angry");
	std::cout << "Angry Entered" << '\n';
}

void StrategyAngryState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("No longer Angry");
	strategyManager.angerMeter = 0.0f;
	strategyManager.boredomMeter = 0.0f;
}

void StrategyAngryState::evaluate(StrategyManager& strategyManager)
{
	if (strategyManager.egoMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::egoState);
		return;
	}


}

void StrategyRageState::enter(StrategyManager& strategyManager)
{
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / (FRAMES_PER_SECOND);

	timeWhenRageEntered = seconds;

	BWAPI::Broodwar->sendText("Entered Rage");
}

void StrategyRageState::exit(StrategyManager& strategyManager)
{
	BWAPI::Broodwar->sendText("Rage Ended");

	strategyManager.boredomMeter = 0.0f;
	strategyManager.angerMeter = 0.0f;
	strategyManager.egoMeter = 0.0f;
}

void StrategyRageState::evaluate(StrategyManager& strategyManager)
{
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / (24);

	//Add strategy anlysis here

	if (seconds == rageTime + timeWhenRageEntered) //Check if rage levels are still not above expected
	{
		strategyManager.changeState(&StrategyManager::contentState);
		return;
	}

}
#pragma endregion

StrategyContentState StrategyManager::contentState("Content");
StrategyBoredomState StrategyManager::boredomState("Boredom");
StrategyEgoState StrategyManager::egoState("Ego");
StrategyDenialState StrategyManager::denialState("Denial");
StrategyRageState StrategyManager::rageState("Rage");
StrategyAngryState StrategyManager::angryState("Angry");


StrategyManager::StrategyManager(ProtoBotCommander *commanderReference) : commanderReference(commanderReference)
{

}

void StrategyManager::onStart()
{
	std::cout << "StrategyManager is a go!" << '\n';
	StrategyManager::currentState = &StrategyManager::contentState;
	currentState->enter(*this);

}

void StrategyManager::onFrame()
{
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / (FRAMES_PER_SECOND);

	
	if ((frame - previousFrameSecond) == 24)
	{
		previousFrameSecond = frame;
		StrategyManager::boredomMeter += boredomPerSecond;
	}

	/*if (BWAPI::Broodwar->self()->supplyTotal() > BWAPI::Broodwar->enemy()->supplyUsed() / 2)
	{
		
	}*/

	//Divide by 2 because zergs workers costs .5 supply
	//std::cout << "Enemy total supply " << BWAPI::Broodwar->self()->supplyUsed() / 2 << std::endl;

	currentState->evaluate(*this);

	//StrategyManager::printBoredomMeter();
}

std::string StrategyManager::getCurrentStateName()
{
	return currentState->stringStateName;
}

void StrategyManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit == nullptr)
		return;

	//need to make this more dynammic for scouts and what not but for now this works for general case.

	BWAPI::Player owner = unit->getPlayer();
	BWAPI::UnitType unitType = unit->getType();


	/*
	* For both cases we want to consider the priority of units, workers will add the most points to ego and anger.
	* 
	* For now we will add a constant value for all units. Need to create a forumla the considers the unittype and the score of a unit.
	*/
	if (owner == BWAPI::Broodwar->self() && ((unitType.isWorker() || unitType.isBuilding())))
	{
		std::cout << "Our unit died" << std::endl;
		StrategyManager::angerMeter += StrategyManager::angerFromUnitDeath;
	}
	else if(owner == BWAPI::Broodwar->enemy() && ((unitType.isWorker() || unitType.isBuilding())))
	{
		std::cout << "Enemy unit died" << std::endl;
		StrategyManager::angerMeter += StrategyManager::egoFromEnemyUnitDeath;
	}

	//Reset boredom to 0 since we had a confrontation
	StrategyManager::boredomMeter = 0.0f;
}

void StrategyManager::changeState(StrategyState* state)
{
	currentState->exit(*this);
	currentState = state;
	currentState->enter(*this);
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

	std::cout << (boredomMeter * 100.0f) << "% " << StrategyManager::currentState->printStateName() << std::endl;
}

void StrategyManager::printAngerMeter()
{
	std::cout << "Anger Meter = [";

	for (float i = 0.1f; i < 1.0f; i += .1f)
	{
		if (angerMeter > i)
		{
			std::cout << "=";
		}
		else
		{
			std::cout << "-";
		}
	}

	std::cout << "] ";

	std::cout << (angerMeter * 100.0f) << "% " << StrategyManager::currentState->printStateName() << std::endl;;
}