#include "StrategyManager.h"
#include "ProtoBotCommander.h"
#include "Squad.h"
#include <BWAPI.h>

int expansionTimes[9] = { 1, 2, 3, 5, 8, 13, 21, 34, 55 };
int mineralsToExpand = 500;
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

StrategyManager::StrategyManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
	StrategyManager::currentState = &StrategyManager::contentState;
}

std::string StrategyManager::onStart()
{
	std::cout << "Strategy Manager Initialized" << '\n';
	//currentState->enter(*this);

	//Test logic to select a random build order that will be passed to the strategy manager. We can take into account the state the bot was in when it lost in later iterations.
	/*std::vector<int> myVector = { 10, 20, 30, 40, 50 };
	const size_t test = myVector.size();
	const int chooseRandBuildOrder = rand() % test;*/

	//Reset Static Variables
	minutesPassedIndex = 0;
	frameSinceLastScout = 0;
	frameSinceLastBuild = 0;
	mineralsToExpand = 1000;

	//Get main base informaiton
	const BWAPI::TilePosition ProtoBot_MainBase = BWAPI::Broodwar->self()->getStartLocation();
	const BWEM::Area* mainArea = theMap.GetArea(ProtoBot_MainBase);

	//Calculate position we will tell squads to wait before attacking if enemy base is unknown or not.
	int shortestDistance = INT_MAX;

	startingChoke = BWAPI::Position(BWEB::Map::getNaturalChoke()->Center());


	/*for (auto area : mainArea->AccessibleNeighbours())
	{
		for (auto choke : area->ChokePoints())
		{
			const std::pair<const BWEM::Area*, const BWEM::Area*> chokeAreas = choke->GetAreas();

			if (chokeAreas.first->Id() == mainArea->Id() || chokeAreas.second->Id() == mainArea->Id()) continue;

			int distance = 0;
			const BWEM::CPPath pathToChoke = theMap.GetPath(BWAPI::Position(ProtoBot_MainBase), BWAPI::Position(choke->Center()), &distance);

			if (distance == -1) continue;

			std::cout << "Path distance: " << (distance) << "\n";

			if (distance < shortestDistance)
			{
				shortestDistance = distance;
				startingChoke = BWAPI::Position(choke->Center());
			}
		}
	}*/

	std::cout << "Starting choke located at " << startingChoke << "\n";

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

	BWAPI::Broodwar->drawCircleMap(startingChoke, 5, BWAPI::Colors::Blue, true);

	//In-Game Time book keeping
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / FRAMES_PER_SECOND;

	//Supply threshold is how close we want to get to the supply cap before building a pylon to cover supply costs.
	//Make supply threshold early game by default.
	//Modify these times
	int supplyThreshold = SUPPLY_THRESHOLD_EARLYGAME;

	if ((seconds / 60) >= 5 && (seconds / 60) < 15)
	{
		supplyThreshold = SUPPLY_THRESHOLD_MIDGAME;
	}
	else if ((seconds / 60) >= 15)
	{
		supplyThreshold = SUPPLY_THRESHOLD_LATEGAME;
	}

	//Building logic
	const bool buildOrderCompleted = commanderReference->buildOrderCompleted();

	//ProtoBot unit information
	const FriendlyBuildingCounter ProtoBot_buildings = commanderReference->informationManager.getFriendlyBuildingCounter();
	const FriendlyUnitCounter ProtoBot_units = commanderReference->informationManager.getFriendlyUnitCounter();
	const FriendlyUpgradeCounter ProtoBot_upgrade = commanderReference->informationManager.getFriendlyUpgradeCounter();
	const FriendlyTechCounter ProtoBot_tech = commanderReference->informationManager.getFriendlyTechCounter();
	std::vector<Squad*> Protobot_IdleSquads = commanderReference->combatManager.IdleSquads;
	std::vector<Squad*> Protobot_Squads = commanderReference->combatManager.Squads;

	//Get Enemy Building information.
	const std::map<BWAPI::Unit, EnemyBuildingInfo>& enemyBuildingInfo = commanderReference->informationManager.getKnownEnemyBuildings();

	//Check how many of our Nexus Economies are completed and saturated.
	std::vector<NexusEconomy> nexusEconomies = commanderReference->getNexusEconomies();
	int completedNexusEconomy = 0;
	int saturatedNexus = 0;

	for (NexusEconomy nexusEconomy : nexusEconomies)
	{
		/*
		* Nexus Economy considered complete if
		*  - Nexus Economy has no gyser to farm and has a worker assigned to every mineral
		*  - Nexus Economy HAS a gyser to farm and has assimilator assigned (no need to check worker size since nexus economy builds assimialtor at > mineral.size())
		*/
		if (nexusEconomy.lifetime < 500) continue;

		if ((nexusEconomy.vespeneGyser != nullptr && (nexusEconomy.assimilator != nullptr && nexusEconomy.assimilator->isCompleted())) ||
			(nexusEconomy.vespeneGyser == nullptr && nexusEconomy.workers.size() >= nexusEconomy.minerals.size()))
		{
			completedNexusEconomy++;
		}
	}

	//This is the normal formula we would use for calculting saturation but we will focus on purely gateways
	//saturatedNexus = (ProtoBot_buildings.gateway / 4) + ((ProtoBot_buildings.gateway / 2) + ProtoBot_buildings.stargate) + ((ProtoBot_buildings.gateway / 2) + ProtoBot_buildings.roboticsFacility);

	//4 Gateways per nexus should be built
	saturatedNexus = (ProtoBot_buildings.gateway / 2);

	std::vector<BWAPI::Position> enemyBaselocations;
	for (const auto [unit, building] : enemyBuildingInfo)
	{
		if (building.type.isResourceDepot())
		{
			//std::cout << building.type << " at position " << building.lastKnownPosition << "\n";

			BWAPI::Broodwar->drawCircleMap(building.lastKnownPosition, 5, BWAPI::Colors::Red, true);

			enemyBaselocations.push_back(building.lastKnownPosition);
		}
	}

	//Move this to inside if so we dont scout during build order unless instructed.
//#pragma region Scout
//	if (frame - frameSinceLastScout >= 24 * 20) {
//		frameSinceLastScout = frame;
//		Scout s;
//		action.commanderAction = s;
//		action.type = ActionType::Action_Scout;
//		return action;
//	}
//#pragma endregion

#pragma region Expand
	if (buildOrderCompleted)
	{
		//Check if we should build a pylon, Change this to be a higher value than 3 as the game goes along.
		//We are not making pylons in advance quick enough
		if (commanderReference->checkAvailableSupply() <= supplyThreshold && ((BWAPI::Broodwar->self()->supplyTotal() / 2) != 200))
		{
			//std::cout << "EXPAND ACTION: Requesting to build Pylon\n";
			Expand actionToTake;
			actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Pylon;

			action.commanderAction = actionToTake;
			action.type = ActionType::Action_Expand;
			return action;
		}

		//If a nexus economy has an gyser to place an assimlator at, place one when we have more than: numbers of minerals + 3 workers.
		for (const NexusEconomy& nexusEconomy : nexusEconomies)
		{
			if (nexusEconomy.vespeneGyser != nullptr
				&& nexusEconomy.assimilator == nullptr
				&& nexusEconomy.workers.size() >= nexusEconomy.minerals.size() + 3
				&& nexusEconomy.lifetime >= 500
				&& checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Assimilator))
			{
				//std::cout << "EXPAND ACTION: Checking nexus economy " << nexusEconomy.nexusID << " needs assimilator\n";
				Expand actionToTake;
				actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Assimilator;

				action.commanderAction = actionToTake;
				action.type = ActionType::Action_Expand;
				return action;
			}
		}

		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Nexus))
		{
			//Not expanding properlly after having enough gateways
			if (ProtoBot_buildings.nexus == saturatedNexus)
			{
				mineralsToExpand *= 2.5;
				std::cout << "EXPAND ACTION: Requesting to expand (4 gateways saturating nexus)\n";

				Expand actionToTake;
				actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Nexus;

				action.commanderAction = actionToTake;
				action.type = ActionType::Action_Expand;
				return action;
			}

			/*if (BWAPI::Broodwar->self()->minerals() > mineralsToExpand)
			{
				mineralsToExpand * 2.5;
				std::cout << "EXPAND ACTION: Requesting to expand (mineral surplus)\n";

				Expand actionToTake;
				actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Nexus;

				action.commanderAction = actionToTake;
				action.type = ActionType::Action_Expand;
				return action;
			}*/
			/*else if (minutesPassedIndex < sizeof(expansionTimes) / sizeof(expansionTimes[0])
				&& expansionTimes[minutesPassedIndex] <= (seconds / 60))
			{
				std::cout << "EXPAND ACTION: Requesting to expand (expansion time " << expansionTimes[minutesPassedIndex] << ")\n";

				minutesPassedIndex++;

				Expand actionToTake;
				actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Nexus;

				action.commanderAction = actionToTake;
				action.type = ActionType::Action_Expand;
				return action;
			}*/
		}
	}
#pragma endregion

#pragma region Build
	if (buildOrderCompleted)
	{
		//Only create 4 gateways per completed nexus economy or 2 gateway and 1 robotics facility.
		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Gateway) && completedNexusEconomy >= 1)
		{
			//std::cout << "Number of \"completed\" Nexus Economies = " << completedNexusEconomy << "\n";

			//4 Gateways per nexus economy
			if (ProtoBot_buildings.gateway < ProtoBot_buildings.nexus * 4)
			{
				//std::cout << "BUILD ACTION: Requesting to warp Gateway\n";
				Build actionToTake;
				actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Gateway;

				action.commanderAction = actionToTake;
				action.type = ActionType::Action_Build;
				return action;
			}
		}

		//Focus on gateways until one is built till we even consider building other units.
		if (ProtoBot_buildings.gateway < 1) return action;

		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Forge) && ProtoBot_buildings.forge < 1)
		{
			//std::cout << "BUILD ACTION: Requesting to warp Forge\n";
			Build actionToTake;
			actionToTake.unitToBuild = BWAPI::UnitTypes::Protoss_Forge;

			action.commanderAction = actionToTake;
			action.type = ActionType::Action_Build;
			return action;
		}

		if (ProtoBot_buildings.gateway < 2) return action;

		//removing stargares for now.
		/*if (checkalreadyrequested(bwapi::unittypes::protoss_stargate) && cyberneticscount == 1 && stargatecount != (gatewaycount / 2))
		{
			std::cout << "build action: requesting to stargate\n";
			build actiontotake;
			actiontotake.unittobuild = bwapi::unittypes::protoss_stargate;

			action.commanderaction = actiontotake;
			action.type = actiontype::action_build;
			return action;
		}*/

		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Cybernetics_Core) && ProtoBot_buildings.cyberneticsCore < 1 && ProtoBot_buildings.gateway >= 1)
		{
			//std::cout << "build action: requesting to warp forge\n";
			Build actiontotake;
			actiontotake.unitToBuild = BWAPI::UnitTypes::Protoss_Cybernetics_Core;

			action.commanderAction = actiontotake;
			action.type = ActionType::Action_Build;
			return action;
		}

		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Robotics_Facility) && ProtoBot_buildings.roboticsFacility < 1 && ProtoBot_buildings.cyberneticsCore == 1)
		{
			//std::cout << "build action: requesting to warp robotics facility\n";
			Build actiontotake;
			actiontotake.unitToBuild = BWAPI::UnitTypes::Protoss_Robotics_Facility;

			action.commanderAction = actiontotake;
			action.type = ActionType::Action_Build;
			return action;
		}

		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Observatory) && ProtoBot_buildings.observatory < 1 && ProtoBot_buildings.roboticsFacility == 1)
		{
			//std::cout << "Build Action: requesting to warp observatory\n";
			Build actiontotake;
			actiontotake.unitToBuild = BWAPI::UnitTypes::Protoss_Observatory;

			action.commanderAction = actiontotake;
			action.type = ActionType::Action_Build;
			return action;
		}

		//Will focus on Zealots, Dragoons, and Observers for first iteration of BASIL upload.
		/*if (checkalreadyrequested(bwapi::unittypes::protoss_citadel_of_adun) && citadelcount < 3 && cyberneticscount == 1 && citadelcount != completednexuseconomy)
		{
			std::cout << "build action: requesting to warp citadel\n";
			build actiontotake;
			actiontotake.unittobuild = bwapi::unittypes::protoss_citadel_of_adun;

			action.commanderaction = actiontotake;
			action.type = actiontype::action_build;
			return action;
		}

		if (checkalreadyrequested(bwapi::unittypes::protoss_templar_archives) && templararchivescount < 3 && citadelcount == 1 && templararchivescount != completednexuseconomy)
		{
			std::cout << "build action: requesting to warp archives\n";
			build actiontotake;
			actiontotake.unittobuild = bwapi::unittypes::protoss_templar_archives;

			action.commanderaction = actiontotake;
			action.type = actiontype::action_build;
			return action;
		}*/
	}
#pragma endregion

//#pragma region Attack
//	//If we have more than two full squads attack. 
//	if (Protobot_Squads.size() >= 2 && enemyBaselocations.size() != 0)
//	{
//		if (commanderReference->combatManager.totalCombatUnits.size() >= (MAX_SQUAD_SIZE * 2))
//		{
//			std::cout << "ATTACK ACTION: Attacking enemy base\n";
//			Attack actionToTake;
//			//Attack the first enemy base location for now.
//			actionToTake.position = enemyBaselocations.at(0);
//
//			action.commanderAction = actionToTake;
//			action.type = ActionType::Action_Attack;
//			return action;
//		}
//	}
//#pragma endregion
//
//#pragma region Defend
//	if (Protobot_IdleSquads.size() != 0)
//	{
//		std::cout << "Defend Action: telling squad to defend base.\n";
//
//		Defend actionToTake;
//		actionToTake.position = startingChoke;
//		
//		action.commanderAction = actionToTake;
//		action.type = ActionType::Action_Defend;
//		return action;
//	}
//#pragma endregion


	//StrategyManager::printBoredomMeter();

	return action;
}

//More logic to this later
bool StrategyManager::shouldGasSteal()
{
	return true;
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

void StrategyManager::onUnitCreate(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	const FriendlyBuildingCounter ProtoBot_buildings = commanderReference->informationManager.getFriendlyBuildingCounter();

	//Calculaute new location for squads to sit while we wait to attack.
	//Helps prevent us from flooding our inside base with units.

	/*
	int shortestDistance = INT_MAX;

	for (auto area : mainArea->AccessibleNeighbours())
	{
		for (auto choke : area->ChokePoints())
		{
			const std::pair<const BWEM::Area*, const BWEM::Area*> chokeAreas = choke->GetAreas();

			if (chokeAreas.first->Id() == mainArea->Id() || chokeAreas.second->Id() == mainArea->Id()) continue;

			int distance = 0;
			const BWEM::CPPath pathToChoke = theMap.GetPath(BWAPI::Position(ProtoBot_MainBase), BWAPI::Position(choke->Center()), &distance);

			if (distance == -1) continue;

			std::cout << "Path distance: " << (distance) << "\n";

			if (distance < shortestDistance)
			{
				shortestDistance = distance;
				startingChoke = BWAPI::Position(choke->Center());
			}
		}
	}
	*/

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