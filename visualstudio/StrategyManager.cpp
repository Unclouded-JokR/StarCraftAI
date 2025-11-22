#include "StrategyManager.h"
#include "ProtoBotCommander.h"
#include <BWAPI.h>

int expansionTimes[8] = { 2, 3, 5, 8, 13, 21, 34, 55};
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

int mineralsToExpand = 500;

StrategyManager::StrategyManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
	StrategyManager::currentState = &StrategyManager::contentState;
}

std::string StrategyManager::onStart()
{
	std::cout << "StrategyManager is a go!" << '\n';
	currentState->enter(*this);

	//Test logic to select a random build order that will be passed to the strategy manager. We can take into account the state the bot was in when it lost in later iterations.
	std::vector<int> myVector = { 10, 20, 30, 40, 50 };
	const size_t test = myVector.size();
	const int chooseRandBuildOrder = rand() % test;

	//Reset Static Variables
	minutesPassedIndex = 0;
	frameSinceLastScout = 0;
	frameSinceLastBuild = 0;


	//return empty string
	return "";
}

bool StrategyManager::checkAlreadyRequested(BWAPI::UnitType type)
{
	return (!commanderReference->requestedBuilding(type)
		&& !(commanderReference->checkUnitIsBeingWarpedIn(type)
			|| commanderReference->checkUnitIsPlanned(type)));
}

Action StrategyManager::onFrame()
{
	None none;
	Action action;
	action.type = ActionType::Action_None;

	// time bookkeeping
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / FRAMES_PER_SECOND;

	// from here on, build logic etc.
	const int supplyUsed = (BWAPI::Broodwar->self()->supplyUsed()) / 2;
	const int totalSupply = (BWAPI::Broodwar->self()->supplyTotal()) / 2;
	const bool buildOrderCompleted = commanderReference->buildOrderCompleted();

	#pragma region Expand
	if (buildOrderCompleted)
	{
		//Check if we should build a pylon
		if (supplyUsed + 2 >= totalSupply && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Pylon))
		{
			Expand actionToTake;
			actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Pylon;

			action.commanderAction = actionToTake;
			action.type = ActionType::Action_Expand;
			return action;
		}

		//Check for assimilators on nexus economies here.
		std::vector<NexusEconomy> nexusEconomies = commanderReference->getNexusEconomies();
		for (const NexusEconomy& nexusEconomy : nexusEconomies)
		{
			if (nexusEconomy.vespeneGyser != nullptr
				&& nexusEconomy.assimilator == nullptr
				&& nexusEconomy.workers.size() >= nexusEconomy.minerals.size() + 3 
				&& nexusEconomy.lifetime >= 500
				&& checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Assimilator))
			{
				std::cout << "Checking nexus economy " << nexusEconomy.nexusID << " needs assimilator\n";
				Expand actionToTake;
				actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Assimilator;

				action.commanderAction = actionToTake;
				action.type = ActionType::Action_Expand;
				return action;
			}
		}

		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Nexus))
		{
			//std::cout << (expansionTimes[minutesPassedIndex]) << " >= " << (seconds / 60) << "\n";

			if (BWAPI::Broodwar->self()->minerals() > 3000)
			{
				std::cout << "Requesting to expand (mineral surplus)\n";

				Expand actionToTake;
				actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Nexus;

				action.commanderAction = actionToTake;
				action.type = ActionType::Action_Expand;
				return action;
			}
			else if (minutesPassedIndex < sizeof(expansionTimes) / sizeof(expansionTimes[0])
				&& expansionTimes[minutesPassedIndex] <= (seconds / 60))
			{
				std::cout << "Requesting to expand (expansion time " << expansionTimes[minutesPassedIndex] << ")\n";

				minutesPassedIndex++;

				Expand actionToTake;
				actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Nexus;

				action.commanderAction = actionToTake;
				action.type = ActionType::Action_Expand;
				return action;
			}
		}
	}
	#pragma endregion

	#pragma region Build


	#pragma endregion

	#pragma region Scout
	if (buildOrderCompleted)
	{
		if (frame - frameSinceLastScout >= 24 * 20) { // every ~20s;
		frameSinceLastScout = frame;
		Scout s;
		action.commanderAction = s;
		action.type = ActionType::Action_Scout;
		return action;                 // <-- ensure we actually send the action
		}
	}
	#pragma endregion

		//#pragma region Building

		////Add building logic here, build tons of gateways and check to make sure we are not building too many upgrades.
		//if (buildOrderCompleted && (frame - frameSinceLastBuild) >= 50)
		//{
		//	frameSinceLastBuild = frame;
		//	const int buildingToBuild = rand() % 100;
		//	Build actionToTake;
		//	action.type = Action_Build;

		//	if (buildingToBuild <= 60)
		//	{
		//		actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Gateway;
		//	}
		//	else if (buildingToBuild <= 80)
		//	{
		//		actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Robotics_Facility;
		//	}
		//	else
		//	{
		//		actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Stargate;
		//	}
		//	action.commanderAction = actionToTake;

		//	return action;
		//}
		//

		//#pragma endregion


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
		//std::cout << "Our unit died" << std::endl;
		StrategyManager::angerMeter += StrategyManager::angerFromUnitDeath;
	}
	else if (owner == BWAPI::Broodwar->enemy() && ((unitType.isWorker() || unitType.isBuilding())))
	{
		//std::cout << "Enemy unit died" << std::endl;
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