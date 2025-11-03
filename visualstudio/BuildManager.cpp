#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"
#include "../src/starterbot/MapTools.h"

bool alreadySentRequest = false;
// constructor for BuildManager
BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
    
}

void BuildManager::onStart()
{
    //Make false at the start of a game.
    buildOrderCompleted = false;
}

void BuildManager::onFrame()
{
    int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;
    int currentMineral = BWAPI::Broodwar->self()->minerals();

    //Continually train dragoons with excess minerals
    if (currentMineral > 500) {
        buildUnitType(BWAPI::UnitTypes::Protoss_Dragoon);
        buildUpgadeType(BWAPI::UpgradeTypes::Singularity_Charge);
    }

    //if the supply does not follow the build order supply numbers we will skip pylons
    switch (currentSupply)
    {
        case 8:
        {
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Pylon);
            break;
        }
        case 10:
        {
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Gateway);
            break;
        }
        case 12:
        {
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Assimilator);
            break;
        }
        case 14:
        {
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Cybernetics_Core);
            break;
        }
        case 15:
        {
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Pylon);
            break;
        }
        case 17:
        {
            buildUnitType(BWAPI::UnitTypes::Protoss_Dragoon);
        }
         case 25:
         {
             Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Robotics_Facility);
             break;
         }
        case 29:
        {
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Gateway);
            break;
        }
        case 31:
        {
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Pylon);
            break;
        }
        case 38:
        {
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Observatory);
            buildOrderCompleted = true;
            break;
        }
        default:
        {
            break;
        }
    }
}

void BuildManager::buildUnitType(BWAPI::UnitType unitToTrain)
{
    for (BWAPI::Unit unit : buildings)
    {
        if (unit->canBuild(unit) && !unit->isTraining())
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
    buildings.insert(unit);
}

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
}

void BuildManager::buildBuilding(BWAPI::Unit unit, BWAPI::UnitType building)
{
    Tools::BuildBuilding(unit, building);
}
