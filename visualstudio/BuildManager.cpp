#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"
#include "../src/starterbot/MapTools.h"

bool alreadySentRequest0 = false;
bool alreadySentRequest1 = false;
bool alreadySentRequest2 = false;
bool alreadySentRequest3 = false;
bool alreadySentRequest4 = false;
bool alreadySentRequest5 = false;
bool alreadySentRequest6 = false;
bool alreadySentRequest7 = false;
bool alreadySentRequest8 = false;


BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference), spenderManager(SpenderManager(commanderReference))
{

}

void BuildManager::onStart()
{
    //Make false at the start of a game.
    buildOrderCompleted = false;
}

void BuildManager::onFrame()
{
    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;
    const int currentMineral = BWAPI::Broodwar->self()->minerals();

    spenderManager.OnFrame();

    //Continually train dragoons with excess minerals
    /*if (currentMineral > 500) {
        buildUnitType(BWAPI::UnitTypes::Protoss_Dragoon);
        buildUpgadeType(BWAPI::UpgradeTypes::Singularity_Charge);
    }*/

    //if the supply does not follow the build order supply numbers we will skip pylons
    /*const BuildOrderInstruction instruction = selectedBuildOrder.buildOrderInstructions.at(0);

    if (instruction.supply <= currentSupply)
    {
        if (instruction.unitToTrain != BWAPI::UnitTypes::None)
        {
            buildUnitType(instruction.unitToTrain);
        }
        else if (instruction.buildingToConstruct != BWAPI::UnitTypes::None)
        {
            spenderManager.addRequest(instruction.buildingToConstruct);
        }

        selectedBuildOrder.buildOrderInstructions.erase(selectedBuildOrder.buildOrderInstructions.begin());
    }*/


    switch (currentSupply)
    {
    case 8:
    {
        if (alreadySentRequest0 == false)
        {
            spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Pylon);
            alreadySentRequest0 = true;
        }
        break;
    }
    case 10:
    {
        if (alreadySentRequest1 == false)
        {
            spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Gateway);
            alreadySentRequest1 = true;
            buildOrderCompleted = true;
        }
        break;
    }
    /*case 12:
    {
        if (alreadySentRequest2 == false)
        {
            spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Assimilator);
            alreadySentRequest2 = true;

        }
        break;
    }*/
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
    }*/
    }
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

void BuildManager::buildUnitType(BWAPI::UnitType unitToTrain)
{
    for (BWAPI::Unit unit : buildings)
    {
        if (unit->canBuild(unitToTrain) && !unit->isTraining())
        {
            unit->train(unitToTrain);
        }
    }
}

void BuildManager::buildUpgadeType(BWAPI::UpgradeType upgradeToBuild)
{
    for (BWAPI::Unit unit : buildings)
    {
        if (unit->canBuild(upgradeToBuild) && !unit->isTraining())
        {
            unit->upgrade(upgradeToBuild);
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