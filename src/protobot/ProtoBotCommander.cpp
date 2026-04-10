#include "ProtoBotCommander.h"
#include "ScoutPolicy.h"

ProtoBotCommander::ProtoBotCommander() : buildManager(this), strategyManager(this), economyManager(this), scoutingManager(this), combatManager(this)
{
	// Initialize singleton InformationManager with this commander reference
	InformationManager::Instance().SetCommander(this);

}

#pragma region BWAPI EVENTS
void ProtoBotCommander::onStart()
{
	//std::cout << "============================\n";
	//std::cout << "Initializing Modules\n";

	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/

	// Set our BWAPI options here    
	BWAPI::Broodwar->setLocalSpeed(10);
	BWAPI::Broodwar->setFrameSkip(0);

	// Enable the flag that tells BWAPI to let users enter input while bot plays
	BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);

	static bool mapInitialized = false;

	//std::cout << "Map initialization...\n";

	//theMap = BWEM::Map::Instance();
	theMap.Initialize();
	theMap.EnableAutomaticPathAnalysis();
	bool startingLocationsOK = theMap.FindBasesForStartingLocations();
	assert(startingLocationsOK);

	BWEB::Map::onStart();
	BWEB::Blocks::findBlocks();

	m_mapTools.onStart();

	const BWAPI::Unitset units = BWAPI::Broodwar->self()->getUnits();

	//Get nexus and create a new instace of a NexusEconomy
	//Need to do this because when units are created at the beggining of the game a nexus economy does not exist.
	for (BWAPI::Unit unit : units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus)
		{
			economyManager.assignUnit(unit);
		}
	}

	/*
	* Protobot Modules
	*/
    InformationManager::Instance().onStart();
	strategyManager.onStart();
	economyManager.onStart();
	scoutingManager.onStart();
	buildManager.onStart();
	combatManager.onStart();

	resourceRequests.clear();
	//std::cout << "============================\n";
	//std::cout << "Agent Start\n";
}

void ProtoBotCommander::onFrame()
{
	// Draw unit health bars, which brood war unfortunately does not do
	Tools::DrawUnitHealthBars();

	// Draw some relevent information to the screen to help us debug the bot
	drawDebugInformation();

	//BWEB::Map::draw();

	removeApprovedRequests();

	//Draw circle over nexus's that have a request
	for (const ResourceRequest& request : resourceRequests)
	{
		if (request.nexusPositionRef != BWAPI::Positions::Invalid 
			&& request.base != nullptr)
		{
			BWAPI::Broodwar->drawCircleMap(request.nexusPositionRef, 10, BWAPI::Colors::Red, true);
		}
	}

	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/
	timerManager.startTimer(TimerManager::All);

	// Update our MapTools information
	timerManager.startTimer(TimerManager::MapTools);
	m_mapTools.onFrame();
	timerManager.stopTimer(TimerManager::MapTools);

	/*
	* Protobot Modules
	*/

	timerManager.startTimer(TimerManager::Information);
    InformationManager::Instance().onFrame();
	timerManager.stopTimer(TimerManager::Information);

	timerManager.startTimer(TimerManager::Economy);
	economyManager.onFrame();
	timerManager.stopTimer(TimerManager::Economy);

	timerManager.startTimer(TimerManager::Strategy);
	std::vector<Action> actions = strategyManager.onFrame(resourceRequests);

	bool issuedScoutThisFrame = false;

	for (const Action& action : actions)
	{
		switch (action.type)
		{
		case Action::ACTION_SCOUT:
			if (!issuedScoutThisFrame)
			{
				BWAPI::Unit scout = getUnitToScout();
				if (scout)
				{
					issuedScoutThisFrame = true;
				}
			}
			break;
		case Action::ACTION_ATTACK:
			combatManager.attack(action.attackPosition);
			break;
		case Action::ACTION_DEFEND:
			combatManager.defend(action.defendPosition);
			break;
		case Action::ACTION_REINFORCE:
			combatManager.reinforce(action.reinforcePosition);
			break;
		}
	}
	timerManager.stopTimer(TimerManager::Strategy);

	timerManager.startTimer(TimerManager::Build);
	buildManager.onFrame(resourceRequests);
	timerManager.stopTimer(TimerManager::Build);

	//Uncomment this once onFrame does not steal a worker.
	timerManager.startTimer(TimerManager::Scouting);
	scoutingManager.onFrame();
	timerManager.stopTimer(TimerManager::Scouting);

	timerManager.startTimer(TimerManager::Combat);
	combatManager.onFrame();
	timerManager.stopTimer(TimerManager::Combat);

	timerManager.stopTimer(TimerManager::All);
}

void ProtoBotCommander::onEnd(bool isWinner)
{
	// Important: Clear all caches BWEB pointers upon game end, otherwise invalid = crash
	BWEB::Map::onEnd();
}

/*
* When a unit is destroyed send a broadcast out to all modules.
*
* Each module will check if the unit is a assigned to their module. If the unit is, remove the unit. Others drop the message.
*/
void ProtoBotCommander::onUnitDestroy(BWAPI::Unit unit)
{
	//Managers that deal with unit assignments
	economyManager.onUnitDestroy(unit);
	combatManager.onUnitDestroy(unit);
	scoutingManager.onUnitDestroy(unit);

	//Managers that deal with unit information updates
	strategyManager.onUnitDestroy(unit);
	InformationManager::Instance().onUnitDestroy(unit);
	buildManager.onUnitDestroy(unit);
}

void ProtoBotCommander::onUnitMorph(BWAPI::Unit unit)
{
	InformationManager::Instance().onUnitMorph(unit);

	//Remove unit from resource requests. Refinery sends an onUnitMorph event not a seperate onCreate/onComplete event
	if (unit->getPlayer() == BWAPI::Broodwar->self())
	{
		for (ResourceRequest& request : resourceRequests)
		{
			if (request.base != nullptr)
			{
				if (request.state == ResourceRequest::State::Approved_BeingBuilt &&
					request.unit == unit->getType())
				{
					for (const BWEM::Geyser* geyer : request.base->Geysers())
					{
						if (unit->getPosition() == geyer->Pos())
						{
							request.state = ResourceRequest::State::Accepted_Completed;

						}
					}
				}
			}
			else
			{
				if (request.state == ResourceRequest::State::Approved_BeingBuilt &&
					request.unit == unit->getType() &&
					request.base == nullptr)
				{
					request.state = ResourceRequest::State::Accepted_Completed;
				}
			}
		}
	}

	//Remove builder
	buildManager.onUnitMorph(unit);
}

void ProtoBotCommander::onSendText(std::string text)
{
	if (text == "/map")
	{
		m_mapTools.toggleDraw();
	}
	
	if (text == "/unitdebug_on")
	{
		drawUnitDebug = true;
	}
	
	if (text == "/unitdebug_off")
	{
		drawUnitDebug = false;
	}

	combatManager.onSendText(text);
}

void ProtoBotCommander::onUnitCreate(BWAPI::Unit unit)
{
	if (unit == nullptr) return;

	buildManager.onUnitCreate(unit);
	InformationManager::Instance().onUnitCreate(unit);
	strategyManager.onUnitCreate(unit);

	//Update requests
	if (unit->getPlayer() == BWAPI::Broodwar->self())
	{
		for (ResourceRequest& request : resourceRequests)
		{
			if (request.state == ResourceRequest::State::Approved_BeingBuilt &&
				request.unit == unit->getType())
			{
				request.state = ResourceRequest::State::Accepted_Completed;
			}
			else if (request.state == ResourceRequest::State::Approved_BeingBuilt &&
				request.unit == unit->getType())
			{
				request.state = ResourceRequest::State::Accepted_Completed;
			}
		}
	}

	if (unit->getPlayer() == BWAPI::Broodwar->self())
	{
		switch (unit->getType())
		{
		case BWAPI::UnitTypes::Protoss_Gateway:
			if (requestCounter.gateway_requests > 0)
				--requestCounter.gateway_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Nexus:
			if (requestCounter.nexus_requests > 0)
				--requestCounter.nexus_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Forge:
			if (requestCounter.forge_requests > 0)
				--requestCounter.forge_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
			if (requestCounter.cybernetics_requests > 0)
				--requestCounter.cybernetics_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Robotics_Facility:
			if (requestCounter.robotics_requests > 0)
				--requestCounter.robotics_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Observatory:
			if (requestCounter.observatory_requests > 0)
				--requestCounter.observatory_requests;
			break;

		case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
			if (requestCounter.citadel_requests > 0)
				--requestCounter.citadel_requests;
			break;

		case BWAPI::UnitTypes::Protoss_Templar_Archives:
			if (requestCounter.templarArchives_requests > 0)
				--requestCounter.templarArchives_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Probe:
			if(requestCounter.worker_requests > 0)
				--requestCounter.worker_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Zealot:
			if (requestCounter.zealots_requests > 0)
				--requestCounter.zealots_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Dragoon:
			if (requestCounter.dragoons_requests > 0)
				--requestCounter.dragoons_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Observer:
			if (requestCounter.observers_requests > 0)
				--requestCounter.observers_requests;
			break;
		case BWAPI::UnitTypes::Protoss_Dark_Templar:
			if (requestCounter.dark_templars_requests > 0)
				--requestCounter.dark_templars_requests;
			break;
		default:
			break;
		}
	}
}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{
	//std::cout << "ProtoBot onUnitComplete: " << unit->getType() << "\n";

	strategyManager.onUnitComplete(unit);
    InformationManager::Instance().onUnitComplete(unit);

	if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

	const BWAPI::UnitType unit_type = unit->getType();

	//We will let the Ecconomy Manager exclusivly deal with all ecconomy units (Nexus, Assimilator, Probe).
	if (unit_type == BWAPI::UnitTypes::Protoss_Nexus || unit_type == BWAPI::UnitTypes::Protoss_Assimilator || unit_type == BWAPI::UnitTypes::Protoss_Probe)
	{
		//std::cout << "Calling onComplete for " << unit_type << " " << unit->getID() << "\n";
		economyManager.assignUnit(unit);
		buildManager.onUnitComplete(unit);
		return;
	}

	if (unit_type.isBuilding())
	{
		//std::cout << "Calling onComplete for " << unit_type << " " << unit->getID() << "\n";
		buildManager.onUnitComplete(unit);
		return;
	}

	if (unit_type == BWAPI::UnitTypes::Protoss_Observer)
	{
		if (scoutingManager.canAcceptObserverScout())
		{
			scoutingManager.assignScout(unit);
			//BWAPI::Broodwar->printf("[Commander] Assigned Observer %d to Scouting", unit->getID());
			return;
		}
	}

	/*
	if (unit_type == BWAPI::UnitTypes::Protoss_Zealot || unit_type == BWAPI::UnitTypes::Protoss_Dragoon)
	{
		if (scoutingManager.canAcceptCombatScout(unit_type))
		{
			scoutingManager.assignScout(unit);
			BWAPI::Broodwar->printf("[Commander] Assigned %s %d to Scouting", unit_type.c_str(), unit->getID());
			return;
		}
	}*/

	if (unit_type == BWAPI::UnitTypes::Protoss_Observer)
	{
		if (scoutingManager.canAcceptObserverScout())
		{
			scoutingManager.assignScout(unit);
			//BWAPI::Broodwar->printf("[Commander] Assigned %s %d to Scouting", unit_type.c_str(), unit->getID());
			return;
		}
	}

	if (unit_type == BWAPI::UnitTypes::Protoss_Dark_Templar)
	{
		scoutingManager.assignScout(unit);
		return;
	}

	//Gone through all cases assume it is a combat unit 
	combatManager.assignUnit(unit);
}

void ProtoBotCommander::onUnitShow(BWAPI::Unit unit)
{
	buildManager.onUnitDiscover(unit);
}

void ProtoBotCommander::onUnitHide(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitRenegade(BWAPI::Unit unit)
{

}

void ProtoBotCommander::removeApprovedRequests()
{
	for (std::vector<ResourceRequest>::iterator it = resourceRequests.begin(); it != resourceRequests.end();)
	{
		if (it->state == ResourceRequest::State::Accepted_Completed || (it->attempts == MAX_ATTEMPTS && it->fromBuildOrder == false))
		{
			const ResourceRequest::Type request_type = it->type;
			//int mineralCost = -1;
			//int gasCost = -1;
			//std::string bwapiType_string;
			//std::string type_string;
			//std::string frame_string = (it->frameToStartBuilding == -1 ? "N\\A" : std::to_string(it->frameToStartBuilding));

			switch (request_type)
			{
			case ResourceRequest::Unit:
				/*type_string = "Unit";
				mineralCost = it->unit.mineralPrice();
				gasCost = it->unit.gasPrice();
				bwapiType_string = it->unit.toString();*/
				break;
			case ResourceRequest::Building:
				/*type_string = "Building";
				mineralCost = it->unit.mineralPrice();
				gasCost = it->unit.gasPrice();
				bwapiType_string = it->unit.toString();*/
				break;
			case ResourceRequest::Upgrade:
				/*type_string = "Upgrade";
				mineralCost = it->upgrade.mineralPrice();
				gasCost = it->upgrade.gasPrice();
				bwapiType_string = it->upgrade.toString();*/

				switch (it->upgrade)
				{
					case BWAPI::UpgradeTypes::Singularity_Charge:
						if (requestCounter.singularity_requests > 0)
							--requestCounter.singularity_requests;
						break;
					case BWAPI::UpgradeTypes::Protoss_Ground_Weapons:
						if (requestCounter.groundWeapons_requests > 0)
							--requestCounter.groundWeapons_requests;
						break;
					case BWAPI::UpgradeTypes::Protoss_Ground_Armor:
						if (requestCounter.groundArmor_requests > 0)
							--requestCounter.groundArmor_requests;
						break;
					case BWAPI::UpgradeTypes::Protoss_Plasma_Shields:
						if (requestCounter.plasmaShields_requests > 0)
							--requestCounter.plasmaShields_requests;
						break;
					case BWAPI::UpgradeTypes::Leg_Enhancements:
						if (requestCounter.legEnhancements_requests > 0)
							--requestCounter.legEnhancements_requests;
						break;
				}

				break;
			case ResourceRequest::Tech:
				/*type_string = "Tech";
				mineralCost = it->tech.mineralPrice();
				gasCost = it->tech.gasPrice();
				bwapiType_string = it->tech.toString();*/
				break;
			}

			//double seconds = double(it->frameRequestApproved - it->frameRequestCreated) / 24.0;

			/*std::cout << "Request for " << type_string << " (" << bwapiType_string << ") "
				<< "\nFrame Request Created = " << it->frameRequestCreated
				<< "\nFrame Request Approved = " << it->frameRequestApproved
				<< "\nFrame Request Serviced = " << it->frameRequestServiced 
				<< "\nFrame Request Removed = " 
				<< BWAPI::Broodwar->getFrameCount() << "\nTotal Frames to Complete = " 
				<< (it->frameRequestApproved - it->frameRequestCreated)
				<< " (" << seconds << " seconds)" << "\n";

			*/

			//Add time between frames and other stuff.

			it = resourceRequests.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void ProtoBotCommander::requestBuilding(BWAPI::UnitType building, bool fromBuildOrder, bool isWall, bool isRampPlacement, BWAPI::Position nexusPosition, const BWEM::Base* baseLocation)
{
	ResourceRequest request;
	request.type = ResourceRequest::Type::Building;
	request.unit = building;
	request.fromBuildOrder = fromBuildOrder;
	request.frameRequestCreated = BWAPI::Broodwar->getFrameCount();
	request.isWall = isWall;
	request.isRampPlacement = isRampPlacement;
	request.nexusPositionRef = nexusPosition;
	request.base = baseLocation;
	
	switch (building)
	{
		case BWAPI::UnitTypes::Protoss_Gateway:
			requestCounter.gateway_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Nexus:
			requestCounter.nexus_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Forge:
			requestCounter.forge_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
			requestCounter.cybernetics_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Robotics_Facility:
			requestCounter.robotics_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Observatory:
			requestCounter.observatory_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
			requestCounter.citadel_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Templar_Archives:
			requestCounter.templarArchives_requests++;
			break;
		default:
			break;
	}

	resourceRequests.push_back(request);

	/*if (building == BWAPI::UnitTypes::Protoss_Assimilator && fromBuildOrder == false)
	{
		std::cout << "Assimilator has been added to queue for nexus at location " << request.nexusPositionRef << "\n";
		std::cout << "Assimilators in queue:\n";

		for (const ResourceRequest& temp : resourceRequests)
		{
			if (temp.unit == BWAPI::UnitTypes::Protoss_Assimilator && fromBuildOrder == false)
			{
				std::cout << "Assimilator requested for nexus at location " << temp.nexusPositionRef << "\n";
			}
		}
		std::cout << "=================================\n";
	}*/
}

void ProtoBotCommander::requestUnit(BWAPI::UnitType unit, BWAPI::Unit buildingToTrain, bool fromBuildOrder)
{
	ResourceRequest request;
	request.type = ResourceRequest::Type::Unit;
	request.unit = unit;
	request.frameRequestCreated = BWAPI::Broodwar->getFrameCount();
	request.requestedBuilding = buildingToTrain;
	request.fromBuildOrder = fromBuildOrder;

	switch (unit)
	{
		case BWAPI::UnitTypes::Protoss_Probe:
			requestCounter.worker_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Zealot:
			requestCounter.zealots_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Dragoon:
			requestCounter.dragoons_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Observer:
			requestCounter.observers_requests++;
			break;
		case BWAPI::UnitTypes::Protoss_Dark_Templar:
			requestCounter.dark_templars_requests++;
			break;
	}	

	resourceRequests.push_back(request);
}

void ProtoBotCommander::requestUpgrade(BWAPI::Unit unit, BWAPI::UpgradeType upgrade, bool fromBuildOrder)
{
	ResourceRequest request;
	request.type = ResourceRequest::Type::Upgrade;
	request.frameRequestCreated = BWAPI::Broodwar->getFrameCount();
	request.upgrade = upgrade;
	request.requestedBuilding = unit;
	request.fromBuildOrder = fromBuildOrder;

	switch (request.upgrade)
	{
		case BWAPI::UpgradeTypes::Singularity_Charge:
				requestCounter.singularity_requests++;
			break;
		case BWAPI::UpgradeTypes::Protoss_Ground_Weapons:
				requestCounter.groundWeapons_requests++;
			break;
		case BWAPI::UpgradeTypes::Protoss_Ground_Armor:
				requestCounter.groundArmor_requests++;
			break;
		case BWAPI::UpgradeTypes::Protoss_Plasma_Shields:
				requestCounter.plasmaShields_requests++;
			break;
		case BWAPI::UpgradeTypes::Leg_Enhancements:
				requestCounter.legEnhancements_requests++;
			break;
	}

	resourceRequests.push_back(request);
}

bool ProtoBotCommander::alreadySentRequest(int unitID)
{
	for (const ResourceRequest& request : resourceRequests)
	{
		if (request.requestedBuilding != nullptr)
		{
			if (unitID == request.requestedBuilding->getID()) return true;
		}
	}
	return false;
}

bool ProtoBotCommander::requestedBuilding(BWAPI::UnitType building, BWAPI::Position nexusPosition)
{
	for (const ResourceRequest& request : resourceRequests)
	{
		if (building == request.unit && request.nexusPositionRef == nexusPosition && !request.isCheese)
		{
			//if (request.nexusPositionRef != BWAPI::Positions::Invalid) std::cout << "Nexus at " << request.nexusPositionRef << " already has assimilator requested\n";

			return true;
		}
	}
	return false;
}

bool ProtoBotCommander::upgradeAlreadyRequested(BWAPI::Unit building)
{
	for (const ResourceRequest& request : resourceRequests)
	{
		if (request.requestedBuilding != nullptr)
		{
			if (building->getID() == request.requestedBuilding->getID()) return true;
		}
	}
	return false;
}

bool ProtoBotCommander::checkUnitIsPlanned(BWAPI::UnitType building, BWAPI::Position nexusPosition)
{
	for (const ResourceRequest& request : resourceRequests)
	{
		if (building == request.unit
			&& (request.state == ResourceRequest::State::Approved_InProgress || request.state == ResourceRequest::State::PendingApproval || request.state == ResourceRequest::State::Approved_BeingBuilt)
			&& request.nexusPositionRef == nexusPosition
			&& !request.isCheese)
		{
			//if (request.nexusPositionRef != BWAPI::Positions::Invalid) std::cout << "Nexus at " << request.nexusPositionRef << " already has assimilator planned\n";

			return true;
		}
	}
	return false;
}

void ProtoBotCommander::drawResourceRequestQueue(int x, int y, bool background)
{
	if (background) BWAPI::Broodwar->drawBoxScreen(x - 5, y - 5, x + 270, y + 130, BWAPI::Colors::Black, true);
	BWAPI::Broodwar->drawTextScreen(x + 5, y - 3, "%cResource Request Queue", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x + 5, y + -2, "%c___________________________________________", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x + 5, y + 10, "%c %2s | %-10s | %4s | %4s | %14s", BWAPI::Text::White, "#", "Type", "Min", "Gas", "Frame Approved");
	BWAPI::Broodwar->drawTextScreen(x + 5, y + 14, "%c___________________________________________", BWAPI::Text::White);

	const int min_y = y + 15;
	for (size_t iter = 0; iter < 10; iter++)
	{
		if (iter < resourceRequests.size())
		{
			const ResourceRequest request = resourceRequests.at(iter);

			const ResourceRequest::Type request_type = request.type;
			int mineralCost = -1;
			int gasCost = -1;
			std::string type_string;
			std::string frame_string = (request.frameRequestApproved == -1 ? "N\\A" : std::to_string(request.frameRequestApproved));

			switch (request_type)
			{
				case ResourceRequest::Unit:
					type_string = "Unit";
					mineralCost = request.unit.mineralPrice();
					gasCost = request.unit.gasPrice();
					break;
				case ResourceRequest::Building:
					type_string = "Building";
					mineralCost = request.unit.mineralPrice();
					gasCost = request.unit.gasPrice();
					break;
				case ResourceRequest::Upgrade:
					type_string = "Upgrade";
					mineralCost = request.upgrade.mineralPrice();
					gasCost = request.upgrade.gasPrice();
					break;
				case ResourceRequest::Tech:
					type_string = "Tech";
					mineralCost = request.tech.mineralPrice();
					gasCost = request.tech.gasPrice();
					break;
			}
			BWAPI::Broodwar->drawTextScreen(x, min_y + ((iter + 1) * 10), "%c %2d   %-10s   %4d   %4d   %6s", BWAPI::Text::White, iter + 1, type_string.c_str(), mineralCost, gasCost, frame_string.c_str());
		}
		else
		{
			BWAPI::Broodwar->drawTextScreen(x, min_y + ((iter + 1) * 10), "%c %2d   %-10s   %4d   %4d   %6s", BWAPI::Text::White, iter + 1, "None", 0, 0, "N\\A");
		}
	}
}

//Only counts when completed
void ProtoBotCommander::drawUnitCount(FriendlyUnitCounter ProtoBot_unitCount, int x, int y, bool background)
{
	if (background) BWAPI::Broodwar->drawBoxScreen(x - 5, y - 5, x + 150, y + 65, BWAPI::Colors::Black, true);
	BWAPI::Broodwar->drawTextScreen(x, y, "%cCurrent Unit Count", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 1, "%c________________________", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 10, "%cWorkers = %d", BWAPI::Text::White, ProtoBot_unitCount.probe);
	BWAPI::Broodwar->drawTextScreen(x, y + 20, "%cZealots = %d", BWAPI::Text::White, ProtoBot_unitCount.zealot);
	BWAPI::Broodwar->drawTextScreen(x, y + 30, "%cDragoons = %d", BWAPI::Text::White, ProtoBot_unitCount.dragoon);
	BWAPI::Broodwar->drawTextScreen(x, y + 40, "%cObservers = %d", BWAPI::Text::White, ProtoBot_unitCount.observer);
	BWAPI::Broodwar->drawTextScreen(x, y + 50, "%cDark Templars = %d", BWAPI::Text::White, ProtoBot_unitCount.darkTemplar);

}

//Only counts when completed
void ProtoBotCommander::drawBuildingCount(FriendlyBuildingCounter ProtoBot_buildingCount, int x, int y, bool background)
{
	if (background) BWAPI::Broodwar->drawBoxScreen(x - 5, y - 5, x + 150, y + 115, BWAPI::Colors::Black, true);

	BWAPI::Broodwar->drawTextScreen(x, y, "%cCurrent Building Count", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 1, "%c________________________", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 10, "%cPylon = %d", BWAPI::Text::White, ProtoBot_buildingCount.pylon);
	BWAPI::Broodwar->drawTextScreen(x, y + 20, "%cNexus = %d", BWAPI::Text::White, ProtoBot_buildingCount.nexus);
	BWAPI::Broodwar->drawTextScreen(x, y + 30, "%cGateway = %d", BWAPI::Text::White, ProtoBot_buildingCount.gateway);
	BWAPI::Broodwar->drawTextScreen(x, y + 40, "%cForge = %d", BWAPI::Text::White, ProtoBot_buildingCount.forge);
	BWAPI::Broodwar->drawTextScreen(x, y + 50, "%cCybernetics Core = %d", BWAPI::Text::White, ProtoBot_buildingCount.cyberneticsCore);
	BWAPI::Broodwar->drawTextScreen(x, y + 60, "%cPhoton Can. = %d", BWAPI::Text::White, ProtoBot_buildingCount.photonCannon);
	BWAPI::Broodwar->drawTextScreen(x, y + 70, "%cRobotics Facil. = %d", BWAPI::Text::White, ProtoBot_buildingCount.roboticsFacility);
	BWAPI::Broodwar->drawTextScreen(x, y + 80, "%cCitadel of Adun = %d", BWAPI::Text::White, ProtoBot_buildingCount.citadelOfAdun);
	BWAPI::Broodwar->drawTextScreen(x, y + 90, "%cObservatory = %d", BWAPI::Text::White, ProtoBot_buildingCount.observatory);
	BWAPI::Broodwar->drawTextScreen(x, y + 100, "%cTemplar Arch. = %d", BWAPI::Text::White, ProtoBot_buildingCount.templarArchives);
}

//Only counts when completed
void ProtoBotCommander::drawUpgradeCount(FriendlyUpgradeCounter ProtoBot_upgradeCount, int x, int y, bool background)
{
	if (background) BWAPI::Broodwar->drawBoxScreen(x - 5, y - 5, x + 150, y + 65, BWAPI::Colors::Black, true);

	BWAPI::Broodwar->drawTextScreen(x, y, "%cCurrent Upgrade Count", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 1, "%c________________________", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 10, "%cDragoon Range = %s", BWAPI::Text::White, (ProtoBot_upgradeCount.singularityCharge ? "true" : "false"));
	BWAPI::Broodwar->drawTextScreen(x, y + 20, "%cGround Weapons = %d", BWAPI::Text::White, ProtoBot_upgradeCount.groundWeapons);
	BWAPI::Broodwar->drawTextScreen(x, y + 30, "%cGround Armor = %d", BWAPI::Text::White, ProtoBot_upgradeCount.groundArmor);
	BWAPI::Broodwar->drawTextScreen(x, y + 40, "%cPlasma Shields = %d", BWAPI::Text::White, ProtoBot_upgradeCount.plasmaShields);
	BWAPI::Broodwar->drawTextScreen(x, y + 50, "%cLeg Enhance. = %d", BWAPI::Text::White, ProtoBot_upgradeCount.legEnhancements);
}

void ProtoBotCommander::drawBwapiResourceInfo(int x, int y, bool background)
{
	if(background) BWAPI::Broodwar->drawBoxScreen(x - 5, y - 5, x + 200, y + 55, BWAPI::Colors::Black, true);

	BWAPI::Broodwar->drawTextScreen(x, y, "%cBWAPI Resource Information", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 1, "%c________________________________", BWAPI::Text::White);
	BWAPI::Broodwar->drawTextScreen(x, y + 10, "%cTotal Minerals Gathered: %d", BWAPI::Text::White, BWAPI::Broodwar->self()->gatheredMinerals());
	BWAPI::Broodwar->drawTextScreen(x, y + 20, "%cTotal Gas Gathered : %d", BWAPI::Text::White, BWAPI::Broodwar->self()->gatheredGas());
	BWAPI::Broodwar->drawTextScreen(x, y + 30, "%cTotal Minerals Spent: %d", BWAPI::Text::White, BWAPI::Broodwar->self()->spentMinerals());
	BWAPI::Broodwar->drawTextScreen(x, y + 40, "%cTotal Gas Spent : %d", BWAPI::Text::White, BWAPI::Broodwar->self()->spentGas());
}

void ProtoBotCommander::drawDebugInformation()
{
	Tools::DrawUnitCommands();
	Tools::DrawUnitBoundingBoxes();

	// Display the game frame rate as text in the upper left area of the screen
	BWAPI::Broodwar->drawTextScreen(5, 5, "%cFrame: %d", BWAPI::Text::White, BWAPI::Broodwar->getFrameCount());
	BWAPI::Broodwar->drawTextScreen(100, 5, "%cFPS: %d", BWAPI::Text::White, BWAPI::Broodwar->getFPS());
	BWAPI::Broodwar->drawTextScreen(170, 5, "%cOpponent Race: %s", BWAPI::Text::White, strategyManager.opponentRace.c_str());

	if (!drawUnitDebug) return;

	//Need to find a place to put this on the screen, might need to have a /command to get the stuff I need.
	//drawBwapiResourceInfo(5, 102);

	drawBuildingCount(InformationManager::Instance().getFriendlyBuildingCounter(), 490, 30);
	drawUpgradeCount(InformationManager::Instance().getFriendlyUpgradeCounter(), 490, 152);
	drawUnitCount(InformationManager::Instance().getFriendlyUnitCounter(), 1, 165);
	timerManager.displayTimers(490, 225);
	drawResourceRequestQueue(1, 25);
}
#pragma endregion

bool ProtoBotCommander::checkUnitIsBeingWarpedIn(BWAPI::UnitType building, const BWEM::Base* nexus)
{
	return buildManager.checkUnitIsBeingWarpedIn(building, nexus);
}

BWAPI::Unit ProtoBotCommander::getUnitToBuild(BWAPI::Position buildLocation)
{
	//Will not check for null, we expect to get a unit that is able to build. We may also be able to add a command once they return a mineral.
	return economyManager.getAvalibleWorker(buildLocation);
}

const std::set<BWAPI::Unit>& ProtoBotCommander::getKnownEnemyUnits()
{
    return InformationManager::Instance().getKnownEnemies();
}

const std::map<BWAPI::Unit, EnemyBuildingInfo>& ProtoBotCommander::getKnownEnemyBuildings()
{
    return InformationManager::Instance().getKnownEnemyBuildings();
}

//Move this command into the strategy manager to keep the flow of data consistent.
void ProtoBotCommander::requestCheese(BWAPI::UnitType building, BWAPI::Unit unit)
{
	ResourceRequest request;
	request.type = ResourceRequest::Type::Building;
	request.unit = building;
	request.scoutToPlaceBuilding = unit;
	request.isCheese = true;
	request.fromBuildOrder = false;

	resourceRequests.push_back(request);
}

bool ProtoBotCommander::checkCheeseRequest(BWAPI::Unit unit)
{
	for (ResourceRequest& request : resourceRequests)
	{
		if (request.type != ResourceRequest::Type::Building && request.isCheese) continue;

		if (request.scoutToPlaceBuilding == unit && request.state == ResourceRequest::State::Approved_BeingBuilt) return true;
	}

	return false;
}

bool ProtoBotCommander::buildOrderCompleted()
{
	return buildManager.isBuildOrderCompleted();
}

std::vector<NexusEconomy> ProtoBotCommander::getNexusEconomies()
{
	return economyManager.getNexusEconomies();
}

bool ProtoBotCommander::checkWorkerIsConstructing(BWAPI::Unit unit)
{
	return buildManager.checkWorkerIsConstructing(unit);
}

//[TODO] change this to to ask the economy manager to get a worker that can scout, getAvalibleWorker() is a method that gets a builder
//You can also change the name of this in order to make it clear what "getAvalibleWorker()" is doing.
//Also get rid of the self get Units, eco should return a unit, not a null ptr.
BWAPI::Unit ProtoBotCommander::getUnitToScout()
{
	auto isValidUnit = [](BWAPI::Unit u)
		{
			return u && u->exists() && u->getPlayer() == BWAPI::Broodwar->self();
		};

	const int frame = BWAPI::Broodwar->getFrameCount();

	if (frame >= kCombatScoutFrame && !scoutingManager.combatScoutingStarted())
	{
		scoutingManager.setCombatScoutingStarted(true);
	}

	// Before combat scouting: keep exactly one worker scout
	if (!scoutingManager.combatScoutingStarted())
	{
		if (scoutingManager.canAcceptWorkerScout())
		{
			BWAPI::Unit w = economyManager.getUnitScout();
			if (isValidUnit(w))
			{
				scoutingManager.assignScout(w);
				return w;
			}
		}

		return nullptr;
	}

	// After combat scouting starts, assign at most one scout per call.

	if (scoutingManager.canAcceptCombatScout(BWAPI::UnitTypes::Protoss_Zealot))
	{
		BWAPI::Unit u = combatManager.getAvailableUnit(
			[](BWAPI::Unit x)
			{
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Zealot;
			}
		);

		if (isValidUnit(u))
		{
			scoutingManager.assignScout(u);
			return u;
		}
	}

	if (scoutingManager.canAcceptCombatScout(BWAPI::UnitTypes::Protoss_Dragoon))
	{
		BWAPI::Unit u = combatManager.getAvailableUnit(
			[](BWAPI::Unit x)
			{
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Dragoon;
			}
		);

		if (isValidUnit(u))
		{
			scoutingManager.assignScout(u);
			return u;
		}
	}

	if (scoutingManager.canAcceptObserverScout())
	{
		BWAPI::Unit u = combatManager.getAvailableUnit(
			[](BWAPI::Unit x)
			{
				return x && x->exists() && x->getType() == BWAPI::UnitTypes::Protoss_Observer;
			}
		);

		if (isValidUnit(u))
		{
			scoutingManager.assignScout(u);
			return u;
		}
	}

	return nullptr;
}

// ------- Enemy Locations ----------
void ProtoBotCommander::onEnemyMainFound(const BWAPI::TilePosition& tp) {
	enemy_.main = tp;
	enemy_.frameLastUpdateMain = BWAPI::Broodwar->getFrameCount();
	//BWAPI::Broodwar->printf("[Commander] Enemy main set to (%d,%d)", tp.x, tp.y);
	
	// StrategyManager.onEnemyMain(tp);
}

void ProtoBotCommander::onEnemyNaturalFound(const BWAPI::TilePosition& tp) {
	enemy_.natural = tp;
	enemy_.frameLastUpdateNat = BWAPI::Broodwar->getFrameCount();
	//BWAPI::Broodwar->printf("[Commander] Enemy natural set to (%d,%d)", tp.x, tp.y);

	// StrategyManager.onEnemyNaturalFound(tp)
}

int ProtoBotCommander::getEnemyGroundThreatAt(BWAPI::Position p) const {
    return InformationManager::Instance().getEnemyGroundThreatAt(p);
}

int ProtoBotCommander::getEnemyDetectionAt(BWAPI::Position p) const {
    return InformationManager::Instance().getEnemyDetectionAt(p);
}

ThreatQueryResult ProtoBotCommander::queryThreatAt(const BWAPI::Position& pos) const
{
    return InformationManager::Instance().queryThreatAt(pos);
}

bool ProtoBotCommander::isAirThreatened(const BWAPI::Position& pos, int threshold) const
{
	const auto r = queryThreatAt(pos);
	return r.airThreat >= threshold;
}

bool ProtoBotCommander::isDetectorThreatened(const BWAPI::Position& pos) const
{
	const auto r = queryThreatAt(pos);
	return r.detectorThreat > 0;
}

bool ProtoBotCommander::shouldGasSteal()
{
	bool const enable = strategyManager.shouldGasSteal();

	return enable;
}