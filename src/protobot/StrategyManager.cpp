#include "StrategyManager.h"
#include "ProtoBotCommander.h"
#include "Squad.h"
#include <BWAPI.h>

StrategyManager::StrategyManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

void StrategyManager::onStart()
{
	//std::cout << "Strategy Manager Initialized" << '\n';

	//Reset Static Variables
	minutesPassedIndex = 0;
	frameSinceLastScout = 0;
	ProductionGoal_index = 0;
	timer = 0;
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
		if (neighbor == nullptr) continue;

		ProtoBot_Areas.insert(neighbor);

		for (const BWEM::Area* neighbors_neighbor : neighbor->AccessibleNeighbours())
		{
			ProtoBot_Areas.insert(neighbors_neighbor);
		}
	}

	for (const BWEM::Area* area : ProtoBot_Areas)
	{
		if (area == nullptr) continue;

		for (const BWEM::ChokePoint* choke : area->ChokePoints())
		{
			if (choke == nullptr) continue;

			const std::pair<const BWEM::Area*, const BWEM::Area*> areas = choke->GetAreas();

			//Ignore choke that is on ramp to prevent builders from being able to construct.
			if (areas.first == mainArea || areas.second == mainArea || choke->Blocked() || choke->BlockingNeutral()) continue;

			ProtoBotArea_SquadPlacements.insert(choke);
			PositionsFilled.insert({ choke, false });
		}
	}

	spenderManager.onStart();

	//Set active goals
	//activeGoals.insert(SATURATE_WORKERS);
	//activeGoals.insert(EARLY_ZEALOTS);

	//Check for opponent race and unit counts;
	opponentRace = BWAPI::Broodwar->enemy()->getRace();
	std::cout << "Unit Race is " << opponentRace << "\n";

	if (opponentRace != BWAPI::Races::Unknown) opponentRaceNotKnown = false;
}

std::vector<Action> StrategyManager::onFrame(std::vector<ResourceRequest>& resourceRequests)
{
	if (opponentRaceNotKnown == true) checkForOpponentRace();

	std::vector<Action> actionsToReturn;
	PossibleRequests unitsWeCanCreate;

	//Building logic
	const bool buildOrderCompleted = commanderReference->buildOrderCompleted();

	//Might need to move this.
	spenderManager.OnFrame(resourceRequests);

	updateUnitProductionGoals();
	updateUpgradeGoals();

	planUnitProduction(unitsWeCanCreate);
	planUpgradeProduction(unitsWeCanCreate);

	//Only plan buildings once we are out of a Build Order.
	planBuildingProduction(resourceRequests, unitsWeCanCreate);

	finalizeProductionPlan(resourceRequests, unitsWeCanCreate);

	if (drawStrategyDebug)
	{
		drawUnitProductionGoals();
		drawUpgradeProductionGoals();
	}


	//Debug: Drawing choke points to get an idea on where the BWEM can have us position squads
	/*for (const BWEM::ChokePoint* choke : ProtoBotArea_SquadPlacements)
	{
		if (PositionsFilled[choke])
		{
			BWAPI::Broodwar->drawCircleMap(BWAPI::Position(choke->Center()), 10, BWAPI::Broodwar->self()->getColor(), true);
		}
		else
		{
			BWAPI::Broodwar->drawCircleMap(BWAPI::Position(choke->Center()), 10, BWAPI::Colors::Red, true);
		}

	}*/

	//In-Game Time book keeping
	const int frame = BWAPI::Broodwar->getFrameCount();
	const int seconds = frame / FRAMES_PER_SECOND;
	const int minutes = seconds / 60;

	//Estimate income
	ourIncomePerFrameMinerals = workerIncomePerFrameMinerals * activeMiners();
	outIncomePerFrameGas = workerIncomePerFrameGas * activeDrillers();

	if (!(minutesPassedIndex == expansionTimes.size()) && expansionTimes.at(minutesPassedIndex) <= minutes && !buildOrderCompleted) minutesPassedIndex++;

	//ProtoBot unit information
	const FriendlyBuildingCounter ProtoBot_buildings = InformationManager::Instance().getFriendlyBuildingCounter();
	const FriendlyUnitCounter ProtoBot_units = InformationManager::Instance().getFriendlyUnitCounter();
	const FriendlyUpgradeCounter ProtoBot_upgrade = InformationManager::Instance().getFriendlyUpgradeCounter();
	const FriendlyTechCounter ProtoBot_tech = InformationManager::Instance().getFriendlyTechCounter();
	std::vector<Squad*> Protobot_IdleSquads = commanderReference->combatManager.IdleSquads;
	std::vector<Squad*> Protobot_Squads = commanderReference->combatManager.Squads;
	BWAPI::Unitset buildings = commanderReference->buildManager.getBuildings();

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

	const int dynamicSupplyThreshold = supplyThreshold + (ProtoBot_buildings.gateway * 2) + (ProtoBot_buildings.nexus * 1);


	int numberFullSquads = 0;

	for (const Squad* squad : Protobot_Squads)
	{
		if (squad->info.units.size() == MAX_SQUAD_SIZE) numberFullSquads++;
	}

	//std::cout << "Reserving " << getTotalMineralsNeeded() << " out of " << BWAPI::Broodwar->self()->minerals() << "\n";

	//Get Enemy Building information.
	const std::map<BWAPI::Unit, EnemyBuildingInfo>& enemyBuildingInfo = InformationManager::Instance().getKnownEnemyBuildings();

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
			(nexusEconomy.vespeneGyser == nullptr && nexusEconomy.workers.size() >= nexusEconomy.minerals.size() + 3))
		{
			completedNexusEconomy++;
		}
	}

	//4 Gateways per nexus should be built
	saturatedNexus = (ProtoBot_buildings.gateway / 4);

	std::vector<BWAPI::Position> enemyBaselocations;
	for (const auto [unit, building] : enemyBuildingInfo)
	{
		if (building.type.isResourceDepot())
		{
			//BWAPI::Broodwar->drawCircleMap(building.lastKnownPosition, 5, BWAPI::Colors::Red, true);
			enemyBaselocations.push_back(building.lastKnownPosition);
		}
	}

	//First is minerals avalible, Second is gas avalible
	std::pair<int, int> resourcesAvalible = std::make_pair(spenderManager.getPlannedMinerals(resourceRequests), spenderManager.getPlannedGas(resourceRequests));
	const ProtoBotRequestCounter& requests = commanderReference->requestCounter;

	/*std::cout
	<< "ProtoBotRequestCounter {\n"
	<< "  nexus_requests: " << requests.nexus_requests << "\n"
	<< "  gateway_requests: " << requests.gateway_requests << "\n"
	<< "  forge_requests: " << requests.forge_requests << "\n"
	<< "  cybernetics_requests: " << requests.cybernetics_requests << "\n"
	<< "  robotics_requests: " << requests.robotics_requests << "\n"
	<< "  observatory_requests: " << requests.observatory_requests << "\n"
	<< "  citadel_requests: " << requests.citadel_requests << "\n"
	<< "  templarArchives_requests: " << requests.templarArchives_requests << "\n"
	<< "}\n";*/

	if (buildOrderCompleted)
	{
		if (ProtoBot_ProductionFocus == ProductionFocus::UNIT_PRODUCTION && !(ProductionGoal_index >= ProtoBot_ProductionGoals.size()))
		{
			if (timer + 1 >= (UNIT_PRODUCTION_TIME + (DELAY_PER_PRODUCTION * ProductionGoal_index)))
			{
				ProtoBot_ProductionFocus = ProductionFocus::EXPANDING_INFLUENCE;
			}

			//Should build gateway's or nexus's if we have enough minerals. Need to make sure we are not over saturating nexuses.

			if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Nexus))
			{
				//Not expanding properlly after having enough gateways
				if (ProtoBot_buildings.nexus == saturatedNexus)
				{
					//std::cout << "EXPAND ACTION: Requesting to expand (4 gateways saturating nexus)\n";
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Nexus);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Nexus.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Nexus.gasPrice();
				}

				if (BWAPI::Broodwar->self()->minerals() > mineralExcessToExpand)
				{
					mineralExcessToExpand *= 2;
					//std::cout << "EXPAND ACTION: Requesting to expand (mineral surplus)\n";
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Nexus);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Nexus.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Nexus.gasPrice();
				}

				if (!(minutesPassedIndex == expansionTimes.size()) && expansionTimes.at(minutesPassedIndex) <= minutes)
				{
					//std::cout << "EXPAND ACTION: Requesting to expand (expansion time " << expansionTimes.at(minutesPassedIndex) << ")\n";
					minutesPassedIndex++;
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Nexus);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Nexus.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Nexus.gasPrice();
				}
			}

			//Only create 4 gateways per completed nexus economy or 2 gateway and 1 robotics facility.
			if (canAfford(BWAPI::UnitTypes::Protoss_Gateway, resourcesAvalible))
			{
				//std::cout << "Number of \"completed\" Nexus Economies = " << completedNexusEconomy << "\n";

				//4 Gateways per nexus economy
				if (ProtoBot_buildings.gateway < ProtoBot_buildings.nexus * 4)
				{
					//std::cout << "BUILD ACTION: Requesting to warp Gateway\n";
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Gateway);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Gateway.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Gateway.gasPrice();
				}
			}

			timer++;
		}
		else if (ProtoBot_ProductionFocus == ProductionFocus::EXPANDING_INFLUENCE && !(ProductionGoal_index >= ProtoBot_ProductionGoals.size()))
		{
			//Add timer to push production goals after a certain amount of frames
			if (!metProductionGoal(ProtoBot_buildings))
			{
				//Nexus
				//On expanding influence we should create a new nexus immedietly
				if (ProtoBot_buildings.nexus + incompleteBuildings.nexus + requests.nexus_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).nexusCount)
				{
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Nexus);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Nexus.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Nexus.gasPrice();
				}

				//Gateway
				if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Gateway) &&
					ProtoBot_buildings.gateway + incompleteBuildings.gateway + requests.gateway_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).gatewayCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Gateway, resourcesAvalible) &&
					ProtoBot_buildings.nexus >= ProtoBot_ProductionGoals.at(ProductionGoal_index).nexusCount)
				{
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Gateway);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Gateway.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Gateway.gasPrice();
				}

				//Forge
				if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Forge) &&
					ProtoBot_buildings.forge + incompleteBuildings.forge + requests.forge_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).forgeCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Forge, resourcesAvalible) &&
					ProtoBot_buildings.gateway >= ProtoBot_ProductionGoals.at(ProductionGoal_index).gatewayCount)
				{
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Forge);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Forge.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Forge.gasPrice();
				}

				//Cybernetics
				if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Cybernetics_Core) &&
					ProtoBot_buildings.cyberneticsCore + incompleteBuildings.cyberneticsCore + requests.cybernetics_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).cyberneticsCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Cybernetics_Core, resourcesAvalible) &&
					ProtoBot_buildings.forge >= ProtoBot_ProductionGoals.at(ProductionGoal_index).forgeCount)
				{
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Cybernetics_Core);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Cybernetics_Core.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Cybernetics_Core.gasPrice();
				}

				//Robotics
				if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Robotics_Facility) &&
					ProtoBot_buildings.roboticsFacility + incompleteBuildings.roboticsFacility + requests.robotics_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).roboticsCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Robotics_Facility, resourcesAvalible) &&
					ProtoBot_buildings.cyberneticsCore >= ProtoBot_ProductionGoals.at(ProductionGoal_index).cyberneticsCount)
				{
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Robotics_Facility);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Robotics_Facility.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Robotics_Facility.gasPrice();
				}

				//Observatory
				if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Observatory) &&
					ProtoBot_buildings.observatory + incompleteBuildings.observatory + requests.observatory_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).observatoryCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Observatory, resourcesAvalible) &&
					ProtoBot_buildings.roboticsFacility >= ProtoBot_ProductionGoals.at(ProductionGoal_index).roboticsCount)
				{
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Observatory);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Observatory.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Observatory.gasPrice();
				}

				//Citadel
				if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) &&
					ProtoBot_buildings.citadelOfAdun + incompleteBuildings.citadelOfAdun + requests.citadel_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).citadelCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, resourcesAvalible) &&
					ProtoBot_buildings.observatory >= ProtoBot_ProductionGoals.at(ProductionGoal_index).observatoryCount)
				{
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Citadel_of_Adun.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Citadel_of_Adun.gasPrice();
				}

				//Templar Archives
				if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Templar_Archives) &&
					ProtoBot_buildings.templarArchives + incompleteBuildings.templarArchives + requests.templarArchives_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).templarArchivesCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Templar_Archives, resourcesAvalible) &&
					ProtoBot_buildings.citadelOfAdun >= ProtoBot_ProductionGoals.at(ProductionGoal_index).citadelCount)
				{
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Templar_Archives);

					resourcesAvalible.first -= BWAPI::UnitTypes::Protoss_Templar_Archives.mineralPrice();
					resourcesAvalible.second -= BWAPI::UnitTypes::Protoss_Templar_Archives.gasPrice();
				}
			}
			else
			{
				//std::cout << "Met Production Goal\n";
				timer = 0;
				ProtoBot_ProductionFocus = ProductionFocus::UNIT_PRODUCTION;
				ProductionGoal_index++;
			}
		}
		else
		{
			if (checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Nexus))
			{
				//Not expanding properlly after having enough gateways
				if (ProtoBot_buildings.nexus == saturatedNexus)
				{
					//std::cout << "EXPAND ACTION: Requesting to expand (4 gateways saturating nexus)\n";
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Nexus);
				}

				if (BWAPI::Broodwar->self()->minerals() > mineralExcessToExpand)
				{
					mineralExcessToExpand *= 2;
					//std::cout << "EXPAND ACTION: Requesting to expand (mineral surplus)\n";
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Nexus);
				}

				if (!(minutesPassedIndex == expansionTimes.size()) && expansionTimes.at(minutesPassedIndex) <= minutes)
				{
					//std::cout << "EXPAND ACTION: Requesting to expand (expansion time " << expansionTimes.at(minutesPassedIndex) << ")\n";
					minutesPassedIndex++;
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Nexus);
				}
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
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Assimilator);
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
					commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Gateway);
				}
			}

			//1 forge for now but if we want upgrades 2/3 for armor and weapons we need to know when to go some of these buildings.
			if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Forge) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Forge) && ProtoBot_buildings.forge < 1 && (ProtoBot_buildings.gateway >= 1))
			{
				//std::cout << "BUILD ACTION: Requesting to warp Forge\n";

				commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Forge);
			}

			//Only need 1 cybernetics core
			if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Cybernetics_Core) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Cybernetics_Core) && ProtoBot_buildings.cyberneticsCore < 1 && ProtoBot_buildings.gateway >= 1)
			{
				//std::cout << "build action: requesting to warp forge\n";

				commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
			}

			//Should only build 1 robotics facility
			if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Robotics_Facility) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Robotics_Facility) && ProtoBot_buildings.roboticsFacility < 1 && ProtoBot_buildings.cyberneticsCore >= 1)
			{
				//std::cout << "build action: requesting to warp robotics facility\n";

				commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Robotics_Facility);
			}

			//Only 1 observatory
			if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Observatory) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Observatory) && ProtoBot_buildings.observatory < 1 && ProtoBot_buildings.roboticsFacility >= 1)
			{
				//std::cout << "Build Action: requesting to warp observatory\n";

				commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Observatory);
			}

			//2/3 Forge upgrade units

			if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) && ProtoBot_buildings.citadelOfAdun < 1 && ProtoBot_buildings.cyberneticsCore >= 1)
			{
				//std::cout << "build action: requesting to warp citadel of adun\n";

				commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
			}

			if (haveRequiredTech(BWAPI::UnitTypes::Protoss_Templar_Archives) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Templar_Archives) && ProtoBot_buildings.templarArchives < 1 && ProtoBot_buildings.citadelOfAdun >= 1)
			{
				//std::cout << "build action: requesting to warp templar archives\n";

				commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Templar_Archives);
			}
		}
	}

	//Move this to inside if so we dont scout during build order unless instructed.
#pragma region Scout

	if (buildOrderCompleted)
	{
		//change this to frames since last info gained
		if (frame - frameSinceLastScout >= 24 * 20) {
			frameSinceLastScout = frame;

			Action scout;
			scout.type = Action::ACTION_SCOUT;
			actionsToReturn.push_back(scout);
		}
	}

#pragma endregion

#pragma region EnemyCheck
	BWAPI::Unitset unitsOnVisison = BWAPI::Broodwar->enemy()->getUnits();
	BWAPI::Unit unitToAttack = nullptr;
	BWAPI::Position StartingLocation = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	bool enemyNearFriendlyArea = false;
	int closest = INT_MAX;
	for (const BWAPI::Unit unit : unitsOnVisison)
	{
		if (!unit->exists() || unit == nullptr) {
			continue;
		}

		if (unit->isCloaked()) {
			continue;
		}

		const bool isPhantom = phantomPositions.find(unit->getPosition()) != phantomPositions.end();
		if (isPhantom) {
			continue;
		}

		const int dist = unit->getPosition().getApproxDistance(StartingLocation);
		if (dist < closest) {
			closest = dist;
			unitToAttack = unit;
		}
	}

	if (unitToAttack != nullptr) {
		const BWEM::Area* enemyAreaLocation = theMap.GetNearestArea(unitToAttack->getTilePosition());
		for (const BWEM::Area* area : ProtoBot_Areas)
		{
			if (enemyAreaLocation == area) {
				enemyNearFriendlyArea = true;
				break;
			}
		}
	}
#pragma endregion

#pragma region Attack
	// If we have more than two full squads attack. 
	const int totalSupply = BWAPI::Broodwar->self()->supplyTotal() / 2;
	const int supplyUsed = BWAPI::Broodwar->self()->supplyUsed() / 2;
	const int numUnits = commanderReference->combatManager.allUnits.size();
	constexpr int targetCount = NUM_SQUADS_TO_ATTACK * MAX_SQUAD_SIZE;

	//Add timer on supply cap to make us attack so we dont waste time.
	if (supplyUsed >= MINIMUM_SUPPLY_TO_ALL_IN || numUnits > targetCount || (totalSupply == MAX_SUPPLY && supplyUsed + 1 == MAX_SUPPLY))
	{
		if (numUnits > (int) floor(targetCount / 3)) {
			isAttackPhase = true;
			BWAPI::Position attackPos = BWAPI::Positions::Invalid;

			if (unitToAttack){
				const bool isPhantom = phantomPositions.find(unitToAttack->getPosition()) != phantomPositions.end();
				// Every 60 frames, checks if the position from unitToAttack is actually existing (fixing phantom position bug)
				if (BWAPI::Broodwar->getFrameCount() % 60 == 0
					&& !isPhantom
					&& BWAPI::Broodwar->isVisible(BWAPI::TilePosition(unitToAttack->getPosition())) 
					&& BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition(unitToAttack->getPosition()), BWAPI::Filter::IsEnemy).empty()){

					phantomPositions.insert(unitToAttack->getPosition());
				}
				else{
					attackPos = unitToAttack->getPosition();
				}
			}
			
			// If no enemy in vision found, attack closest unit or building from trackedEnemies
			if (attackPos == BWAPI::Positions::Invalid){
				int closestDist = INT_MAX;
				for (const auto& [_, enemyInfo] : InformationManager::Instance().getTrackedEnemies()) {
					if (enemyInfo.destroyed) {
						continue;
					}

					if (enemyInfo.type.hasPermanentCloak()) {
						continue;
					}

					// Every 60 frames, checks if the position from unitToAttack is actually existing (fixing phantom position bug)
					const bool isPhantom = phantomPositions.find(enemyInfo.lastSeenPos) != phantomPositions.end();

					if (isPhantom) {
						continue;
					}

					if (BWAPI::Broodwar->getFrameCount() % 60 == 0
						&& !isPhantom
						&& BWAPI::Broodwar->isVisible(BWAPI::TilePosition(enemyInfo.lastSeenPos))
						&& BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition(enemyInfo.lastSeenPos), BWAPI::Filter::IsEnemy).empty()) {

						phantomPositions.insert(enemyInfo.lastSeenPos);
						continue;
					}

					const int dist = StartingLocation.getApproxDistance(enemyInfo.lastSeenPos);
					if (dist < closestDist) {
						closestDist = dist;
						attackPos = enemyInfo.lastSeenPos;
					}
				}
			}

			// If all other attack position methods failed, attack an enemy base
			if (attackPos == BWAPI::Positions::Invalid && enemyBaselocations.size() > 0) {
				attackPos = enemyBaselocations.at(enemyBaselocations.size() - 1);
			}

			// Send action only if theres a new, valid attack position
			if (attackPos != BWAPI::Positions::Invalid && attackPos != lastAttackPos) {
				Action attack;
				attack.type = Action::ACTION_ATTACK;
				attack.attackPosition = attackPos;
				actionsToReturn.push_back(attack);

				lastAttackPos = attackPos;
				isAttackPhase = true;
			}
		}
		else {
			lastAttackPos = BWAPI::Positions::Invalid;
			isAttackPhase = false;
		}
	}
#pragma endregion

#pragma region Defend
	if (!isAttackPhase) {
		if (unitToAttack != nullptr && unitToAttack->exists() && enemyNearFriendlyArea) {
			int closestDist = 0;

			Action attack;
			attack.type = Action::ACTION_REINFORCE;
			attack.reinforcePosition = unitToAttack->getPosition();
			actionsToReturn.push_back(attack);
		}

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

void StrategyManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	//std::cout << unit->getType() << " was destroyed and was " << (unit->isCompleted() ? "completed\n" : "not completed\n");

	//Update Units counts
	switch (unit->getType())
	{
	case BWAPI::UnitTypes::Protoss_Probe:
		ProtoBot_createdUnitCount.created_workers--;
		break;
	case BWAPI::UnitTypes::Protoss_Zealot:
		ProtoBot_createdUnitCount.created_zealots--;
		break;
	case BWAPI::UnitTypes::Protoss_Dragoon:
		ProtoBot_createdUnitCount.created_dragoons--;
		break;
	case BWAPI::UnitTypes::Protoss_Dark_Templar:
		ProtoBot_createdUnitCount.created_dark_templars--;
		break;
	case BWAPI::UnitTypes::Protoss_Observer:
		ProtoBot_createdUnitCount.created_observers--;
		break;
	case BWAPI::UnitTypes::Protoss_Arbiter_Tribunal:
		incompleteBuildings.arbiterTribunal--;
		break;
	case BWAPI::UnitTypes::Protoss_Assimilator:
		incompleteBuildings.assimilator--;
		break;
	case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
		incompleteBuildings.citadelOfAdun--;
		ProtoBot_createdUnitCount.created_citadel--;
		break;
	case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
		incompleteBuildings.cyberneticsCore--;
		ProtoBot_createdUnitCount.created_cybernetics--;
		break;
	case BWAPI::UnitTypes::Protoss_Fleet_Beacon:
		incompleteBuildings.fleetBeacon--;
		break;
	case BWAPI::UnitTypes::Protoss_Forge:
		incompleteBuildings.forge--;
		ProtoBot_createdUnitCount.created_forge--;
		break;
	case BWAPI::UnitTypes::Protoss_Gateway:
		incompleteBuildings.gateway--;
		ProtoBot_createdUnitCount.created_gateway--;
		break;
	case BWAPI::UnitTypes::Protoss_Nexus:
		incompleteBuildings.nexus--;
		ProtoBot_createdUnitCount.created_nexus--;
		break;
	case BWAPI::UnitTypes::Protoss_Observatory:
		incompleteBuildings.observatory--;
		ProtoBot_createdUnitCount.created_observatory--;
		break;
	case BWAPI::UnitTypes::Protoss_Photon_Cannon:
		incompleteBuildings.photonCannon--;
		ProtoBot_createdUnitCount.created_photonCannons--;
		break;
	case BWAPI::UnitTypes::Protoss_Pylon:
		incompleteBuildings.pylon--;
		ProtoBot_createdUnitCount.created_pylons--;
		break;
	case BWAPI::UnitTypes::Protoss_Robotics_Facility:
		incompleteBuildings.roboticsFacility--;
		ProtoBot_createdUnitCount.created_roboticsFacility--;
		break;
	case BWAPI::UnitTypes::Protoss_Robotics_Support_Bay:
		incompleteBuildings.roboticsSupportBay--;
		break;
	case BWAPI::UnitTypes::Protoss_Shield_Battery:
		incompleteBuildings.shieldBattery--;
		break;
	case BWAPI::UnitTypes::Protoss_Stargate:
		incompleteBuildings.stargate--;
		break;
	case BWAPI::UnitTypes::Protoss_Templar_Archives:
		incompleteBuildings.templarArchives--;
		ProtoBot_createdUnitCount.created_templarArchives--;
		break;
	default:
		break;
	}

	if (unit->getType().isResourceDepot())
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
				if (areas.first == mainArea || areas.second == mainArea || choke->Blocked() || choke->BlockingNeutral()) continue;

				ProtoBotArea_SquadPlacements.insert(choke);
			}
		}

		resourceDepots.erase(unit);
	}

	if (unit->getType().isWorker()) workers.erase(unit);

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility)
	{
		unitProduction.erase(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Forge ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Cybernetics_Core ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Citadel_of_Adun)
	{
		upgradeProduction.erase(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Forge) forges.erase(unit);

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Cybernetics_Core) cybernetics.erase(unit);

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Citadel_of_Adun) citadels.erase(unit);
}

void StrategyManager::onUnitCreate(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	switch (unit->getType())
	{
	case BWAPI::UnitTypes::Protoss_Probe:
		ProtoBot_createdUnitCount.created_workers++;
		unitProductionCounter.worker++;
		break;
	case BWAPI::UnitTypes::Protoss_Zealot:
		ProtoBot_createdUnitCount.created_zealots++;
		unitProductionCounter.zealots++;
		break;
	case BWAPI::UnitTypes::Protoss_Dragoon:
		ProtoBot_createdUnitCount.created_dragoons++;
		unitProductionCounter.dragoons++;
		break;
	case BWAPI::UnitTypes::Protoss_Dark_Templar:
		ProtoBot_createdUnitCount.created_dark_templars++;
		unitProductionCounter.dark_templars++;
		break;
	case BWAPI::UnitTypes::Protoss_Observer:
		ProtoBot_createdUnitCount.created_observers++;
		unitProductionCounter.observers++;
		break;
	case BWAPI::UnitTypes::Protoss_Arbiter_Tribunal:
		incompleteBuildings.arbiterTribunal++;
		break;
	case BWAPI::UnitTypes::Protoss_Assimilator:
		incompleteBuildings.assimilator++;
		break;
	case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
		incompleteBuildings.citadelOfAdun++;
		ProtoBot_createdUnitCount.created_citadel++;
		break;
	case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
		incompleteBuildings.cyberneticsCore++;
		ProtoBot_createdUnitCount.created_cybernetics++;
		break;
	case BWAPI::UnitTypes::Protoss_Fleet_Beacon:
		incompleteBuildings.fleetBeacon++;
		break;
	case BWAPI::UnitTypes::Protoss_Forge:
		incompleteBuildings.forge++;
		ProtoBot_createdUnitCount.created_forge++;
		break;
	case BWAPI::UnitTypes::Protoss_Gateway:
		incompleteBuildings.gateway++;
		ProtoBot_createdUnitCount.created_gateway++;
		break;
	case BWAPI::UnitTypes::Protoss_Nexus:
		incompleteBuildings.nexus++;
		ProtoBot_createdUnitCount.created_nexus++;
		break;
	case BWAPI::UnitTypes::Protoss_Observatory:
		incompleteBuildings.observatory++;
		ProtoBot_createdUnitCount.created_observatory++;
		break;
	case BWAPI::UnitTypes::Protoss_Photon_Cannon:
		incompleteBuildings.photonCannon++;
		ProtoBot_createdUnitCount.created_photonCannons++;
		break;
	case BWAPI::UnitTypes::Protoss_Pylon:
		incompleteBuildings.pylon++;
		ProtoBot_createdUnitCount.created_pylons++;
		break;
	case BWAPI::UnitTypes::Protoss_Robotics_Facility:
		incompleteBuildings.roboticsFacility++;
		ProtoBot_createdUnitCount.created_roboticsFacility++;
		break;
	case BWAPI::UnitTypes::Protoss_Robotics_Support_Bay:
		incompleteBuildings.roboticsSupportBay++;
		break;
	case BWAPI::UnitTypes::Protoss_Shield_Battery:
		incompleteBuildings.shieldBattery++;
		break;
	case BWAPI::UnitTypes::Protoss_Stargate:
		incompleteBuildings.stargate++;
		break;
	case BWAPI::UnitTypes::Protoss_Templar_Archives:
		incompleteBuildings.templarArchives++;
		ProtoBot_createdUnitCount.created_templarArchives++;
		break;
	default:
		break;
	}

	if (unit->getType().isResourceDepot())
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
				if (areas.first == mainArea || areas.second == mainArea || choke->Blocked() || choke->BlockingNeutral()) continue;

				ProtoBotArea_SquadPlacements.insert(choke);
				PositionsFilled.insert({ choke, false });
			}
		}

		resourceDepots.insert(unit);
	}

	if (unit->getType().isWorker()) workers.insert(unit);

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility)
	{
		unitProduction.insert(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Forge ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Cybernetics_Core ||
		unit->getType() == BWAPI::UnitTypes::Protoss_Citadel_of_Adun)
	{
		upgradeProduction.insert(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Forge) forges.insert(unit);

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Cybernetics_Core) cybernetics.insert(unit);

	if (unit->getType() == BWAPI::UnitTypes::Protoss_Citadel_of_Adun) citadels.insert(unit);
}

void StrategyManager::onUnitComplete(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	switch (unit->getType())
	{
	case BWAPI::UnitTypes::Protoss_Arbiter_Tribunal:
		incompleteBuildings.arbiterTribunal--;
		break;
	case BWAPI::UnitTypes::Protoss_Assimilator:
		incompleteBuildings.assimilator--;
		break;
	case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
		incompleteBuildings.citadelOfAdun--;
		break;
	case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
		incompleteBuildings.cyberneticsCore--;
		break;
	case BWAPI::UnitTypes::Protoss_Fleet_Beacon:
		incompleteBuildings.fleetBeacon--;
		break;
	case BWAPI::UnitTypes::Protoss_Forge:
		incompleteBuildings.forge--;
		break;
	case BWAPI::UnitTypes::Protoss_Gateway:
		incompleteBuildings.gateway--;
		break;
	case BWAPI::UnitTypes::Protoss_Nexus:
		incompleteBuildings.nexus--;
		break;
	case BWAPI::UnitTypes::Protoss_Observatory:
		incompleteBuildings.observatory--;
		break;
	case BWAPI::UnitTypes::Protoss_Photon_Cannon:
		incompleteBuildings.photonCannon--;
		break;
	case BWAPI::UnitTypes::Protoss_Pylon:
		incompleteBuildings.pylon--;
		break;
	case BWAPI::UnitTypes::Protoss_Robotics_Facility:
		incompleteBuildings.roboticsFacility--;
		break;
	case BWAPI::UnitTypes::Protoss_Robotics_Support_Bay:
		incompleteBuildings.roboticsSupportBay--;
		break;
	case BWAPI::UnitTypes::Protoss_Shield_Battery:
		incompleteBuildings.shieldBattery--;
		break;
	case BWAPI::UnitTypes::Protoss_Stargate:
		incompleteBuildings.stargate--;
		break;
	case BWAPI::UnitTypes::Protoss_Templar_Archives:
		incompleteBuildings.templarArchives--;
		break;
	default:
		break;
	}
}

void StrategyManager::drawGameUnitProduction(UnitProductionGameCounter& unitProduction, int x, int y, bool background)
{
	if (background) BWAPI::Broodwar->drawBoxScreen(x - 5, y - 5, x + 200, y + 65, BWAPI::Colors::Black, true);

	BWAPI::Broodwar->drawTextScreen(x, y, "%cTotal Combat Units Created (Game)", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 1, "%c________________________________", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 10, "%cWorkers = %d", BWAPI::Text::White, unitProduction.worker);
	BWAPI::Broodwar->drawTextScreen(x, y + 20, "%cZealots = %d", BWAPI::Text::White, unitProduction.zealots);
	BWAPI::Broodwar->drawTextScreen(x, y + 30, "%cDragoons = %d", BWAPI::Text::White, unitProduction.dragoons);
	BWAPI::Broodwar->drawTextScreen(x, y + 40, "%cObservers = %d", BWAPI::Text::White, unitProduction.observers);
	BWAPI::Broodwar->drawTextScreen(x, y + 50, "%cDark Templars = %d", BWAPI::Text::White, unitProduction.dark_templars);
}

void StrategyManager::drawUpgradeProductionGoals()
{
	int x = 5;
	int y = 230;
	BWAPI::Broodwar->drawTextScreen(x, y, "Upgarde Production Goals:");
	int index = 0;
	for (const UpgradeProductionGoals goal : upgradeProductionGoals)
	{
		std::string temp;

		switch (goal)
		{
		case RESEARCH_SINGULARITY_CHARGE:
			temp = "RESEARCH_SINGULARITY_CHARGE";
			break;
		case RESEARCH_GROUND_WEAPONS:
			temp = "RESEARCH_GROUND_WEAPONS";
			break;
		case RESEARCH_GROUND_ARMOR:
			temp = "RESEARCH_GROUND_ARMOR";
			break;
		case RESEARCH_PLASMA_SHIELDS:
			temp = "RESEARCH_PLASMA_SHIELDS";
			break;
		case SOMETHING_WENT_WRONG_RESEARCH_LEG_ENHANCEMENTS:
			temp = "SOMETHING_WENT_WRONG_RESEARCH_LEG_ENHANCEMENTS";
			break;
		default:
			temp = "UNKNOWN_GOAL";
			break;
		}

		BWAPI::Broodwar->drawTextScreen(x, y + ((index + 1) * 10), "%s", temp.c_str());
		index++;
	}
}

void StrategyManager::drawUnitProductionGoals()
{
	int x = 5;
	int y = 170;
	BWAPI::Broodwar->drawTextScreen(x, y, "Production Goals:");
	int index = 0;

	for (const UnitProductionGoals goal : unitProductionGoals)
	{
		std::string temp;

		switch (goal)
		{
		case SATURATE_WORKERS:
			temp = "SATURATE_WORKERS";
			break;
		case EARLY_ZEALOTS:
			temp = "EARLY_ZEALOTS";
			break;
		case DARK_TEMPLAR_ATTEMPT:
			temp = "DARK_TEMPLAR_ATTEMPT";
			break;
		case OBSERVER_SCOUTS:
			temp = "OBSERVER_SCOUTS";
			break;
		case INFINITE_DRAGOONS:
			temp = "INFINITE_DRAGOONS";
			break;
		case SOMETHING_WENT_WRONG_GO_INFINITE_ZEALOTS:
			temp = "SOMETHING_WENT_WRONG_GO_INFINITE_ZEALOTS";
			break;
		case INVISIBLE_UNIT_DETECTED_SQUADS_NEED_OBSERVERS:
			temp = "INVISIBLE_UNIT_DETECTED_SQUADS_NEED_OBSERVERS";
			break;
		case FLYING_UNIT_DETECTED_NEED_CANNONS:
			temp = "FLYING_UNIT_DETECHED_NEED_CANNONS";
			break;
		default:
			temp = "UNKNOWN_GOAL";
			break;
		}

		BWAPI::Broodwar->drawTextScreen(x, y + ((index + 1) * 10), "%s", temp.c_str());
		index++;
	}
}

void StrategyManager::checkForOpponentRace()
{
	for (const auto& pair : InformationManager::Instance().getKnownEnemyBuildings()) {
		if (pair.first->getPlayer() == BWAPI::Broodwar->enemy() &&
			pair.first->exists() &&
			pair.first->isVisible() &&
			pair.second.destroyed == false)
		{
			opponentRaceNotKnown = false;
			opponentRace = pair.first->getType().getRace();
			//std::cout << "Enemy building found, Opponent Race is " << opponentRace << "\n";
			break;
		}
	}

	if (opponentRaceNotKnown == false) return;

	for (const auto& enemyUnit : InformationManager::Instance().getKnownEnemies()) {
		if (enemyUnit->getPlayer() == BWAPI::Broodwar->enemy() &&
			enemyUnit->exists() &&
			enemyUnit->isVisible())
		{
			opponentRaceNotKnown = false;
			opponentRace = enemyUnit->getType().getRace();
			//std::cout << "Enemy combat unit found, Opponent Race is " << opponentRace << "\n";
		}
	}
}

int StrategyManager::activeMiners()
{
	int activeMineralWorkerCount = 0;

	for (const BWAPI::Unit worker : workers)
	{
		if (worker == nullptr || !worker->exists()) continue;

		const BWAPI::Order order = worker->getOrder();

		if (order == BWAPI::Orders::MoveToMinerals ||
			order == BWAPI::Orders::WaitForMinerals ||
			order == BWAPI::Orders::MiningMinerals ||
			order == BWAPI::Orders::ReturnMinerals)
		{
			activeMineralWorkerCount++;
		}
	}

	return activeMineralWorkerCount;
}

int StrategyManager::activeDrillers()
{
	int activeDrillerlWorkerCount = 0;

	for (const BWAPI::Unit worker : workers)
	{
		if (worker == nullptr || !worker->exists()) continue;

		const BWAPI::Order order = worker->getOrder();

		if (order == BWAPI::Orders::MoveToGas ||
			order == BWAPI::Orders::WaitForGas ||
			order == BWAPI::Orders::HarvestGas ||
			order == BWAPI::Orders::ReturnGas)
		{
			activeDrillerlWorkerCount++;
		}
	}

	return activeDrillerlWorkerCount;
}

void StrategyManager::updateUnitProductionGoals()
{
	//Notes:
	//Need a way to check tech tree to make sure we can build
	const ProtoBotRequestCounter& request_count = commanderReference->requestCounter;
	const FriendlyBuildingCounter completedBuildingsCount = InformationManager::Instance().getFriendlyBuildingCounter();
	std::vector<Squad*> ProtoBot_Squads = commanderReference->combatManager.Squads;

	//Probes
	if (request_count.worker_requests + ProtoBot_createdUnitCount.created_workers < MAX_WORKERS)
	{
		unitProductionGoals.insert(SATURATE_WORKERS);
	}
	else
	{
		unitProductionGoals.erase(SATURATE_WORKERS);
	}

	//Zealots 
	//Always have 3 zealots at the beggining of the game unless the build order we have focuses dragoons then dont attempt to create any zealots.
	if ((request_count.zealots_requests + unitProductionCounter.zealots < MAX_EARLY_ZEALOTS && request_count.dragoons_requests + unitProductionCounter.dragoons < 1))
	{
		unitProductionGoals.insert(EARLY_ZEALOTS);
	}
	else
	{
		unitProductionGoals.erase(EARLY_ZEALOTS);
	}

	//Dragoons
	//Once this is inserted it will not be removed unless something bad happens.
	//Will be added once we have made + planned 3 zealots or have atleast made 1 dragoon during build order.
	if ((request_count.zealots_requests + unitProductionCounter.zealots == MAX_EARLY_ZEALOTS || request_count.dragoons_requests + unitProductionCounter.dragoons >= 1))
	{
		unitProductionGoals.insert(INFINITE_DRAGOONS);
	}

	//Minimum Observer Production
	if (request_count.observers_requests + unitProductionCounter.observers < MAX_OBSERVERS_FOR_SCOUTING)
	{
		unitProductionGoals.insert(OBSERVER_SCOUTS);
	}
	else
	{
		unitProductionGoals.erase(OBSERVER_SCOUTS);
	}

	//Constant Observer Production
	/*if (InformationManager::Instance().enemyHasCloakTech())
	{
		int numObservers = (request_count.observers_requests + unitProductionCounter.observers) - MAX_OBSERVERS_FOR_SCOUTING

		//Check to make sure negative unit count doesnt happen from scouting observers.
		//Can probably work with matthew to make sure we can check what squads have a observer.
		numObersvers = (numObservers < 0 ? 0 : numObservers)

		if ((request_count.observers_requests + unitProductionCounter.observers) - MAX_OBSERVERS_FOR_SCOUTING < ProtoBot_Squads.size())
		{
			activeGoals.insert(INVISIBLE_UNIT_DETECTED_SQUADS_NEED_OBSERVERS);
		}
		else
		{
			activeGoals.insert(INVISIBLE_UNIT_DETECTED_SQUADS_NEED_OBSERVERS);
		}
	}*/

	//Dark Templars
	if (opponentRace == BWAPI::Races::Terran || opponentRace == BWAPI::Races::Protoss)
	{
		if (request_count.dark_templars_requests + unitProductionCounter.dark_templars < MAX_DARK_TEMPLARS)
		{
			unitProductionGoals.insert(DARK_TEMPLAR_ATTEMPT);
		}
		else
		{
			unitProductionGoals.erase(DARK_TEMPLAR_ATTEMPT);
		}
	}

	//Photon Cannons (Yes I know they are not combat units but we will leave this here for now)
	//InformationManager::Instance().enemyHasAirTech();
}

void StrategyManager::planUnitProduction(PossibleRequests& possibleRequestList)
{
	const ProtoBotRequestCounter& request_count = commanderReference->requestCounter;
	const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;

	//Prevents construction of units outside Build Order.
	const bool trainingBlock = commanderReference->buildManager.shouldPreventUnitTraining(currentSupply);
	const std::vector<NexusEconomy> nexusEconomies = commanderReference->getNexusEconomies();

	//Add tech tree into production.

	for (const UnitProductionGoals productionGoal : unitProductionGoals)
	{
		switch (productionGoal)
		{
		case SATURATE_WORKERS:
			for (const NexusEconomy nexusEconomy : nexusEconomies)
			{
				if (commanderReference->alreadySentRequest(nexusEconomy.nexus->getID()) == false &&
					!nexusEconomy.nexus->isTraining() &&
					nexusEconomy.nexus->isCompleted() &&
					nexusEconomy.workers.size() < (nexusEconomy.minerals.size() * MAXIMUM_WORKERS_PER_MINERAL) + (nexusEconomy.vespeneGyser != nullptr ? WORKERS_PER_ASSIMILATOR : 0) &&
					(request_count.worker_requests + ProtoBot_createdUnitCount.created_workers + 1) <= MAX_WORKERS)
				{
					PossibleUnitRequest probe;
					probe.unit = BWAPI::UnitTypes::Protoss_Probe;
					probe.trainer = nexusEconomy.nexus;

					possibleRequestList.units.push_back(probe);
				}
			}
			break;

		case EARLY_ZEALOTS:
			if (trainingBlock) break;

			for (const BWAPI::Unit building : unitProduction)
			{
				if (building->getType() != BWAPI::UnitTypes::Protoss_Gateway) continue;

				//unitProductionCounter.zealots
				if (commanderReference->alreadySentRequest(building->getID()) == false &&
					!building->isTraining() &&
					building->isCompleted() &&
					(request_count.zealots_requests + unitProductionCounter.zealots + 1) <= MAX_EARLY_ZEALOTS)
				{
					PossibleUnitRequest zealot;
					zealot.unit = BWAPI::UnitTypes::Protoss_Zealot;
					zealot.trainer = building;

					possibleRequestList.units.push_back(zealot);
				}
			}
			break;
		case DARK_TEMPLAR_ATTEMPT:
			if (trainingBlock || haveRequiredTech(BWAPI::UnitTypes::Protoss_Dark_Templar) == false) break;

			for (const BWAPI::Unit building : unitProduction)
			{
				if (building->getType() != BWAPI::UnitTypes::Protoss_Gateway) continue;

				if (commanderReference->alreadySentRequest(building->getID()) == false &&
					!building->isTraining() &&
					building->isCompleted() &&
					(request_count.dark_templars_requests + unitProductionCounter.dark_templars + 1) <= MAX_EARLY_ZEALOTS)
				{
					PossibleUnitRequest darkTemplar;
					darkTemplar.unit = BWAPI::UnitTypes::Protoss_Dark_Templar;
					darkTemplar.trainer = building;

					possibleRequestList.units.push_back(darkTemplar);
				}
			}
			break;
		case OBSERVER_SCOUTS:
			if (trainingBlock || haveRequiredTech(BWAPI::UnitTypes::Protoss_Observer) == false) break;

			for (const BWAPI::Unit building : unitProduction)
			{
				if (building->getType() != BWAPI::UnitTypes::Protoss_Robotics_Facility) continue;

				if (commanderReference->alreadySentRequest(building->getID()) == false &&
					!building->isTraining() &&
					building->isCompleted() &&
					(request_count.observers_requests + unitProductionCounter.observers + 1) <= MAX_OBSERVERS_FOR_SCOUTING)
				{
					PossibleUnitRequest observer;
					observer.unit = BWAPI::UnitTypes::Protoss_Observer;
					observer.trainer = building;

					possibleRequestList.units.push_back(observer);
				}
			}
			break;
		case INFINITE_DRAGOONS:
			if (trainingBlock || haveRequiredTech(BWAPI::UnitTypes::Protoss_Dragoon) == false) break;

			for (const BWAPI::Unit building : unitProduction)
			{
				if (building->getType() != BWAPI::UnitTypes::Protoss_Gateway) continue;

				if (commanderReference->alreadySentRequest(building->getID()) == false &&
					!building->isTraining() &&
					building->isCompleted())
				{
					PossibleUnitRequest dragoon;
					dragoon.unit = BWAPI::UnitTypes::Protoss_Dragoon;
					dragoon.trainer = building;

					possibleRequestList.units.push_back(dragoon);
				}
			}
			break;
		default:
			break;
		}
	}
}

void StrategyManager::updateUpgradesBeingCreated()
{
	//Reset values.
	upgradesInProduction.singularity_charge = 0;
	upgradesInProduction.ground_weapons = 0;
	upgradesInProduction.ground_armor = 0;
	upgradesInProduction.plasma_shields = 0;
	upgradesInProduction.leg_enhancements = 0;

	for (BWAPI::Unit upgrade_building : upgradeProduction)
	{
		if (!upgrade_building->isUpgrading()) continue;

		switch (upgrade_building->getUpgrade())
		{
		case BWAPI::UpgradeTypes::Singularity_Charge:
			upgradesInProduction.singularity_charge++;
			break;
		case BWAPI::UpgradeTypes::Protoss_Ground_Weapons:
			upgradesInProduction.ground_weapons++;
			break;
		case BWAPI::UpgradeTypes::Protoss_Ground_Armor:
			upgradesInProduction.ground_armor++;
			break;
		case BWAPI::UpgradeTypes::Protoss_Plasma_Shields:
			upgradesInProduction.plasma_shields++;
			break;
		case BWAPI::UpgradeTypes::Leg_Enhancements:
			upgradesInProduction.leg_enhancements++;
			break;
		}
	}
}

void StrategyManager::updateUpgradeGoals()
{
	const ProtoBotRequestCounter& request_count = commanderReference->requestCounter;
	const FriendlyBuildingCounter completedBuildingsCount = InformationManager::Instance().getFriendlyBuildingCounter();
	const FriendlyUnitCounter completedUnitsCount = InformationManager::Instance().getFriendlyUnitCounter();
	const FriendlyUpgradeCounter completedUpgradesCount = InformationManager::Instance().getFriendlyUpgradeCounter();
	const int ProtoBot_Squads = commanderReference->combatManager.Squads.size();

	updateUpgradesBeingCreated();

	//Singularity Charge
	if (request_count.singularity_requests + upgradesInProduction.singularity_charge + completedUpgradesCount.singularityCharge < MAX_SINGULARITY_UPGRADES)
	{
		upgradeProductionGoals.insert(RESEARCH_SINGULARITY_CHARGE);
	}
	else
	{
		upgradeProductionGoals.erase(RESEARCH_SINGULARITY_CHARGE);
	}

	//Ground Armor
	if (request_count.groundArmor_requests + upgradesInProduction.ground_armor + completedUpgradesCount.groundArmor < 1
		&& (ProtoBot_Squads >= 5))
	{
		upgradeProductionGoals.insert(RESEARCH_GROUND_ARMOR);
	}
	else
	{
		upgradeProductionGoals.erase(RESEARCH_GROUND_ARMOR);
	}

	//Ground Weapons
	if (request_count.groundWeapons_requests + upgradesInProduction.ground_weapons + completedUpgradesCount.groundWeapons < 1
		&& (ProtoBot_Squads >= 3 || completedUnitsCount.dragoon >= 20))
	{
		upgradeProductionGoals.insert(RESEARCH_GROUND_WEAPONS);
	}
	else
	{
		upgradeProductionGoals.erase(RESEARCH_GROUND_WEAPONS);
	}

	//Plasma Shields


	//Leg Enhancements

}

void StrategyManager::planUpgradeProduction(PossibleRequests& possibleRequestList)
{
	const ProtoBotRequestCounter& request_count = commanderReference->requestCounter;
	const FriendlyBuildingCounter completedBuildingsCount = InformationManager::Instance().getFriendlyBuildingCounter();
	const FriendlyUnitCounter completedUnitsCount = InformationManager::Instance().getFriendlyUnitCounter();
	const FriendlyUpgradeCounter completedUpgradesCount = InformationManager::Instance().getFriendlyUpgradeCounter();
	const int ProtoBot_Squads = commanderReference->combatManager.Squads.size();

	for (const UpgradeProductionGoals productionGoal : upgradeProductionGoals)
	{
		switch (productionGoal)
		{
		case RESEARCH_SINGULARITY_CHARGE:
			for (const BWAPI::Unit unit : cybernetics)
			{
				if (commanderReference->alreadySentRequest(unit->getID()) == false &&
					!unit->isTraining() &&
					unit->isCompleted() &&
					completedUnitsCount.dragoon >= 1 &&
					(request_count.singularity_requests + upgradesInProduction.singularity_charge + completedUpgradesCount.singularityCharge + 1) == MAX_SINGULARITY_UPGRADES)
				{
					PossibleUpgradeRequest singularity;
					singularity.upgrade = BWAPI::UpgradeTypes::Singularity_Charge;
					singularity.trainer = unit;

					possibleRequestList.upgrades.push_back(singularity);
				}
			}
			break;
		case RESEARCH_GROUND_WEAPONS:
			for (const BWAPI::Unit unit : forges)
			{
				if (commanderReference->alreadySentRequest(unit->getID()) == false &&
					!unit->isTraining() &&
					unit->isCompleted() &&
					ProtoBot_Squads >= 3 &&
					(request_count.groundWeapons_requests + upgradesInProduction.ground_weapons + completedUpgradesCount.groundWeapons + 1) <= 1)
				{
					PossibleUpgradeRequest groundWeapons;
					groundWeapons.upgrade = BWAPI::UpgradeTypes::Protoss_Ground_Weapons;
					groundWeapons.trainer = unit;

					possibleRequestList.upgrades.push_back(groundWeapons);
				}
			}
			break;
		case RESEARCH_GROUND_ARMOR:
			for (const BWAPI::Unit unit : forges)
			{
				if (commanderReference->alreadySentRequest(unit->getID()) == false &&
					!unit->isTraining() &&
					unit->isCompleted() &&
					ProtoBot_Squads >= 3 &&
					(request_count.groundArmor_requests + upgradesInProduction.ground_armor + completedUpgradesCount.groundArmor + 1) <= 1)
				{
					PossibleUpgradeRequest groundArmor;
					groundArmor.upgrade = BWAPI::UpgradeTypes::Protoss_Ground_Armor;
					groundArmor.trainer = unit;

					possibleRequestList.upgrades.push_back(groundArmor);
				}
			}
			break;
		}
	}
}

void StrategyManager::planBuildingProduction(std::vector<ResourceRequest>& resourceRequests, PossibleRequests& possibleRequestList)
{
	const FriendlyBuildingCounter ProtoBot_buildings = InformationManager::Instance().getFriendlyBuildingCounter();
	const FriendlyUnitCounter ProtoBot_units = InformationManager::Instance().getFriendlyUnitCounter();
	const FriendlyUpgradeCounter ProtoBot_upgrade = InformationManager::Instance().getFriendlyUpgradeCounter();
	const FriendlyTechCounter ProtoBot_tech = InformationManager::Instance().getFriendlyTechCounter();
	BWAPI::Unitset incompleteBuildings = commanderReference->buildManager.getBuildings();
	std::vector<NexusEconomy> nexusEconomies = commanderReference->getNexusEconomies();

	//Better solution would be supply at a rate of roughly (#nexus / probe build time + #gateways / zealot build time + ...) 
	//This is not really an acurrate way to get supply use projection but it "works" 
	const int dynamicSupplyThreshold = supplyThreshold + (ProtoBot_buildings.gateway * 2) + (ProtoBot_buildings.nexus * 1);

	//Pylon requests: Once build order is completed, run this check to make sure we have enough supply to do things.
	if (spenderManager.plannedSupply(resourceRequests, incompleteBuildings) <= dynamicSupplyThreshold && ((BWAPI::Broodwar->self()->supplyTotal() / 2) != MAX_SUPPLY))
	{
		PossibleBuildingRequest pylon;
		pylon.building = BWAPI::UnitTypes::Protoss_Pylon;

		possibleRequestList.supplyBuildings.push_back(pylon);
	}

	//Assimilators for nexus's
	for (const NexusEconomy& nexusEconomy : nexusEconomies)
	{
		if (nexusEconomy.vespeneGyser != nullptr
			&& nexusEconomy.assimilator == nullptr
			&& nexusEconomy.nexus != nullptr
			&& nexusEconomy.workers.size() >= nexusEconomy.minerals.size() + 2)
		{
			const BWEM::Base* base = &getBaseReference(nexusEconomy.nexus);

			//Unit is already planned in this case
			if (commanderReference->checkUnitIsBeingWarpedIn(BWAPI::UnitTypes::Protoss_Assimilator, base)
				|| commanderReference->requestedBuilding(BWAPI::UnitTypes::Protoss_Assimilator, base->Center())
				|| commanderReference->checkUnitIsPlanned(BWAPI::UnitTypes::Protoss_Assimilator, base->Center()))
			{
				continue;
			}

			PossibleBuildingRequest assimilator;
			assimilator.building = BWAPI::UnitTypes::Protoss_Assimilator;

			assimilator.base = base;

			//Change this to position to be extra careful
			assimilator.nexusPosition = assimilator.base->Center();

			//Check to make sure correct base is being found and used.
			/*
			std::cout << "Nexus does not have assimlator...requesting\n";
			std::cout << "Nexus ID: " << nexusEconomy.nexus->getID() << "\n";
			std::cout << "Has Gyser: " << (nexusEconomy.vespeneGyser == nullptr ? "no\n" : "yes\n");
			std::cout << "Has Assimilator: " << (nexusEconomy.assimilator == nullptr ? "false\n" : "true\n");
			std::cout << "BWAPI::Position: " << assimilator.nexusPosition << "\n";
			std::cout << "BWEM Base location: " << assimilator.base->Center() << "\n";
			*/
			possibleRequestList.supplyBuildings.push_back(assimilator);
		}
	}
}

const BWEM::Base& StrategyManager::getBaseReference(BWAPI::Unit nexus)
{
	for (const BWEM::Area& area : theMap.Areas())
	{
		for (const BWEM::Base& base : area.Bases())
		{
			if (base.Location() == nexus->getTilePosition()) return base;
		}
	}

	std::cout << "WARNING BASE NOT FOUND\n";
}

void StrategyManager::finalizeProductionPlan(std::vector<ResourceRequest>& resourceRequests, PossibleRequests& possibleRequestList)
{
	for (const PossibleUnitRequest& unitRequest : possibleRequestList.units)
	{
		commanderReference->requestUnit(unitRequest.unit, unitRequest.trainer);
	}

	for (const PossibleBuildingRequest& unitRequest : possibleRequestList.supplyBuildings)
	{
		commanderReference->requestBuilding(unitRequest.building, false, false, false, unitRequest.nexusPosition, unitRequest.base);
	}

	for (const PossibleUpgradeRequest& unitRequest : possibleRequestList.upgrades)
	{
		commanderReference->requestUpgrade(unitRequest.trainer, unitRequest.upgrade);
	}
}


bool StrategyManager::checkAlreadyRequested(BWAPI::UnitType type)
{
	return !(commanderReference->requestedBuilding(type) || commanderReference->checkUnitIsBeingWarpedIn(type) || commanderReference->checkUnitIsPlanned(type));
}

bool StrategyManager::metProductionGoal(FriendlyBuildingCounter buildings)
{
	const ProductionGoals proudctionGoal = ProtoBot_ProductionGoals.at(ProductionGoal_index);

	if (buildings.nexus >= proudctionGoal.nexusCount &&
		buildings.gateway >= proudctionGoal.gatewayCount &&
		buildings.forge >= proudctionGoal.forgeCount &&
		buildings.cyberneticsCore >= proudctionGoal.cyberneticsCount &&
		buildings.roboticsFacility >= proudctionGoal.roboticsCount &&
		buildings.observatory >= proudctionGoal.observatoryCount &&
		buildings.citadelOfAdun >= proudctionGoal.citadelCount &&
		buildings.templarArchives >= proudctionGoal.templarArchivesCount)
	{
		return true;
	}

	return false;
}

bool StrategyManager::canAfford(BWAPI::UnitType building, std::pair<int, int> resources)
{
	if (resources.first - building.mineralPrice() >= 0 && resources.second - building.gasPrice() >= 0) return true;

	return false;
}

//More logic to this later
bool StrategyManager::shouldGasSteal()
{
	return true;
}

bool StrategyManager::haveRequiredTech(BWAPI::UnitType unit)
{
	const FriendlyBuildingCounter ProtoBot_Buildings = InformationManager::Instance().getFriendlyBuildingCounter();
	const std::map<BWAPI::UnitType, int> requiredBuildings = unit.requiredUnits();

	bool haveRequiredTech = true;

	for (const std::pair<const BWAPI::UnitType, int> requiredBuilding : requiredBuildings)
	{
		switch (requiredBuilding.first)
		{
		case BWAPI::UnitTypes::Protoss_Gateway:
			if (ProtoBot_Buildings.gateway < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Forge:
			if (ProtoBot_Buildings.forge < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
			if (ProtoBot_Buildings.cyberneticsCore < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Robotics_Facility:
			if (ProtoBot_Buildings.roboticsFacility < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
			if (ProtoBot_Buildings.citadelOfAdun < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Templar_Archives:
			if (ProtoBot_Buildings.templarArchives < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Stargate:
			if (ProtoBot_Buildings.stargate < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Observatory:
			if (ProtoBot_Buildings.observatory < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Robotics_Support_Bay:
			if (ProtoBot_Buildings.roboticsSupportBay < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Fleet_Beacon:
			if (ProtoBot_Buildings.fleetBeacon < requiredBuilding.second) haveRequiredTech = false;
			break;
		case BWAPI::UnitTypes::Protoss_Arbiter_Tribunal:
			if (ProtoBot_Buildings.arbiterTribunal < requiredBuilding.second) haveRequiredTech = false;
			break;
		default:
			break;
		}
	}

	return haveRequiredTech;
}