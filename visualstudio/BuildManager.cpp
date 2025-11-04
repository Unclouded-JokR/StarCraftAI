#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"
#include "../src/starterbot/MapTools.h"

bool alreadySentRequest = false;
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
    buildOrderCompleted = true;

    BuildOrderInstruction insturction1;
    insturction1.supply = 8;
    insturction1.buildingToConstruct = BWAPI::UnitTypes::Protoss_Pylon;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction1);

    BuildOrderInstruction insturction2;
    insturction2.supply = 10;
    insturction2.buildingToConstruct = BWAPI::UnitTypes::Protoss_Gateway;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction2);

    BuildOrderInstruction insturction3;
    insturction3.supply = 12;
    insturction3.buildingToConstruct = BWAPI::UnitTypes::Protoss_Assimilator;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction3);

    BuildOrderInstruction insturction4;
    insturction4.supply = 14;
    insturction4.buildingToConstruct = BWAPI::UnitTypes::Protoss_Cybernetics_Core;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction4);

    BuildOrderInstruction insturction5;
    insturction5.supply = 15;
    insturction5.buildingToConstruct = BWAPI::UnitTypes::Protoss_Pylon;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction5);

    BuildOrderInstruction insturction10;
    insturction10.supply = 17;
    insturction10.unitToTrain = BWAPI::UnitTypes::Protoss_Dragoon;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction10);

    BuildOrderInstruction insturction6;
    insturction6.supply = 25;
    insturction6.unitToTrain = BWAPI::UnitTypes::Protoss_Robotics_Facility;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction6);

    BuildOrderInstruction insturction7;
    insturction7.supply = 29;
    insturction7.unitToTrain = BWAPI::UnitTypes::Protoss_Gateway;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction7);

    BuildOrderInstruction insturction8;
    insturction8.supply = 31;
    insturction8.unitToTrain = BWAPI::UnitTypes::Protoss_Pylon;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction8);

    BuildOrderInstruction insturction9;
    insturction9.supply = 38;
    insturction9.unitToTrain = BWAPI::UnitTypes::Protoss_Observatory;

    selectedBuildOrder.buildOrderInstructions.push_back(insturction9);

    std::cout << "Build Order Size: " << selectedBuildOrder.buildOrderInstructions.size() << "\n";
}

void BuildManager::onFrame()
{
    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;
    const int currentMineral = BWAPI::Broodwar->self()->minerals();

    spenderManager.OnFrame();

    //Continually train dragoons with excess minerals
    if (currentMineral > 500) {
        buildUnitType(BWAPI::UnitTypes::Protoss_Dragoon);
        buildUpgadeType(BWAPI::UpgradeTypes::Singularity_Charge);
    }

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


    /*switch (currentSupply)
    {
        case 8:
        {
            if (alreadySentRequest == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Pylon);
                alreadySentRequest = true;
            }
            break;
        }
        case 10:
        {
            if (alreadySentRequest1 == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Gateway);
                alreadySentRequest1 = true;
            }
            break;
        }
        case 12:
        {
            if (alreadySentRequest2 == false)
            {
                spenderManager.addRequest(BWAPI::UnitTypes::Protoss_Assimilator);
                alreadySentRequest2 = true;
            }
            break;
        }
        case 14:
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
    }*/
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
    std::cout << "Assigning " << unit->getType() << "to BuildManager\n";
    buildings.insert(unit);
    std::cout << "Buildings size: " << buildings.size() << "\n";
}

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
}

void BuildManager::buildBuilding(BWAPI::UnitType building)
{
    spenderManager.addRequest(building);
}
