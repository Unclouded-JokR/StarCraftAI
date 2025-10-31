#include "StrategyManager.h"
#include "ProtoBotCommander.h"
#include <BWAPI.h>

int previousFrameSecond = 0;
std::vector<int> expansionTimes = { 5, 10, 20, 30, 40 , 50 };
int minutesPassedIndex = 0;
int frameSinceLastScout = 0;
int frameSinceLastBuild = 0;

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
	else if (strategyManager.angerMeter >= 1.0f || strategyManager.boredomMeter >= 1.0f)
	{
		strategyManager.changeState(&StrategyManager::rageState);
		return;
	}


}

void StrategyRageState::enter(StrategyManager& strategyManager)
{
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / (FRAMES_PER_SECOND);
	timeWhenRageEntered = seconds;

	strategyManager.boredomMeter = 0.0f;
	strategyManager.angerMeter = 0.0f;
	strategyManager.egoMeter = 0.0f;

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
	const int seconds = frame / FRAMES_PER_SECOND;

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
	StrategyManager::currentState = &StrategyManager::contentState;
}

void StrategyManager::onStart()
{
	std::cout << "StrategyManager is a go!" << '\n';
	currentState->enter(*this);

	//Test logic to select a random build order that will be passed to the strategy manager. We can take into account the state the bot was in when it lost in later iterations.
	std::vector<int> myVector = { 10, 20, 30, 40, 50 };
	const size_t test = myVector.size();
	const int chooseRandBuildOrder = rand() % test;
	//return build order chosen
}

Action StrategyManager::onFrame()
{
	None none;
	Action action;
	action.commanderAction = none;
	action.type = ActionType::Action_None;

	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / (FRAMES_PER_SECOND);


	if ((frame - previousFrameSecond) == 24)
	{
		previousFrameSecond = frame;
		StrategyManager::boredomMeter += boredomPerSecond;
	}

	currentState->evaluate(*this);

	const int supplyUsed = (BWAPI::Broodwar->self()->supplyUsed()) / 2;
	const int totalSupply = (BWAPI::Broodwar->self()->supplyTotal()) / 2;
	const bool buildOrderCompleted = commanderReference->buildOrderCompleted();

	//WorkerSet workerSet = commanderReference.checkWorkerSetNeedsAssimilator();

#pragma region Expand
	if (supplyUsed + 4 >= totalSupply)
	{
		Expand actionToTake;
		actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Probe;

		action.commanderAction = actionToTake;
		action.type = ActionType::Action_Expand;
		return action;
	}
	/*else if(workerSet != nullptr)
	{
		Exapnd action;
		action.unitToBuild = BWAPI::UnitTypes::Protoss_Assimilator;
	}*/
	//If we have a stock pile of minerals
	else if (BWAPI::Broodwar->self()->minerals() > 3000)
	{
		Expand actionToTake;
		actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Nexus;

		action.commanderAction = actionToTake;
		action.type = ActionType::Action_Expand;
		return action;
	}
	else if (buildOrderCompleted)
	{
		if (minutesPassedIndex < expansionTimes.size() && seconds / 60 > expansionTimes.at(minutesPassedIndex))
		{
			minutesPassedIndex++;

			Expand actionToTake;
			actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Nexus;
			return action;
		}
	}
#pragma endregion

	//#pragma region Build Anti-Air
	//const std::set<BWAPI::Unit>& knownEnemyUnits = commanderReference->getKnownEnemyUnits();
	//const std::map<BWAPI::Unit, EnemyBuildingInfo>& knownEnemyBuildings = commanderReference->getKnownEnemyBuildings();

	//for (const BWAPI::Unit unit : knownEnemyUnits)
	//{
	//	if (unit->isFlying())
	//	{
	//		//Build anti air around base
	//	}
	//}

	//for (const auto building : knownEnemyBuildings)
	//{
	//	if (building.first->isFlying())
	//	{
	//		//Build anti air around base
	//	}
	//}
	//#pragma endregion

#pragma region Scout
	if (buildOrderCompleted && frame - frameSinceLastScout >= 200)
	{
		frameSinceLastScout = frame;
		Scout actionToTake;

		action.commanderAction = actionToTake;
		action.type = ActionType::Action_Scout;
		return action;
	}
	#pragma endregion

	#pragma region Building

	//Add building logic here, build tons of gateways and check to make sure we are not building too many upgrades.
	if (buildOrderCompleted && (frame - frameSinceLastBuild) >= 50)
	{
		frameSinceLastBuild = frame;
		const int buildingToBuild = rand() % 100;
		Build actionToTake;
		action.type = Action_Build;

		if (buildingToBuild <= 60)
		{
			actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Gateway;
		}
		else if (buildingToBuild <= 80)
		{
			actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Robotics_Facility;
		}
		else
		{
			actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Stargate;
		}
		action.commanderAction = actionToTake;

		return action;
	}
	

	#pragma endregion


	//StrategyManager::printBoredomMeter();

	return action;
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