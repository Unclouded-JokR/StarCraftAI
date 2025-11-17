#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include <fstream>
#include "../src/starterbot/Tools.h"
#include "../src/starterbot/MapTools.h"

using namespace BWAPI;
using namespace Tools;
using namespace All;
using namespace UnitTypes;
using namespace std;

BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference), spenderManager(SpenderManager(commanderReference))
{

}

void BuildManager::onStart()
{
    //Make false at the start of a game.
    buildOrderCompleted = false;

    BWEB::Map::onStart();
	BWEB::Blocks::findBlocks();
	BWEB::Stations::findStations();

}

void BuildManager::onUnitDestroy(BWAPI::Unit unit)
{
    if (unit->getPlayer() != BWAPI::Broodwar->self())
        return;

    BWAPI::UnitType unitType = unit->getType();

    if (!unitType.isBuilding()) return;

    //Check if a non-completed building has been killed
    for (BWAPI::Unit warp : buildingWarps)
    {
        if (unit == warp)
        {
            buildingWarps.erase(warp);
            return;
        }
    }

    //If the unit is something dealing with economy exit.
    if (unitType == BWAPI::UnitTypes::Protoss_Pylon || unitType == BWAPI::UnitTypes::Protoss_Nexus || unitType == BWAPI::UnitTypes::Protoss_Assimilator) return;

    for (BWAPI::Unit building : buildings)
    {
        if (building->getID() == unit->getID())
        {
            buildings.erase(building);
            break;
        }
    }

}

void BuildManager::assignBuilding(BWAPI::Unit unit)
{
    std::cout << "Assigning " << unit->getType() << " to BuildManager\n";
    buildings.insert(unit);
    std::cout << "Buildings size: " << buildings.size() << "\n";
}

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
}

bool BuildManager::requestedBuilding(BWAPI::UnitType building)
{
    return spenderManager.requestedBuilding(building);
}

void BuildManager::buildBuilding(BWAPI::UnitType building)
{
    spenderManager.addRequest(building);
}

void BuildManager::trainUnit(BWAPI::UnitType unitToTrain, BWAPI::Unit unit)
{
    spenderManager.addRequest(unitToTrain, unit);
}

void BuildManager::onCreate(BWAPI::Unit unit)
{
    buildingWarps.insert(unit);
    spenderManager.onUnitCreate(unit);
}

bool BuildManager::alreadySentRequest(int unitID)
{
    return spenderManager.buildingAlreadyMadeRequest(unitID);
}

bool BuildManager::checkUnitIsBeingWarpedIn(BWAPI::UnitType building)
{
    for (BWAPI::Unit warp : buildingWarps)
    {
        if (building == warp->getType())
        {
            return true;
        }
    }

    return false;
}

bool BuildManager::checkUnitIsPlanned(BWAPI::UnitType building)
{
    return spenderManager.checkUnitIsPlanned(building);
}

void BuildManager::buildingDoneWarping(BWAPI::Unit unit)
{
    for (BWAPI::Unit warp : buildingWarps)
    {
        if (unit == warp)
        {
            buildingWarps.erase(unit);
            break;
        }
    }
}

void BuildManager::onFrame() {
    spenderManager.OnFrame();
    buildQueue.clear();

    if (!buildOrderCompleted)
    {
        updateBuild();
        runBuildQueue();
        runUnitQueue();
    }

    pumpUnit();
    expansionBuilding();
    ////Might need to add filter on units, economy buildings, and pylons having the "Warpping Building" text.
    //for (BWAPI::Unit building : buildingWarps)
    //{
    //    BWAPI::Broodwar->drawTextMap(building->getPosition(), "Warpping Building");
    //}

    for (BWAPI::Unit building : buildings)
    {
        BWAPI::Broodwar->drawTextMap(building->getPosition(), std::to_string(building->getID()).c_str());
    }
}

void BuildManager::updateBuild() {
    BWAPI::Race enemyRace = BWAPI::Broodwar->enemy()->getRace();

    if (enemyRace == BWAPI::Races::Protoss)
    {
        PvP_10_12_Gateway();
    }
    if (enemyRace == BWAPI::Races::Terran)
    {
        PvT_2Gateway_Observer();
    }
    if (enemyRace == BWAPI::Races::Zerg)
    {
        PvZ_10_12_Gateway();
    }
    else
        PvT_2Gateway_Observer();

}

void BuildManager::PvT() {
    PvT_2Gateway_Observer();
}

void BuildManager::PvP() {
    PvP_10_12_Gateway();
}

void BuildManager::PvZ() {
    PvZ_10_12_Gateway();
}

void BuildManager::pumpUnit() {
    const int currentMineral = BWAPI::Broodwar->self()->minerals();
    const int currentGas = BWAPI::Broodwar->self()->gas();
    int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;

    for (auto& unit : buildings)
    {
        if (unit->getType() == Protoss_Gateway && !unit->isTraining() && !alreadySentRequest(unit->getID()))
        {
            if (unit->canTrain(Protoss_Dragoon))
            {
                trainUnit(Protoss_Dragoon, unit);
                cout << "Training Dragoon\n";
            }
        }
        else if (unit->getType() == Protoss_Robotics_Facility && !unit->isTraining() && !alreadySentRequest(unit->getID()) && unit->canTrain(Protoss_Observer))
        {
            //20 percent chance to create a 
            const int temp = rand() % 5;

            if (temp == 0)
            {
                trainUnit(Protoss_Observer, unit);
                cout << "Training Observer\n";
            }
        }
    }
}

void BuildManager::PvT_2Gateway_Observer() {
    currentBuild = "PvT_2Gateway_Observer";

    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;

    buildQueue[Protoss_Pylon] = (currentSupply >= 8) + (currentSupply >= 15) + (currentSupply >= 22) + ((currentSupply >= 31));
    buildQueue[Protoss_Gateway] = (currentSupply >= 10) + (currentSupply >= 29);
    buildQueue[Protoss_Assimilator] = (currentSupply >= 12);
    buildQueue[Protoss_Cybernetics_Core] = (currentSupply >= 14);
    buildQueue[Protoss_Robotics_Facility] = (currentSupply >= 25);
    buildQueue[Protoss_Observatory] = (currentSupply >= 38);
    //buildQueue[Protoss_Nexus] = (currentSupply >= 20);

    unitQueue[Protoss_Dragoon] = com(Protoss_Cybernetics_Core) > 0;
    unitQueue[Protoss_Observer] = com(Protoss_Observatory) > 0;

    ////Start pumping Zealots once 1st Dragoon built, can be changed
    //zealotUnitPump = vis(Protoss_Dragoon) > 0;
    if (currentSupply >= 38)
        buildOrderCompleted = true;
}

void BuildManager::PvP_10_12_Gateway() {
    currentBuild = "PvP_10/12_Gateway";

    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;

    buildQueue[Protoss_Pylon] = (currentSupply >= 8) + (currentSupply >= 16);
    buildQueue[Protoss_Gateway] = (currentSupply >= 10) + (currentSupply >= 12);
    zealotUnitPump = com(Protoss_Gateway) > 0;
    if (vis(Protoss_Pylon) > 1)
        buildOrderCompleted = true;
}

void BuildManager::PvZ_10_12_Gateway() {
    currentBuild = "PvZ_10/12_Gateway";
    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;

    buildQueue[Protoss_Pylon] = (currentSupply >= 8) + (currentSupply >= 15) + (currentSupply >= 21);
    buildQueue[Protoss_Gateway] = (currentSupply >= 10) + (currentSupply >= 12);
    zealotUnitPump = com(Protoss_Gateway) > 0;
    if (vis(Protoss_Pylon) > 2)
        buildOrderCompleted = true;
}

std::map<UnitType, int>& BuildManager::getBuildQueue() { return buildQueue; }
std::map<UnitType, int>& BuildManager::getUnitQueue() { return unitQueue; }

void BuildManager::runBuildQueue() {
    for (auto& [building, count] : getBuildQueue()) {
        int queuedCount = 0;
        while (count > (queuedCount + vis(building)) && !requestedBuilding(building) && !checkUnitIsPlanned(building)) {
            queuedCount++;
            //if (building.isResourceDepot())
            //    ExpansionBuild(building);
            buildBuilding(building);
        }
    }
}

void BuildManager::runUnitQueue() {
    for (auto& build : buildings)
    {
        for (auto& [unit, count] : getUnitQueue()) {
            int queuedCount = 0;
            while (count > (queuedCount + vis(unit)) && !requestedBuilding(unit) && !checkUnitIsPlanned(unit)) {
                queuedCount++;

                if (build->getType() == Protoss_Gateway && !alreadySentRequest(build->getID()) && !build->isTraining())
                    trainUnit(unit, build);
                if (build->getType() == Protoss_Robotics_Facility && !alreadySentRequest(build->getID()) && !build->isTraining())
                    trainUnit(unit, build);
            }
        }
    }
}

// Test function to build a nexus, will be replaced with an addRequest from spenderManager
void BuildManager::expansionBuilding(){
    if(!buildOrderCompleted || vis(Protoss_Nexus > 1) || requestsent)
        return;
    const bool startedbuilding = Tools::ExpansionBuild(Tools::GetUnitOfType(Protoss_Probe));
    requestsent = true;
}

vector<BuildManager::BuildList> BuildManager::getBuildOrders(BWAPI::Race race){
    vector<BuildList> builds;
    if (race == BWAPI::Races::Terran)
        builds.push_back(&BuildManager::PvT_2Gateway_Observer);
    if (race == BWAPI::Races::Protoss)
        builds.push_back(&BuildManager::PvP_10_12_Gateway);    
    if (race == BWAPI::Races::Zerg)
        builds.push_back(&BuildManager::PvZ_10_12_Gateway);
    else
        builds.push_back(&BuildManager::PvT_2Gateway_Observer);
    return builds;
}