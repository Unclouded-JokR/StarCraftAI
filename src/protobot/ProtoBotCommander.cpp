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
	/*
	* Do not touch this code, these are lines of code from StarterBot that we need to have our bot functioning.
	*/
	timerManager.startTimer(TimerManager::All);

	// Update our MapTools information
	timerManager.startTimer(TimerManager::MapTools);
	m_mapTools.onFrame();
	timerManager.stopTimer(TimerManager::MapTools);

	removeApprovedRequests();

	/*
	* Protobot Modules
	*/

	timerManager.startTimer(TimerManager::Information);
    InformationManager::Instance().onFrame();
	timerManager.stopTimer(TimerManager::Information);

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

	//Leaving these in a specific order due to cases like building manager possibly needing units.
	timerManager.startTimer(TimerManager::Economy);
	economyManager.onFrame();
	timerManager.stopTimer(TimerManager::Economy);

	//Uncomment this once onFrame does not steal a worker.
	timerManager.startTimer(TimerManager::Scouting);
	scoutingManager.onFrame();
	timerManager.stopTimer(TimerManager::Scouting);

	timerManager.startTimer(TimerManager::Combat);
	combatManager.onFrame();
	timerManager.stopTimer(TimerManager::Combat);

	timerManager.stopTimer(TimerManager::All);

	// Draw unit health bars, which brood war unfortunately does not do
	Tools::DrawUnitHealthBars();

	// Draw some relevent information to the screen to help us debug the bot
	drawDebugInformation();

	//BWEB::Walls::draw();
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
	buildManager.onUnitMorph(unit);

	if (unit->getPlayer() == BWAPI::Broodwar->self())
	{
		//Need to check this for tech and upgrades;
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
}

void ProtoBotCommander::onSendText(std::string text)
{
	if (text == "/map")
	{
		m_mapTools.toggleDraw();
	}

	combatManager.handleTextCommand(text);
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
		if (it->state == ResourceRequest::State::Accepted_Completed || it->attempts == MAX_ATTEMPTS)
		{
			//if (it->state == ResourceRequest::State::Accepted_Completed) std::cout << "Completed Request\n";
			//if (it->attempts == MAX_ATTEMPTS) std::cout << "Killing request to build " << it->unit << "\n";

			it = resourceRequests.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void ProtoBotCommander::requestBuilding(BWAPI::UnitType building, bool fromBuildOrder, bool isWall, bool isRampPlacement)
{
	ResourceRequest request;
	request.type = ResourceRequest::Type::Building;
	request.unit = building;
	request.fromBuildOrder = fromBuildOrder;
	request.isWall = isWall;
	request.isRampPlacement = isRampPlacement;

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
}

void ProtoBotCommander::requestUnit(BWAPI::UnitType unit, BWAPI::Unit buildingToTrain, bool fromBuildOrder)
{
	ResourceRequest request;
	request.type = ResourceRequest::Type::Unit;
	request.unit = unit;
	request.requestedBuilding = buildingToTrain;
	request.fromBuildOrder = fromBuildOrder;

	resourceRequests.push_back(request);
}

void ProtoBotCommander::requestUpgrade(BWAPI::Unit unit, BWAPI::UpgradeType upgrade, bool fromBuildOrder)
{
	ResourceRequest request;
	request.type = ResourceRequest::Type::Upgrade;
	request.upgrade = upgrade;
	request.requestedBuilding = unit;
	request.fromBuildOrder = fromBuildOrder;

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

bool ProtoBotCommander::requestedBuilding(BWAPI::UnitType building)
{
	for (const ResourceRequest& request : resourceRequests)
	{
		if (building == request.unit && !request.isCheese) return true;
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

bool ProtoBotCommander::checkUnitIsPlanned(BWAPI::UnitType building)
{
	for (const ResourceRequest& request : resourceRequests)
	{
		if (building == request.unit && request.state == ResourceRequest::State::Approved_InProgress && !request.isCheese) return true;
	}
	return false;
}

void ProtoBotCommander::drawDebugInformation()
{
	// Display the game frame rate as text in the upper left area of the screen
	BWAPI::Broodwar->drawTextScreen(0, 10, "FPS: %d", BWAPI::Broodwar->getFPS());
	BWAPI::Broodwar->drawTextScreen(0, 20, "Average FPS: %f", BWAPI::Broodwar->getAverageFPS());
	BWAPI::Broodwar->drawTextScreen(0, 0, "Frame: %d", BWAPI::Broodwar->getFrameCount());

	BWAPI::Broodwar->drawTextScreen(0, 40, "Resource Information");
	BWAPI::Broodwar->drawTextScreen(0, 41, "_________________________");
	BWAPI::Broodwar->drawTextScreen(0, 50, "Total Minerals Gathered: %d", BWAPI::Broodwar->self()->gatheredMinerals());
	BWAPI::Broodwar->drawTextScreen(0, 60, "Total Gas Gathered : %d", BWAPI::Broodwar->self()->gatheredGas());

	BWAPI::Broodwar->drawTextScreen(0, 75, "Total Minerals Spent: %d", BWAPI::Broodwar->self()->spentMinerals());
	BWAPI::Broodwar->drawTextScreen(0, 85, "Total Gas Spent : %d", BWAPI::Broodwar->self()->spentGas());

	/*BWAPI::Broodwar->drawTextScreen(0, 40, "Elapsed Time (Real time): %02d:", BWAPI::Broodwar->elapsedTime() / 60);
	BWAPI::Broodwar->drawTextScreen(142, 40, "%02d", BWAPI::Broodwar->elapsedTime() % 60);*/

	Tools::DrawUnitCommands();
	Tools::DrawUnitBoundingBoxes();

	timerManager.displayTimers(490, 225);
}
#pragma endregion

bool ProtoBotCommander::checkUnitIsBeingWarpedIn(BWAPI::UnitType building)
{
	return buildManager.checkUnitIsBeingWarpedIn(building);
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