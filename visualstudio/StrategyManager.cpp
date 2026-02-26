#include "StrategyManager.h"
#include "ProtoBotCommander.h"
#include "Squad.h"
#include <BWAPI.h>

StrategyManager::StrategyManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
	
}

void StrategyManager::onStart()
{
	std::cout << "Strategy Manager Initialized" << '\n';

	//Reset Static Variables
	minutesPassedIndex = 0;
	frameSinceLastScout = 0;
	mineralExcessToExpand = 1000;
	ProtoBot_Areas.clear();
	ProtoBotArea_SquadPlacements.clear();
	PositionsFilled.clear();

	//For squad placments we will cover areas we own and have the wait at choke points of psuedo areas we "own".
	const BWAPI::TilePosition ProtoBot_MainBase = BWAPI::Broodwar->self()->getStartLocation();
	mainArea = theMap.GetNearestArea(ProtoBot_MainBase);
	ProtoBot_Areas.insert(mainArea);

	for (const BWEM::Area* neighbor : mainArea->AccessibleNeighbours())
	{
		ProtoBot_Areas.insert(neighbor);

		for (const BWEM::Area* neighbors_neighbor : neighbor->AccessibleNeighbours())
		{
			ProtoBot_Areas.insert(neighbors_neighbor);
		}
	}

	for (const BWEM::Area* area : ProtoBot_Areas)
	{
		for (const BWEM::ChokePoint* choke : area->ChokePoints())
		{
			const std::pair<const BWEM::Area*, const BWEM::Area*> areas = choke->GetAreas();
			
			//Ignore choke that is on ramp to prevent builders from being able to construct.
			if (areas.first == mainArea || areas.second == mainArea) continue;

			ProtoBotArea_SquadPlacements.insert(choke);
			PositionsFilled.insert({ choke, false });
		}
	}

	std::cout << "ProtoBot areas: " << ProtoBot_Areas.size() << "\n";

	startingChoke = BWAPI::Position(BWEB::Map::getNaturalChoke()->Center());

	std::cout << "Starting choke located at " << startingChoke << "\n";	
}

bool StrategyManager::checkAlreadyRequested(BWAPI::UnitType type)
{
	return (!commanderReference->requestedBuilding(type)
		&& !(commanderReference->checkUnitIsBeingWarpedIn(type)
			|| commanderReference->checkUnitIsPlanned(type)));
}

std::vector<Action> StrategyManager::onFrame()
{
	std::vector<Action> actionsToReturn;

	//Debug: Drawing choke points to get an idea on where the BWEM can have us position squads
	for (const BWEM::ChokePoint* choke : ProtoBotArea_SquadPlacements)
	{
		if (PositionsFilled[choke])
		{
			BWAPI::Broodwar->drawCircleMap(BWAPI::Position(choke->Center()), 10, BWAPI::Broodwar->self()->getColor(), true);
		}
		else
		{
			BWAPI::Broodwar->drawCircleMap(BWAPI::Position(choke->Center()), 10, BWAPI::Colors::Red, true);
		}
		
	}

	//BWAPI::Broodwar->drawCircleMap(startingChoke, 5, BWAPI::Colors::Red, true);

	//In-Game Time book keeping
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / FRAMES_PER_SECOND;
	const int minutes = seconds / 60;

	//Building logic
	const bool buildOrderCompleted = commanderReference->buildOrderCompleted();

	if (!(minutesPassedIndex == expansionTimes.size()) && expansionTimes.at(minutesPassedIndex) <= minutes && !buildOrderCompleted) minutesPassedIndex++;

	//ProtoBot unit information
	const FriendlyBuildingCounter ProtoBot_buildings = commanderReference->informationManager.getFriendlyBuildingCounter();
	const FriendlyUnitCounter ProtoBot_units = commanderReference->informationManager.getFriendlyUnitCounter();
	const FriendlyUpgradeCounter ProtoBot_upgrade = commanderReference->informationManager.getFriendlyUpgradeCounter();
	const FriendlyTechCounter ProtoBot_tech = commanderReference->informationManager.getFriendlyTechCounter();
	std::vector<Squad*> Protobot_IdleSquads = commanderReference->combatManager.IdleSquads;
	std::vector<Squad*> Protobot_Squads = commanderReference->combatManager.Squads;
	int numberFullSquads = 0;

	for (const Squad* squad : Protobot_Squads)
	{
		if (squad->units.size() == MAX_SQUAD_SIZE) numberFullSquads++;
	}

	//Supply threshold is how close we want to get to the supply cap before building a pylon to cover supply costs.
	int supplyThreshold = SUPPLY_THRESHOLD_EARLYGAME;

	if ((seconds / 60) >= MIDGAME_TIME && (seconds / 60) < LATEGAME_TIME)
	{
		supplyThreshold = SUPPLY_THRESHOLD_MIDGAME;
	}
	else if ((seconds / 60) >= LATEGAME_TIME)
	{
		supplyThreshold = SUPPLY_THRESHOLD_LATEGAME;
	}

	const int dynamicSupplyThreshold = supplyThreshold + (ProtoBot_buildings.gateway * 2);

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
	saturatedNexus = (ProtoBot_buildings.gateway / 4);

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
#pragma region Scout

	/*if (buildOrderCompleted)
	{
		//change this to frames since last info gained
		if (frame - frameSinceLastScout >= 24 * 20) {
			frameSinceLastScout = frame;

			Action scout;
			scout.type = Action::ACTION_SCOUT;
			actionsToReturn.push_back(scout);
		}
	}*/
	
#pragma endregion

#pragma region Expand
	if (buildOrderCompleted)
	{
		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Nexus))
		{
			//Not expanding properlly after having enough gateways
			if (ProtoBot_buildings.nexus == saturatedNexus)
			{
				std::cout << "EXPAND ACTION: Requesting to expand (4 gateways saturating nexus)\n";
				Action expand;
				expand.type = Action::ACTION_EXPAND;
				expand.expansionToConstruct = BWAPI::UnitTypes::Protoss_Nexus;
				
				actionsToReturn.push_back(expand);
			}

			if (BWAPI::Broodwar->self()->minerals() > mineralExcessToExpand)
			{
				mineralExcessToExpand *= 2;
				std::cout << "EXPAND ACTION: Requesting to expand (mineral surplus)\n";

				Action expand;
				expand.type = Action::ACTION_EXPAND;
				expand.expansionToConstruct = BWAPI::UnitTypes::Protoss_Nexus;

				actionsToReturn.push_back(expand);
			}
			
			if (!(minutesPassedIndex == expansionTimes.size()) && expansionTimes.at(minutesPassedIndex) <= minutes)
			{
				std::cout << "EXPAND ACTION: Requesting to expand (expansion time " << expansionTimes.at(minutesPassedIndex) << ")\n";
				minutesPassedIndex++;

				Action expand;
				expand.type = Action::ACTION_EXPAND;
				expand.expansionToConstruct = BWAPI::UnitTypes::Protoss_Nexus;

				actionsToReturn.push_back(expand);
			}
		}
	}
#pragma endregion

#pragma region Build
	//Will focus on Zealots, Dragoons, and Observers for first iteration of BASIL upload.
	if (buildOrderCompleted)
	{
		//Check if we should build a pylon, Change this to be a higher value than 3 as the game goes along.
		//We are not making pylons in advance quick enough
		if (commanderReference->checkAvailableSupply() <= dynamicSupplyThreshold && ((BWAPI::Broodwar->self()->supplyTotal() / 2) != MAX_SUPPLY))
		{
			//std::cout << "EXPAND ACTION: Requesting to build Pylon\n";
			Action action;
			action.type = Action::ACTION_BUILD;
			action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Pylon;

			actionsToReturn.push_back(action);
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
				Action action;
				action.type = Action::ACTION_BUILD;
				action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Assimilator;

				actionsToReturn.push_back(action);
			}
		}

		//Only create 4 gateways per completed nexus economy or 2 gateway and 1 robotics facility.
		if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Gateway) && ProtoBot_buildings.pylon >= 1)
		{
			//std::cout << "Number of \"completed\" Nexus Economies = " << completedNexusEconomy << "\n";

			//4 Gateways per nexus economy
			if (ProtoBot_buildings.gateway < ProtoBot_buildings.nexus * 4)
			{
				//std::cout << "BUILD ACTION: Requesting to warp Gateway\n";
				Action action;
				action.type = Action::ACTION_BUILD;
				action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Gateway;

				actionsToReturn.push_back(action);
			}
		}
		
		//1 forge for now but if we want upgrades 2/3 for armor and weapons we need to know when to go some of these buildings.
		/*if (checkTechTree(BWAPI::UnitTypes::Protoss_Forge, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Forge) && ProtoBot_buildings.forge < 1 && (ProtoBot_buildings.gateway >= 1))
		{
			std::cout << "BUILD ACTION: Requesting to warp Forge\n";
			Action action;
			action.type = Action::ACTION_BUILD;
			action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Forge;

			actionsToReturn.push_back(action);
		}*/

		//Only need 1 cybernetics core
		if (checkTechTree(BWAPI::UnitTypes::Protoss_Cybernetics_Core, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Cybernetics_Core) && ProtoBot_buildings.cyberneticsCore < 1 && ProtoBot_buildings.gateway >= 1)
		{
			std::cout << "build action: requesting to warp forge\n";
			Action action;
			action.type = Action::ACTION_BUILD;
			action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Cybernetics_Core;

			actionsToReturn.push_back(action);
		}

		//Should only build 1 robotics facility
		/*if (checkTechTree(BWAPI::UnitTypes::Protoss_Robotics_Facility, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Robotics_Facility) && ProtoBot_buildings.roboticsFacility < 1 && ProtoBot_buildings.cyberneticsCore >= 1)
		{
			std::cout << "build action: requesting to warp robotics facility\n";
			Action action;
			action.type = Action::ACTION_BUILD;
			action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Robotics_Facility;

			actionsToReturn.push_back(action);
		}*/

		//Only 1 observatory
		/*if (checkTechTree(BWAPI::UnitTypes::Protoss_Observatory, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Observatory) && ProtoBot_buildings.observatory < 1 && ProtoBot_buildings.roboticsFacility >= 1)
		{
			std::cout << "Build Action: requesting to warp observatory\n";
			Action action;
			action.type = Action::ACTION_BUILD;
			action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Observatory;

			actionsToReturn.push_back(action);
		}*/

		//2/3 Forge upgrade units

		/*if (checkTechTree(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) && ProtoBot_buildings.citadelOfAdun < 1 && ProtoBot_buildings.cyberneticsCore >= 1)
		{
			std::cout << "build action: requesting to warp citadel of adun\n";
			Action action;
			action.type = Action::ACTION_BUILD;
			action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Citadel_of_Adun;

			actionsToReturn.push_back(action);
		}

		if (checkTechTree(BWAPI::UnitTypes::Protoss_Templar_Archives, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Templar_Archives) && ProtoBot_buildings.templarArchives < 1 && ProtoBot_buildings.citadelOfAdun >= 1)
		{
			std::cout << "build action: requesting to warp templar archives\n";
			Action action;
			action.type = Action::ACTION_BUILD;
			action.buildingToConstruct = BWAPI::UnitTypes::Protoss_Templar_Archives;

			actionsToReturn.push_back(action);
		}*/
	}
#pragma endregion
/*
#pragma region Attack
	// If we have more than two full squads attack. 
	if (Protobot_Squads.size() >= 2 && enemyBaselocations.size() != 0)
	{
		if (commanderReference->combatManager.totalCombatUnits.size() >= (MAX_SQUAD_SIZE * 2))
		{
			//std::cout << "ATTACK ACTION: Attacking enemy base\n";
			Action attack;
			attack.type = Action::ACTION_ATTACK;
			attack.attackPosition = enemyBaselocations.at(0);

			actionsToReturn.push_back(attack);
		}
	}
#pragma endregion
*/

#pragma region Defend
	BWAPI::Unitset unitsOnVisison = BWAPI::Broodwar->getAllUnits();
	BWAPI::Unitset enemyCombatUnits;

	for (BWAPI::Unit unit : unitsOnVisison)
	{
		if (unit->getPlayer() == BWAPI::Broodwar->enemy() && !unit->getType().isBuilding())
		{
			enemyCombatUnits.insert(unit);
		}
	}

	bool enemyAttacking = false;
	BWAPI::Unit unitToAttack;

	for (const BWAPI::Unit unit : enemyCombatUnits)
	{
		unitToAttack = unit;
		const BWEM::Area* enemyAreaLocation = theMap.GetNearestArea(unit->getTilePosition());
		
		for (const BWEM::Area* area : ProtoBot_Areas)
		{
			if (enemyAreaLocation == area) enemyAttacking = true;
		}
	}

	if (enemyAttacking)
	{
		Action attack;
		attack.type = Action::ACTION_ATTACK;
		attack.attackPosition = unitToAttack->getPosition();

		actionsToReturn.push_back(attack);
		
		//Remove all squads from placement
		for (const BWEM::ChokePoint* placement : ProtoBotArea_SquadPlacements)
		{
			PositionsFilled[placement] = false;
		}
	}
	else
	{
		if (Protobot_IdleSquads.size() != 0)
		{
			int distanceToClosestChoke = INT_MAX;
			BWAPI::WalkPosition areaToDefend = BWAPI::WalkPositions::Invalid;
			const BWEM::ChokePoint* chokeToDefend;

			for (const BWEM::ChokePoint* placement : ProtoBotArea_SquadPlacements)
			{
				if (PositionsFilled[placement] == false)
				{
					int distanceToChoke = 0;
					const BWEM::CPPath pathToExpansion = theMap.GetPath(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), BWAPI::Position(placement->Center()), &distanceToChoke);

					if (distanceToChoke == -1) continue;

					if (distanceToChoke < distanceToClosestChoke)
					{
						distanceToClosestChoke = distanceToChoke;
						areaToDefend = placement->Center();
						chokeToDefend = placement;
					}
				}
			}

			if (areaToDefend != BWAPI::WalkPositions::Invalid)
			{
				Action defend;
				defend.type = Action::ACTION_DEFEND;
				defend.defendPosition = BWAPI::Position(areaToDefend);

				actionsToReturn.push_back(defend);
				PositionsFilled[chokeToDefend] = true;
			}
		}
	}
#pragma endregion

	return actionsToReturn;
}

//More logic to this later
bool StrategyManager::shouldGasSteal()
{
	return false;
}

bool StrategyManager::checkTechTree(BWAPI::UnitType unit, FriendlyBuildingCounter ProtoBot_Buildings)
{
	const std::map<BWAPI::UnitType, int> requiredBuildings = unit.requiredUnits();
	bool hasRequiredTech = true;

	for (auto requiredBuilding : requiredBuildings)
	{
		switch (requiredBuilding.first)
		{
			case BWAPI::UnitTypes::Protoss_Nexus:
				if (ProtoBot_Buildings.nexus < requiredBuilding.second) hasRequiredTech = false;
				break;

			case BWAPI::UnitTypes::Protoss_Gateway:
				if (ProtoBot_Buildings.gateway < requiredBuilding.second) hasRequiredTech = false;
				break;

			case BWAPI::UnitTypes::Protoss_Forge:
				if (ProtoBot_Buildings.forge < requiredBuilding.second) hasRequiredTech = false;
				break;

			case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
				if (ProtoBot_Buildings.cyberneticsCore < requiredBuilding.second) hasRequiredTech = false;
				break;

			case BWAPI::UnitTypes::Protoss_Robotics_Facility:
				if (ProtoBot_Buildings.roboticsFacility < requiredBuilding.second) hasRequiredTech = false;
				break;

			case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
				if (ProtoBot_Buildings.citadelOfAdun < requiredBuilding.second) hasRequiredTech = false;
				break;

			case BWAPI::UnitTypes::Protoss_Templar_Archives:
				if (ProtoBot_Buildings.templarArchives < requiredBuilding.second) hasRequiredTech = false;
				break;

			case BWAPI::UnitTypes::Protoss_Stargate:
				if (ProtoBot_Buildings.stargate < requiredBuilding.second) hasRequiredTech = false;
				break;

		}
	}


	return hasRequiredTech;
}

void StrategyManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus)
	{
		std::unordered_set<const BWEM::Area*> areasToRemove;
		std::unordered_set<const BWEM::Area*> areasStillOwned;

		const BWAPI::Position ProtoBot_DestroyedBase = unit->getPosition();
		const BWEM::Area* oldArea = theMap.GetNearestArea(BWAPI::TilePosition(ProtoBot_DestroyedBase));

		//Find potential areas we need to remove
		areasToRemove.insert(oldArea);
		for (const BWEM::Area* neighbor : oldArea->AccessibleNeighbours())
		{
			areasToRemove.insert(neighbor);

			for (const BWEM::Area* neighbors_neighbor : neighbor->AccessibleNeighbours())
			{
				areasToRemove.insert(neighbors_neighbor);
			}
		}

		//Get the areas we still own.
		ProtoBot_Areas.erase(oldArea);
		for (const BWEM::Area* area : ProtoBot_Areas)
		{
			areasStillOwned.insert(area);

			for (const BWEM::Area* neighbor : area->AccessibleNeighbours())
			{
				areasStillOwned.insert(neighbor);

				for (const BWEM::Area* neighbors_neighbor : neighbor->AccessibleNeighbours())
				{
					areasStillOwned.insert(neighbors_neighbor);
				}
			}
		}

		//Do a remove elements that we still dont own and are in the areasToRemove set.
		for (const BWEM::Area* area : areasToRemove)
		{
			if (areasStillOwned.find(area) == areasStillOwned.end()) ProtoBot_Areas.erase(area);
		}

		//Update choke points.
		for (const BWEM::Area* area : ProtoBot_Areas)
		{
			for (const BWEM::ChokePoint* choke : area->ChokePoints())
			{
				const std::pair<const BWEM::Area*, const BWEM::Area*> areas = choke->GetAreas();

				//Ignore choke that is on ramp to prevent builders from being able to construct.
				if (areas.first == mainArea || areas.second == mainArea) continue;

				ProtoBotArea_SquadPlacements.insert(choke);
			}
		}

	}
}

void StrategyManager::onUnitCreate(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus)
	{
		//Add new area to areas we occupy.
		const BWAPI::Position ProtoBot_Base = unit->getPosition();
		const BWEM::Area* newArea = theMap.GetNearestArea(BWAPI::TilePosition(ProtoBot_Base));
		ProtoBot_Areas.insert(newArea);

		for (const BWEM::Area* neighbor : newArea->AccessibleNeighbours())
		{
			ProtoBot_Areas.insert(neighbor);

			for (const BWEM::Area* neighbors_neighbor : neighbor->AccessibleNeighbours())
			{
				ProtoBot_Areas.insert(neighbors_neighbor);
			}
		}

		//Add new choke point locations (Temp solution for now).
		for (const BWEM::Area* area : ProtoBot_Areas)
		{
			for (const BWEM::ChokePoint* choke : area->ChokePoints())
			{
				const std::pair<const BWEM::Area*, const BWEM::Area*> areas = choke->GetAreas();

				//Ignore choke that is on ramp to prevent builders from being able to construct.
				if (areas.first == mainArea || areas.second == mainArea) continue;

				ProtoBotArea_SquadPlacements.insert(choke);
				PositionsFilled.insert({ choke, false });
			}
		}
	}

}

void StrategyManager::onUnitComplete(BWAPI::Unit unit)
{

}