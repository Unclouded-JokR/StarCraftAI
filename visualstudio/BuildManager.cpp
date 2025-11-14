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
    if(transitionReady)
        return true;
    return false;
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
    //runBuildQueue();
    buildAdditionalSupply();
    pumpUnit();

    ////Might need to add filter on units, economy buildings, and pylons having the "Warpping Building" text.
    //for (BWAPI::Unit building : buildingWarps)
    //{
    //    BWAPI::Broodwar->drawTextMap(building->getPosition(), "Warpping Building");
    //}

    for (BWAPI::Unit building : buildings)
    {
        BWAPI::Broodwar->drawTextMap(building->getPosition(), "Assigned Building");
    }
}

void BuildManager::updateBuild() {
    //buildQueue.clear();


    //opener();

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

void BuildManager::opener() {
}
void BuildManager::generic() {
}

void BuildManager::PvT() {
    default_build();
    PvT_2Gateway_Observer();
}

void BuildManager::PvP() {
    
    default_build();
    PvP_10_12_Gateway();
}

void BuildManager::PvZ() {
    default_build();
    PvZ_10_12_Gateway();
}


void BuildManager::default_build() {
    
}
void BuildManager::buildAdditionalSupply()
{
    const int unusedSupply = GetTotalSupply(true) - Broodwar->self()->supplyUsed();
    if (unusedSupply >= 3) { return; }
    const UnitType supplyProviderType = Broodwar->self()->getRace().getSupplyProvider();
    const bool startedBuilding = BuildBuilding(supplyProviderType);
}
void BuildManager::pumpUnit(){
    const int currentMineral = BWAPI::Broodwar->self()->minerals();
    const int currentGas = BWAPI::Broodwar->self()->gas();
    for (auto& unit : buildings)
    {
	    if (unit->getType() == Protoss_Gateway && !unit->isTraining() && currentMineral > 500) {
            unit -> train(Protoss_Zealot);
	    }
        if (unit->getType() == Protoss_Gateway && !unit->isTraining() && currentGas > 300 && vis(Protoss_Dragoon)<=vis(Protoss_Zealot)/2) {
            unit -> train(Protoss_Dragoon);
	    }

    }
}



void BuildManager::PvT_2Gateway_Observer() {
    currentBuild = "PvT_2Gateway_Observer";
    transitionReady = vis(Protoss_Observatory) > 0;

    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed();
    const int currentMineral = BWAPI::Broodwar->self()->minerals();
    switch (currentSupply)
    {
        case 16:
        {
            if (alreadySentRequest0 == false && vis(Protoss_Pylon) == 0)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest0 = true;
                cout << "0" << endl;
            }
            break;
        }
        case 20:
        {
            if (alreadySentRequest1 == false && vis(Protoss_Gateway) == 0)
            {
                buildBuilding(Protoss_Gateway);
                alreadySentRequest1 = true;
            }
            break;
        }
        case 24:
        {
            if (alreadySentRequest2 == false && vis(Protoss_Assimilator) == 0)
            {
                buildBuilding(Protoss_Assimilator);
                alreadySentRequest2 = true;
            }
            break;
        }
        case 28:
        {
            if (alreadySentRequest3 == false && vis(Protoss_Cybernetics_Core) == 0)
            {
                buildBuilding(Protoss_Cybernetics_Core);
                alreadySentRequest3 = true;
            }
            break;
        }
        case 30:
        {
            if (alreadySentRequest4 == false && vis(Protoss_Pylon) == 1)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest4 = true;
            }
            break;
        }
        
        case 34:
        {
            if (vis(Protoss_Dragoon) == 0)
            {
                
                for (auto& unit : buildings)
                {
	                if (unit->getType() == Protoss_Gateway) {
                    unit -> train(Protoss_Dragoon);
                    // calling trainUnit sometimes makes the game hiccup and break the worker queue, using train() for now
	                }
                }
                
            }
            break;
        }

        case 36:
        {
            if (vis(Protoss_Dragoon) == 0)
            {
                
                for (auto& unit : buildings)
                {
	                if (unit->getType() == Protoss_Gateway) {
                    unit -> train(Protoss_Dragoon);
                    // calling trainUnit sometimes makes the game hiccup and break the worker queue, using train() for now
	                }
                }
                
            }
            break;
        }
        
        case 44:
        {
            if (alreadySentRequest6 == false && vis(Protoss_Pylon) == 2)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest6 = true;
            }
            break;
        }
        case 50:
        {
            if (alreadySentRequest7 == false && vis(Protoss_Robotics_Facility) == 0)
            {
                buildBuilding(Protoss_Robotics_Facility);
                alreadySentRequest7 = true;
            }
            break;
        }
        case 58:
        {
            if (alreadySentRequest8 == false && vis(Protoss_Gateway) == 1)
            {
                buildBuilding(Protoss_Gateway);
                alreadySentRequest8 = true;
            }
            break;
        }
        case 62:
        {
            if (alreadySentRequest9 == false && vis(Protoss_Pylon) == 3)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest9 = true;
            }
            break;
        }
        case 76:
        {
            if (alreadySentRequest10 == false && vis(Protoss_Observatory) == 0)
            {
                buildBuilding(Protoss_Observatory);
                alreadySentRequest10 = true;
                transitionReady = true;
            }
            break;
        }
        default:
            break;
    }
}

void BuildManager::PvP_10_12_Gateway() {
    currentBuild = "PvP_10/12_Gateway";
    transitionReady = vis(Protoss_Pylon) > 2;

    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed();
    const int currentMineral = BWAPI::Broodwar->self()->minerals();
    switch (currentSupply)
    {
        case 16:
        {
            if (alreadySentRequest0 == false && vis(Protoss_Pylon) == 0)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest0 = true;
            }
            break;
        }
        case 20:
        {
            if (alreadySentRequest1 == false && vis(Protoss_Gateway) == 0)
            {
                buildBuilding(Protoss_Gateway);
                alreadySentRequest1 = true;

            }
            break;
        }
        case 24:
        {
            if (alreadySentRequest2 == false && vis(Protoss_Gateway) == 1)
            {
                buildBuilding(Protoss_Gateway);
                alreadySentRequest2 = true;

            }
            break;
        }
        case 26:
        {
            if (vis(Protoss_Zealot) == 0)
            {
                for (auto& unit : buildings)
                {
	                if (unit->getType() == Protoss_Gateway) {
                    unit -> train(Protoss_Zealot);
	                }
                }
            }
            break;
        }
        case 32:
        {
            if (alreadySentRequest4 == false && vis(Protoss_Pylon) == 1)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest4 = true;
            }
            break;
        }
        
        default:
            break;
    }
}

void BuildManager::PvZ_10_12_Gateway() {
    currentBuild = "PvZ_10/12_Gateway";
    transitionReady = vis(Protoss_Pylon) > 2;

    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed();
    const int currentMineral = BWAPI::Broodwar->self()->minerals();
    switch (currentSupply)
    {
        case 16:
        {
            if (alreadySentRequest0 == false && vis(Protoss_Pylon) == 0)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest0 = true;
            }
            break;
        }
        case 20:
        {
            if (alreadySentRequest1 == false && vis(Protoss_Gateway) == 0)
            {
                buildBuilding(Protoss_Gateway);
                alreadySentRequest1 = true;

            }
            break;
        }
        case 24:
        {
            if (alreadySentRequest2 == false && vis(Protoss_Gateway) == 1)
            {
                buildBuilding(Protoss_Gateway);
                alreadySentRequest2 = true;

            }
            break;
        }
        case 26:
        {
            if (vis(Protoss_Zealot) == 0)
            {
                for (auto& unit : buildings)
                {
	                if (unit->getType() == Protoss_Gateway) {
                    unit -> train(Protoss_Zealot);
	                }
                }
            }
            break;
        }
        case 30:
        {
            if (alreadySentRequest4 == false && vis(Protoss_Pylon) == 1)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest4 = true;
            }
            break;
        }
        
        case 34:
        {
            if (vis(Protoss_Zealot) == 1)
            {
                
                for (auto& unit : buildings)
                {
	                if (unit->getType() == Protoss_Gateway) {
                    unit -> train(Protoss_Zealot);
	                }
                }
                
            }
            break;
        }
        
        case 38:
        {
            if (vis(Protoss_Zealot) == 2)
            {
                
                for (auto& unit : buildings)
                {
	                if (unit->getType() == Protoss_Gateway) {
                    unit -> train(Protoss_Zealot);
	                }
                }
                
            }
            break;
        }
        case 42:
        {
            if (alreadySentRequest7 == false && vis(Protoss_Pylon) == 3)
            {
                buildBuilding(Protoss_Pylon);
                alreadySentRequest7 = true;
            }
            break;
        }
        
        default:
            break;
    }
}

std::map<UnitType, int>& BuildManager::getBuildQueue() { return buildQueue; }

void BuildManager::runBuildQueue() {
    for (auto& [building, count] : getBuildQueue()) {
        int queuedCount = 0;
        while (count > queuedCount + vis(building)) {
            queuedCount++;
            buildBuilding(building);
            //Broodwar->drawEllipseMap(building->getPosition(), 2, 2, BWAPI::Color(0, 128, 0), true);
        }
    }
}

//void BuildManager::PvP_Transition(){}