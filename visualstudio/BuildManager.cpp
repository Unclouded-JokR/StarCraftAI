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

    alreadySentRequest0 = false;
    alreadySentRequest1 = false;
    alreadySentRequest2 = false;
    alreadySentRequest3 = false;
    alreadySentRequest4 = false;
    alreadySentRequest5 = false;
    alreadySentRequest6 = false;
    alreadySentRequest7 = false;
    alreadySentRequest8 = false;
}
/*
void BuildManager::onFrame()
{
    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;
    const int currentMineral = BWAPI::Broodwar->self()->minerals();

    spenderManager.OnFrame();

    switch (currentSupply)
    {
        case 8:
        {
            if (alreadySentRequest0 == false)
            {
                buildBuilding(BWAPI::UnitTypes::Protoss_Pylon);
                alreadySentRequest0 = true;
            }
            break;
        }
        case 10:
        {
            if (alreadySentRequest1 == false)
            {
                buildBuilding(BWAPI::UnitTypes::Protoss_Gateway);
                alreadySentRequest1 = true;
            }
            break;
        }
        case 12:
        {
            if (alreadySentRequest2 == false)
            {
                buildBuilding(BWAPI::UnitTypes::Protoss_Assimilator);
                alreadySentRequest2 = true;
                buildOrderCompleted = true;
            }
            break;
        }
        /*case 14:
        {
            if (alreadySentRequest3 == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
                alreadySentRequest3 = true;
            }
            break;
        }
        case 15:
        {
            if (alreadySentRequest4 == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Pylon);
                alreadySentRequest4 = true;
            }
            break;
        }
        case 17:
        {
            buildUnitType(BWAPI::UnitTypes::Protoss_Dragoon);
            break;
        }
        case 25:
        {
            if (alreadySentRequest5 == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Robotics_Facility);
                alreadySentRequest5 = true;
            }
            break;
        }
        case 29:
        {
            if (alreadySentRequest6 == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Gateway);
                alreadySentRequest6 = true;
            }
            break;
        }
        case 32:
        {
            if (alreadySentRequest7 == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Pylon);
                alreadySentRequest7 = true;
            }
            break;
        }
        case 38:
        {
            if (alreadySentRequest8 == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Observatory);
                alreadySentRequest8 = true;
                buildOrderCompleted = true;
            }
            break;
        }
        default:
        {
            break;
        }
    }
}
*/
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
    if (transitionReady) //when opener is completed
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
    updateBuild();
    runBuildQueue();
}

void BuildManager::queueSupply()
{
    if (!inBookSupply) {
        int count = min(22, currentSupply / 14) - (vis(Protoss_Nexus) - 1);
        buildQueue[Protoss_Pylon] = count;
    }
}

void BuildManager::updateBuild() {
    buildQueue.clear();
    currentSupply = BWAPI::Broodwar->self()->supplyUsed();
    currentMineral = BWAPI::Broodwar->self()->minerals();
    opener();
    //generic();

}

void BuildManager::opener() {
    BWAPI::Race enemyRace = BWAPI::Broodwar->enemy()->getRace();

    if (enemyRace == BWAPI::Races::Protoss)
    {
        PvP();
    }
    if (enemyRace == BWAPI::Races::Terran)
    {
        PvP();
    }
    if (enemyRace == BWAPI::Races::Zerg)
    {
        PvP();
    }
    else
        PvP(); //for now

}
void BuildManager::generic() {
    queueSupply();
}

void BuildManager::PvT() {
    default_build();
    PvT_2Gate();
}

void BuildManager::PvP() {
    default_build();
    PvP_1Gate();
}

void BuildManager::PvZ() {
    default_build();
    PvZ_1Gate();
}


void BuildManager::default_build() {
    inOpening = true;
    inBookSupply = vis(Protoss_Pylon) < 2;
}

void BuildManager::PvT_2Gate() {
    currentBuild = "PvT_2Gate";
    transitionReady = com(Protoss_Robotics_Facility) > 0;
    inOpening = currentSupply < 60;
    buildQueue[Protoss_Nexus] = 1 + (currentSupply >= 74);
    buildQueue[Protoss_Gateway] = (currentSupply >= 20) + (currentSupply >= 60) + (currentSupply >= 62);
    buildQueue[Protoss_Robotics_Facility] = currentSupply >= 52;
    buildQueue[Protoss_Observatory] = com(Protoss_Robotics_Facility) > 0;
}

void BuildManager::PvP_1Gate() {
    // https://liquipedia.net/starcraft/1_Gate_Core_(vs._Protoss)
    currentBuild = "PvP_1GateCore";
    transitionReady = vis(Protoss_Cybernetics_Core) > 0;
    inOpening = currentSupply < 60;
    buildQueue[Protoss_Nexus] = 1;
    buildQueue[Protoss_Pylon] = (currentSupply >= 16) + (currentSupply >= 32);
    buildQueue[Protoss_Gateway] = currentSupply >= 20;
    buildQueue[Protoss_Assimilator] = currentSupply >= 24;
    buildQueue[Protoss_Cybernetics_Core] = currentSupply >= 34;
}

void BuildManager::PvZ_1Gate() {
    currentBuild = "PvZ_1Gate";
    transitionReady = vis(Protoss_Cybernetics_Core) > 0;
    inOpening = currentSupply < 60;
    buildQueue[Protoss_Nexus] = 1;
    buildQueue[Protoss_Pylon] = (currentSupply >= 16) + (currentSupply >= 32);
    buildQueue[Protoss_Gateway] = currentSupply >= 20;
    buildQueue[Protoss_Assimilator] = currentSupply >= 24;
    buildQueue[Protoss_Cybernetics_Core] = currentSupply >= 34;
}

std::map<UnitType, int>& BuildManager::getBuildQueue() { return buildQueue; }

void BuildManager::runBuildQueue() {
    for (auto& [building, count] : getBuildQueue()) {
        int queuedCount = 0;
        if (!building.isBuilding())
            continue;
        while (count > queuedCount + vis(building)) {
            queuedCount++;
            spenderManager.addRequest(building);
            //Broodwar->drawEllipseMap(building->getPosition(), 2, 2, BWAPI::Color(0, 128, 0), true);
        }
    }
}

//void BuildManager::PvP_Transition(){}