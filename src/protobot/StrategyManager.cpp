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
			if (areas.first == mainArea || areas.second == mainArea || choke->Blocked() || choke->BlockingNeutral()) continue;

			ProtoBotArea_SquadPlacements.insert(choke);
			PositionsFilled.insert({ choke, false });
		}
	}

	//std::cout << "ProtoBot areas: " << ProtoBot_Areas.size() << "\n";

	spenderManager.onStart();
}

bool StrategyManager::checkAlreadyRequested(BWAPI::UnitType type)
{
	return (!commanderReference->requestedBuilding(type)
		&& !(commanderReference->checkUnitIsBeingWarpedIn(type)
			|| commanderReference->checkUnitIsPlanned(type)));
}

std::vector<Action> StrategyManager::onFrame(std::vector<ResourceRequest> &resourceRequests)
{
	//Might need to move this.
	spenderManager.OnFrame(resourceRequests);

	std::vector<Action> actionsToReturn;

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

	//Building logic
	const bool buildOrderCompleted = commanderReference->buildOrderCompleted();

	if (!(minutesPassedIndex == expansionTimes.size()) && expansionTimes.at(minutesPassedIndex) <= minutes && !buildOrderCompleted) minutesPassedIndex++;

    //ProtoBot unit information
	const FriendlyBuildingCounter ProtoBot_buildings = InformationManager::Instance().getFriendlyBuildingCounter();
	const FriendlyUnitCounter ProtoBot_units = InformationManager::Instance().getFriendlyUnitCounter();
	const FriendlyUpgradeCounter ProtoBot_upgrade = InformationManager::Instance().getFriendlyUpgradeCounter();
	const FriendlyTechCounter ProtoBot_tech = InformationManager::Instance().getFriendlyTechCounter();
	std::vector<Squad*> Protobot_IdleSquads = commanderReference->combatManager.IdleSquads;
	std::vector<Squad*> Protobot_Squads = commanderReference->combatManager.Squads;
	int numberFullSquads = 0;

	for (const Squad* squad : Protobot_Squads)
	{
		if (squad->units.size() == MAX_SQUAD_SIZE) numberFullSquads++;
	}

	//std::cout << "Reserving " << getTotalMineralsNeeded() << " out of " << BWAPI::Broodwar->self()->minerals() << "\n";

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

	if (buildOrderCompleted)
	{
		//Pylon requests, Once build order is completed run this method to make sure we have enough supply to do things.
		if (commanderReference->checkAvailableSupply() <= dynamicSupplyThreshold && ((BWAPI::Broodwar->self()->supplyTotal() / 2) != MAX_SUPPLY))
		{
			//std::cout << "EXPAND ACTION: Requesting to build Pylon\n";

			ResourceRequest buildPylon;
			buildPylon.type = ResourceRequest::Type::Building;
			buildPylon.unit = BWAPI::UnitTypes::Protoss_Pylon;
			resourceRequests.push_back(buildPylon);
		}

		//Assimilators for nexus's as well if we need any.
		for (const NexusEconomy& nexusEconomy : nexusEconomies)
		{
			if (nexusEconomy.vespeneGyser != nullptr
				&& nexusEconomy.assimilator == nullptr
				&& nexusEconomy.workers.size() >= nexusEconomy.minerals.size() + 3
				&& nexusEconomy.lifetime >= 500
				&& checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Assimilator))
			{
				//std::cout << "EXPAND ACTION: Checking nexus economy " << nexusEconomy.nexusID << " needs assimilator\n";

				ResourceRequest buildAssimilator;
				buildAssimilator.type = ResourceRequest::Type::Building;
				buildAssimilator.unit = BWAPI::UnitTypes::Protoss_Assimilator;
				resourceRequests.push_back(buildAssimilator);
			}
		}
	}

	//First is minerals avalible, Second is gas avalible
	std::pair<int, int> resourcesAvalible = std::make_pair(spenderManager.getPlannedMinerals(resourceRequests), spenderManager.getPlannedGas(resourceRequests));
	const ProtoBotRequestCounter requests = commanderReference->buildManager.requestCounter;

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
					ResourceRequest buildNexus;
					buildNexus.type = ResourceRequest::Type::Building;
					buildNexus.unit = BWAPI::UnitTypes::Protoss_Nexus;
					resourceRequests.push_back(buildNexus);

					resourcesAvalible.first -= buildNexus.unit.mineralPrice();
					resourcesAvalible.second -= buildNexus.unit.gasPrice();
				}

				if (BWAPI::Broodwar->self()->minerals() > mineralExcessToExpand)
				{
					mineralExcessToExpand *= 2;
					//std::cout << "EXPAND ACTION: Requesting to expand (mineral surplus)\n";

					ResourceRequest buildNexus;
					buildNexus.type = ResourceRequest::Type::Building;
					buildNexus.unit = BWAPI::UnitTypes::Protoss_Nexus;
					resourceRequests.push_back(buildNexus);

					resourcesAvalible.first -= buildNexus.unit.mineralPrice();
					resourcesAvalible.second -= buildNexus.unit.gasPrice();
				}

				if (!(minutesPassedIndex == expansionTimes.size()) && expansionTimes.at(minutesPassedIndex) <= minutes)
				{
					//std::cout << "EXPAND ACTION: Requesting to expand (expansion time " << expansionTimes.at(minutesPassedIndex) << ")\n";
					minutesPassedIndex++;

					ResourceRequest buildNexus;
					buildNexus.type = ResourceRequest::Type::Building;
					buildNexus.unit = BWAPI::UnitTypes::Protoss_Nexus;
					resourceRequests.push_back(buildNexus);

					resourcesAvalible.first -= buildNexus.unit.mineralPrice();
					resourcesAvalible.second -= buildNexus.unit.gasPrice();
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
					ResourceRequest buildGateway;
					buildGateway.type = ResourceRequest::Type::Building;
					buildGateway.unit = BWAPI::UnitTypes::Protoss_Gateway;
					resourceRequests.push_back(buildGateway);

					resourcesAvalible.first -= buildGateway.unit.mineralPrice();
					resourcesAvalible.second -= buildGateway.unit.gasPrice();
				}
			}

			timer++;
		}
		else if(ProtoBot_ProductionFocus == ProductionFocus::EXPANDING_INFLUENCE && !(ProductionGoal_index >= ProtoBot_ProductionGoals.size()))
		{
			//Add timer to push production goals after a certain amount of frames
			if (!metProductionGoal(ProtoBot_buildings))
			{
				//Nexus
				//On expanding influence we should create a new nexus immedietly
				if (ProtoBot_buildings.nexus + incompleteBuildings.nexus + requests.nexus_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).nexusCount)
				{
					ResourceRequest buildNexus;
					buildNexus.type = ResourceRequest::Type::Building;
					buildNexus.unit = BWAPI::UnitTypes::Protoss_Nexus;
					resourceRequests.push_back(buildNexus);

					resourcesAvalible.first -= buildNexus.unit.mineralPrice();
					resourcesAvalible.second -= buildNexus.unit.gasPrice();
				}

				//Gateway
				if (checkTechTree(BWAPI::UnitTypes::Protoss_Gateway, ProtoBot_buildings) &&
					ProtoBot_buildings.gateway + incompleteBuildings.gateway + requests.gateway_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).gatewayCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Gateway, resourcesAvalible) &&
					ProtoBot_buildings.nexus >= ProtoBot_ProductionGoals.at(ProductionGoal_index).nexusCount)
				{
					ResourceRequest buildGateway;
					buildGateway.type = ResourceRequest::Type::Building;
					buildGateway.unit = BWAPI::UnitTypes::Protoss_Gateway;
					resourceRequests.push_back(buildGateway);

					resourcesAvalible.first -= buildGateway.unit.mineralPrice();
					resourcesAvalible.second -= buildGateway.unit.gasPrice();
				}

				//Forge
				if (checkTechTree(BWAPI::UnitTypes::Protoss_Forge, ProtoBot_buildings) &&
					ProtoBot_buildings.forge + incompleteBuildings.forge + requests.forge_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).forgeCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Forge, resourcesAvalible) &&
					ProtoBot_buildings.gateway >= ProtoBot_ProductionGoals.at(ProductionGoal_index).gatewayCount)
				{
					ResourceRequest buildForge;
					buildForge.type = ResourceRequest::Type::Building;
					buildForge.unit = BWAPI::UnitTypes::Protoss_Forge;
					resourceRequests.push_back(buildForge);

					resourcesAvalible.first -= buildForge.unit.mineralPrice();
					resourcesAvalible.second -= buildForge.unit.gasPrice();
				}

				//Cybernetics
				if (checkTechTree(BWAPI::UnitTypes::Protoss_Cybernetics_Core, ProtoBot_buildings) &&
					ProtoBot_buildings.cyberneticsCore + incompleteBuildings.cyberneticsCore + requests.cybernetics_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).cyberneticsCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Cybernetics_Core, resourcesAvalible) &&
					ProtoBot_buildings.forge >= ProtoBot_ProductionGoals.at(ProductionGoal_index).forgeCount)
				{
					ResourceRequest buildCybernetics;
					buildCybernetics.type = ResourceRequest::Type::Building;
					buildCybernetics.unit = BWAPI::UnitTypes::Protoss_Cybernetics_Core;
					resourceRequests.push_back(buildCybernetics);

					resourcesAvalible.first -= buildCybernetics.unit.mineralPrice();
					resourcesAvalible.second -= buildCybernetics.unit.gasPrice();
				}

				//Robotics
				if (checkTechTree(BWAPI::UnitTypes::Protoss_Robotics_Facility, ProtoBot_buildings) &&
					ProtoBot_buildings.roboticsFacility + incompleteBuildings.roboticsFacility + requests.robotics_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).roboticsCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Robotics_Facility, resourcesAvalible) &&
					ProtoBot_buildings.cyberneticsCore >= ProtoBot_ProductionGoals.at(ProductionGoal_index).cyberneticsCount)
				{
					ResourceRequest buildRobotics;
					buildRobotics.type = ResourceRequest::Type::Building;
					buildRobotics.unit = BWAPI::UnitTypes::Protoss_Robotics_Facility;
					resourceRequests.push_back(buildRobotics);

					resourcesAvalible.first -= buildRobotics.unit.mineralPrice();
					resourcesAvalible.second -= buildRobotics.unit.gasPrice();
				}

				//Observatory
				if (checkTechTree(BWAPI::UnitTypes::Protoss_Observatory, ProtoBot_buildings) &&
					ProtoBot_buildings.observatory + incompleteBuildings.observatory + requests.observatory_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).observatoryCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Observatory, resourcesAvalible) &&
					ProtoBot_buildings.roboticsFacility >= ProtoBot_ProductionGoals.at(ProductionGoal_index).roboticsCount)
				{
					ResourceRequest buildObservatory;
					buildObservatory.type = ResourceRequest::Type::Building;
					buildObservatory.unit = BWAPI::UnitTypes::Protoss_Observatory;
					resourceRequests.push_back(buildObservatory);

					resourcesAvalible.first -= buildObservatory.unit.mineralPrice();
					resourcesAvalible.second -= buildObservatory.unit.gasPrice();
				}

				//Citadel
				if (checkTechTree(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, ProtoBot_buildings) &&
					ProtoBot_buildings.citadelOfAdun + incompleteBuildings.citadelOfAdun + requests.citadel_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).citadelCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, resourcesAvalible) &&
					ProtoBot_buildings.observatory >= ProtoBot_ProductionGoals.at(ProductionGoal_index).observatoryCount)
				{
					ResourceRequest buildCitadel;
					buildCitadel.type = ResourceRequest::Type::Building;
					buildCitadel.unit = BWAPI::UnitTypes::Protoss_Citadel_of_Adun;
					resourceRequests.push_back(buildCitadel);

					resourcesAvalible.first -= buildCitadel.unit.mineralPrice();
					resourcesAvalible.second -= buildCitadel.unit.gasPrice();
				}

				//Templar Archives
				if (checkTechTree(BWAPI::UnitTypes::Protoss_Templar_Archives, ProtoBot_buildings) &&
					ProtoBot_buildings.templarArchives + incompleteBuildings.templarArchives + requests.templarArchives_requests < ProtoBot_ProductionGoals.at(ProductionGoal_index).templarArchivesCount &&
					canAfford(BWAPI::UnitTypes::Protoss_Templar_Archives, resourcesAvalible) &&
					ProtoBot_buildings.citadelOfAdun >= ProtoBot_ProductionGoals.at(ProductionGoal_index).citadelCount)
				{
					ResourceRequest buildTemplarArchives;
					buildTemplarArchives.type = ResourceRequest::Type::Building;
					buildTemplarArchives.unit = BWAPI::UnitTypes::Protoss_Templar_Archives;
					resourceRequests.push_back(buildTemplarArchives);

					resourcesAvalible.first -= buildTemplarArchives.unit.mineralPrice();
					resourcesAvalible.second -= buildTemplarArchives.unit.gasPrice();
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
					ResourceRequest buildNexus;
					buildNexus.type = ResourceRequest::Type::Building;
					buildNexus.unit = BWAPI::UnitTypes::Protoss_Nexus;
					resourceRequests.push_back(buildNexus);
				}

				if (BWAPI::Broodwar->self()->minerals() > mineralExcessToExpand)
				{
					mineralExcessToExpand *= 2;
					//std::cout << "EXPAND ACTION: Requesting to expand (mineral surplus)\n";

					ResourceRequest buildNexus;
					buildNexus.type = ResourceRequest::Type::Building;
					buildNexus.unit = BWAPI::UnitTypes::Protoss_Nexus;
					resourceRequests.push_back(buildNexus);
				}

				if (!(minutesPassedIndex == expansionTimes.size()) && expansionTimes.at(minutesPassedIndex) <= minutes)
				{
					//std::cout << "EXPAND ACTION: Requesting to expand (expansion time " << expansionTimes.at(minutesPassedIndex) << ")\n";
					minutesPassedIndex++;

					ResourceRequest buildNexus;
					buildNexus.type = ResourceRequest::Type::Building;
					buildNexus.unit = BWAPI::UnitTypes::Protoss_Nexus;
					resourceRequests.push_back(buildNexus);
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
					ResourceRequest buildAssimilator;
					buildAssimilator.type = ResourceRequest::Type::Building;
					buildAssimilator.unit = BWAPI::UnitTypes::Protoss_Assimilator;
					resourceRequests.push_back(buildAssimilator);
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

					ResourceRequest buildGateway;
					buildGateway.type = ResourceRequest::Type::Building;
					buildGateway.unit = BWAPI::UnitTypes::Protoss_Gateway;
					resourceRequests.push_back(buildGateway);
				}
			}

			//1 forge for now but if we want upgrades 2/3 for armor and weapons we need to know when to go some of these buildings.
			if (checkTechTree(BWAPI::UnitTypes::Protoss_Forge, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Forge) && ProtoBot_buildings.forge < 1 && (ProtoBot_buildings.gateway >= 1))
			{
				//std::cout << "BUILD ACTION: Requesting to warp Forge\n";

				ResourceRequest buildForge;
				buildForge.type = ResourceRequest::Type::Building;
				buildForge.unit = BWAPI::UnitTypes::Protoss_Forge;
				resourceRequests.push_back(buildForge);
			}

			//Only need 1 cybernetics core
			if (checkTechTree(BWAPI::UnitTypes::Protoss_Cybernetics_Core, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Cybernetics_Core) && ProtoBot_buildings.cyberneticsCore < 1 && ProtoBot_buildings.gateway >= 1)
			{
				//std::cout << "build action: requesting to warp forge\n";

				ResourceRequest buildCybernetics;
				buildCybernetics.type = ResourceRequest::Type::Building;
				buildCybernetics.unit = BWAPI::UnitTypes::Protoss_Cybernetics_Core;
				resourceRequests.push_back(buildCybernetics);
			}

			//Should only build 1 robotics facility
			if (checkTechTree(BWAPI::UnitTypes::Protoss_Robotics_Facility, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Robotics_Facility) && ProtoBot_buildings.roboticsFacility < 1 && ProtoBot_buildings.cyberneticsCore >= 1)
			{
				//std::cout << "build action: requesting to warp robotics facility\n";

				ResourceRequest buildRobotics;
				buildRobotics.type = ResourceRequest::Type::Building;
				buildRobotics.unit = BWAPI::UnitTypes::Protoss_Robotics_Facility;
				resourceRequests.push_back(buildRobotics);
			}

			//Only 1 observatory
			if (checkTechTree(BWAPI::UnitTypes::Protoss_Observatory, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Observatory) && ProtoBot_buildings.observatory < 1 && ProtoBot_buildings.roboticsFacility >= 1)
			{
				//std::cout << "Build Action: requesting to warp observatory\n";

				ResourceRequest buildObservatory;
				buildObservatory.type = ResourceRequest::Type::Building;
				buildObservatory.unit = BWAPI::UnitTypes::Protoss_Observatory;
				resourceRequests.push_back(buildObservatory);
			}

			//2/3 Forge upgrade units

			if (checkTechTree(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) && ProtoBot_buildings.citadelOfAdun < 1 && ProtoBot_buildings.cyberneticsCore >= 1)
			{
				//std::cout << "build action: requesting to warp citadel of adun\n";

				ResourceRequest buildCitadel;
				buildCitadel.type = ResourceRequest::Type::Building;
				buildCitadel.unit = BWAPI::UnitTypes::Protoss_Citadel_of_Adun;
				resourceRequests.push_back(buildCitadel);
			}

			if (checkTechTree(BWAPI::UnitTypes::Protoss_Templar_Archives, ProtoBot_buildings) && checkAlreadyRequested(BWAPI::UnitTypes::Protoss_Templar_Archives) && ProtoBot_buildings.templarArchives < 1 && ProtoBot_buildings.citadelOfAdun >= 1)
			{
				//std::cout << "build action: requesting to warp templar archives\n";

				ResourceRequest buildTemplarArchives;
				buildTemplarArchives.type = ResourceRequest::Type::Building;
				buildTemplarArchives.unit = BWAPI::UnitTypes::Protoss_Templar_Archives;
				resourceRequests.push_back(buildTemplarArchives);
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

#pragma region Attack
	// If we have more than two full squads attack. 
	const int totalSupply = BWAPI::Broodwar->self()->supplyTotal() / 2;
	const int supplyUsed = BWAPI::Broodwar->self()->supplyUsed() / 2;
	
	// Boolean used to stop reinforcement action from triggering
	bool isFinalAttack = false;

	//Add timer on supply cap to make us attack so we dont waste time.
	if (supplyUsed >= 150 || (totalSupply == MAX_SUPPLY && supplyUsed + 1 == MAX_SUPPLY))
	{
		if (commanderReference->combatManager.allUnits.size() >= (MAX_SQUAD_SIZE * NUM_SQUADS_TO_ATTACK))
		{
			isFinalAttack = true;

			
			BWAPI::Position attackPos;

			// Prioritize attacking known enemy buildings
			
            for (const auto& pair : InformationManager::Instance().getKnownEnemyBuildings()) {
				if (pair.second.destroyed) {
					continue;
				}

				if (!pair.first->isVisible()) {
					attackPos = pair.second.lastKnownPosition;
					break;
				}
				else if (pair.first->exists()) {
					attackPos = pair.first->getPosition();
					break;
				}
			}

			// If no valid attack position, pick enemy base first if there is any
			if (attackPos == BWAPI::Positions::Invalid && enemyBaselocations.size() > 0){
				attackPos = enemyBaselocations.at(enemyBaselocations.size()-1);
			}

			// Send action only if theres a new, valid attack position
			if (attackPos != lastAttackPos && attackPos != BWAPI::Positions::Invalid) {
				Action attack;
				attack.type = Action::ACTION_ATTACK;
				attack.attackPosition = attackPos;
				actionsToReturn.push_back(attack);

				lastAttackPos = attackPos;
			}
		}
	}
#pragma endregion

#pragma region Defend
	// Only check for defensive positions and actions if we're not in the final attack phase
	if (!isFinalAttack) {
		BWAPI::Unitset unitsOnVisison = BWAPI::Broodwar->enemy()->getUnits();

		bool enemyAttacking = false;
		BWAPI::Unit unitToAttack;

		for (const BWAPI::Unit unit : unitsOnVisison)
		{
			if (!unit->exists() || unit == nullptr) {
				continue;
			}

			if (unit->getType().isBuilding()) continue;

			unitToAttack = unit;
			const BWEM::Area* enemyAreaLocation = theMap.GetNearestArea(unit->getTilePosition());

			for (const BWEM::Area* area : ProtoBot_Areas)
			{
				if (enemyAreaLocation == area) enemyAttacking = true;
			}
		}

		if (enemyAttacking)
		{
			if (unitToAttack->exists() || unitToAttack != nullptr) {
				Action attack;
				attack.type = Action::ACTION_REINFORCE;
				attack.reinforcePosition = unitToAttack->getPosition();
				actionsToReturn.push_back(attack);
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
	}
#pragma endregion

	return actionsToReturn;
}

BWAPI::Unitset StrategyManager::getProtoBotBuildings()
{
	const BWAPI::Unitset ProtoBot_units = BWAPI::Broodwar->self()->getUnits();
	BWAPI::Unitset ProtoBot_buildings;

	for (BWAPI::Unit unit : ProtoBot_units)
	{
		if (unit->getType().isBuilding() && unit->isCompleted())
		{
			ProtoBot_buildings.insert(unit);
		}
	}

	return ProtoBot_buildings;
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
				if (areas.first == mainArea || areas.second == mainArea || choke->Blocked() || choke->BlockingNeutral()) continue;

				ProtoBotArea_SquadPlacements.insert(choke);
			}
		}

	}
}

void StrategyManager::onUnitCreate(BWAPI::Unit unit)
{
	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	switch (unit->getType())
	{
		case BWAPI::UnitTypes::Protoss_Arbiter_Tribunal:
			incompleteBuildings.arbiterTribunal++;
			break;
		case BWAPI::UnitTypes::Protoss_Assimilator:
			incompleteBuildings.assimilator++;
			break;
		case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
			incompleteBuildings.citadelOfAdun++;
			break;
		case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
			incompleteBuildings.cyberneticsCore++;
			break;
		case BWAPI::UnitTypes::Protoss_Fleet_Beacon:
			incompleteBuildings.fleetBeacon++;
			break;
		case BWAPI::UnitTypes::Protoss_Forge:
			incompleteBuildings.forge++;
			break;
		case BWAPI::UnitTypes::Protoss_Gateway:
			incompleteBuildings.gateway++;
			break;
		case BWAPI::UnitTypes::Protoss_Nexus:
			incompleteBuildings.nexus++;
			break;
		case BWAPI::UnitTypes::Protoss_Observatory:
			incompleteBuildings.observatory++;
			break;
		case BWAPI::UnitTypes::Protoss_Photon_Cannon:
			incompleteBuildings.photonCannon++;
			break;
		case BWAPI::UnitTypes::Protoss_Pylon:
			incompleteBuildings.pylon++;
			break;
		case BWAPI::UnitTypes::Protoss_Robotics_Facility:
			incompleteBuildings.roboticsFacility++;
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
			break;
		default:
			break;
	}

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
				if (areas.first == mainArea || areas.second == mainArea || choke->Blocked() || choke->BlockingNeutral()) continue;

				ProtoBotArea_SquadPlacements.insert(choke);
				PositionsFilled.insert({ choke, false });
			}
		}
	}

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