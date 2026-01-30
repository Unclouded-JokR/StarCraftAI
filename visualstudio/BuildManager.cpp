#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "SpenderManager.h"
#include "BuildingPlacer.h"
#include "Builder.h"

BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference), spenderManager(new SpenderManager(this)), buildingPlacer(new BuildingPlacer(this))
{

}

//BuildManager::~BuildManager()
//{
//    delete commanderReference;
//    delete spenderManager;
//    delete buildingPlacer;
//
//    builders.clear();
//    buildings.clear();
//    incompleteBuildings.clear();
//}


#pragma region BWAPI EVENTS
void BuildManager::onStart()
{
    //Make false at the start of a game.
    std::cout << "Builder Manager Initialized" << "\n";
    buildOrderCompleted = true;
    spenderManager->onStart();
    builders.clear();
}

void BuildManager::onFrame() {
    spenderManager->OnFrame();

    //build order check here

    //For now get old functionality to work.
    /*for (std::vector<Builder>::iterator builder = builders.begin(); builder != builders.end();)
    {
        builder->onFrame();
    }*/

    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end();)
    {
        if (it->getUnitReference()->isConstructing())
        {
            it++;
            continue;
        }


        //Need to check if we are able to build. Units on tiles can cause buildings NOT to warp in
        if (it->getUnitReference()->isIdle() || it->getUnitReference()->getPosition() == it->positionToBuild)
        {
            //std::cout << "In position to build " << it->building << "\n";
            if (!it->getUnitReference()->canBuild(it->buildingToConstruct))
            {
                it++;
                continue;
            }

            const bool buildSuccess = it->getUnitReference()->build(it->buildingToConstruct, BWAPI::TilePosition(it->positionToBuild));

            //Get new position to build if we cannot build at this place.
            if (!buildSuccess)
            {
                //std::cout << "BUILD UNSUCCESSFUL, trying another spot\n";
                it->positionToBuild = buildingPlacer->getPositionToBuild(it->buildingToConstruct);
                it++;
                continue;
            }
            //std::cout << (it->probe->isConstructing()) << "\n";
            //std::cout << "Build command returned true, constructing...\n";

            //Need to utilize BWEB's reserving tile system to improve building placement even further.
            //BWEB::Map::addUsed(BWAPI::TilePosition(it->positionToBuild), it->building);

            if (it->buildingToConstruct == BWAPI::UnitTypes::Protoss_Assimilator)
            {
                //Small chance for assimlator to not be build will try to fix this later.
                it = builders.erase(it);
            }
            else
            {
                it++;
            }
        }
        else
        {
            //Testing this cause the move command has a small chance to fail
            if (it->getUnitReference()->getOrder() != BWAPI::Orders::Move)
            {
                //std::cout << "Move command failed, trying again\n";
                it->getUnitReference()->move(it->positionToBuild);
            }

            it++;
        }
    }


    //Debug
    for (BWAPI::Unit building : buildings)
    {
        BWAPI::Broodwar->drawTextMap(building->getPosition(), std::to_string(building->getID()).c_str());
    }

    pumpUnit();

    ////Might need to add filter on units, economy buildings, and pylons having the "Warpping Building" text.
    //for (BWAPI::Unit building : buildingWarps)
    //{
    //    BWAPI::Broodwar->drawTextMap(building->getPosition(), "Warpping Building");
    //}
}

void BuildManager::onUnitCreate(BWAPI::Unit unit)
{
    if (unit == nullptr) return;

    //Remove worker once a building is being warped in.
    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
    {
        if (unit->getType() == it->buildingToConstruct)
        {
            it = builders.erase(it);
            break;
        }
    }

    spenderManager->onUnitCreate(unit);

    if (unit->getType().isBuilding() && !unit->isCompleted()) incompleteBuildings.insert(unit);
}

void BuildManager::onUnitDestroy(BWAPI::Unit unit)
{
    spenderManager->onUnitDestroy(unit);

    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end();)
    {
        if (it->getUnitReference()->getID() == unit->getID())
        {
            const BWAPI::Unit unitAvalible = getUnitToBuild(it->positionToBuild);
            it->setUnitReference(unitAvalible);
            break;
        }
        else
        {
            it++;
        }
    }

    if (unit->getPlayer() != BWAPI::Broodwar->self())
        return;

    BWAPI::UnitType unitType = unit->getType();

    if (!unitType.isBuilding()) return;

    //Check if a non-completed building has been killed
    for (BWAPI::Unit warp : incompleteBuildings)
    {
        if (unit == warp)
        {
            incompleteBuildings.erase(warp);
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

void BuildManager::onUnitMorph(BWAPI::Unit unit)
{
    spenderManager->onUnitMorph(unit);
}

void BuildManager::onUnitComplete(BWAPI::Unit unit)
{
    buildings.insert(unit);

    //Might cause error if the unit was not added possibly.
    std::cout << "Removing " << unit->getType() << "\n";
    incompleteBuildings.erase(unit);

    spenderManager->onUnitComplete(unit);
}

void BuildManager::onUnitDiscover(BWAPI::Unit unit)
{
    spenderManager->onUnitDiscover(unit);
}
#pragma endregion

#pragma region Spender Manager Methods
void BuildManager::buildBuilding(BWAPI::UnitType building)
{
    spenderManager->addRequest(building);
}

void BuildManager::trainUnit(BWAPI::UnitType unitToTrain, BWAPI::Unit unit)
{
    spenderManager->addRequest(unitToTrain, unit);
}

void BuildManager::buildUpgadeType(BWAPI::Unit unit, BWAPI::UpgradeType upgrade)
{
    spenderManager->addRequest(unit, upgrade);
}

bool BuildManager::alreadySentRequest(int unitID)
{
    return spenderManager->buildingAlreadyMadeRequest(unitID);
}

bool BuildManager::requestedBuilding(BWAPI::UnitType building)
{
    return spenderManager->requestedBuilding(building);
}

bool BuildManager::upgradeAlreadyRequested(BWAPI::Unit building)
{
    return spenderManager->upgradeAlreadyRequested(building);
}

bool BuildManager::checkUnitIsPlanned(BWAPI::UnitType building)
{
    return spenderManager->checkUnitIsPlanned(building);
}

bool BuildManager::checkWorkerIsConstructing(BWAPI::Unit unit)
{
    for (Builder& builder : builders)
    {
        if (builder.getUnitReference()->getID() == unit->getID()) return true;
    }

    return false;
}

int BuildManager::checkAvailableSupply()
{
    return spenderManager->plannedSupply();
}
#pragma endregion

void BuildManager::createBuilder(BWAPI::Unit unit, BWAPI::UnitType building, BWAPI::Position positionToBuild)
{
    Builder temp = Builder(unit, building, positionToBuild);
    builders.push_back(temp);
}

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
}

bool BuildManager::checkUnitIsBeingWarpedIn(BWAPI::UnitType building)
{
    for (BWAPI::Unit warp : incompleteBuildings)
    {
        if (building == warp->getType())
        {
            return true;
        }
    }

    return false;
}

void BuildManager::buildingDoneWarping(BWAPI::Unit unit)
{
    for (BWAPI::Unit warp : incompleteBuildings)
    {
        if (unit == warp)
        {
            incompleteBuildings.erase(unit);
            break;
        }
    }

}

void BuildManager::pumpUnit()
{
    /*BWAPI::Unit firstTemplar = nullptr;

    for (BWAPI::Unit unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar && firstTemplar == nullptr && unit->getOrder() != BWAPI::Orders::ArchonWarp)
        {
            firstTemplar = unit;
        }
        else if (unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar && firstTemplar != nullptr && unit->getOrder() != BWAPI::Orders::ArchonWarp)
        {
            firstTemplar->useTech(BWAPI::TechTypes::Archon_Warp, unit);
            std::cout << firstTemplar->getOrder() << "\n";

            firstTemplar = nullptr;
        }
    }*/

    for (auto& unit : buildings)
    {
        BWAPI::UnitType type = unit->getType();
        if (type == BWAPI::UnitTypes::Protoss_Gateway && !unit->isTraining() && !alreadySentRequest(unit->getID()))
        {
            if (unit->canTrain(BWAPI::UnitTypes::Protoss_High_Templar))
            {
                trainUnit(BWAPI::UnitTypes::Protoss_High_Templar, unit);
            }
            else if (unit->canTrain(BWAPI::UnitTypes::Protoss_Dragoon))
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Dragoon, unit);
                //cout << "Training Dragoon\n";
            }
            else
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Zealot, unit);
            }
        }
        /*else if (type == Protoss_Stargate && !unit->isTraining() && !alreadySentRequest(unit->getID()))
        {
            if (unit->canTrain(Protoss_Corsair))
            {
                trainUnit(Protoss_Corsair, unit);
            }
        }*/
        else if (unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility && !unit->isTraining() && !alreadySentRequest(unit->getID()) && unit->canTrain(BWAPI::UnitTypes::Protoss_Observer))
        {
            int observerCount = 0;
            for (BWAPI::Unit unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer) observerCount++;
            }

            if (observerCount < 4)
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Observer, unit);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Cybernetics_Core && !unit->isUpgrading())
        {
            /*if (unit->canUpgrade(BWAPI::UpgradeTypes::Singularity_Charge) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Singularity_Charge);
            }*/
        }
        else if (type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun && !unit->isUpgrading())
        {
            /*if (unit->canUpgrade(BWAPI::UpgradeTypes::Leg_Enhancements) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Leg_Enhancements);
            }*/

        }
        else if (type == BWAPI::UnitTypes::Protoss_Forge && !unit->isUpgrading())
        {
            /*if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Armor);
            }
            else if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
            }
            else if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Plasma_Shields) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
            }*/
        }
        else if (type == BWAPI::UnitTypes::Protoss_Templar_Archives && !unit->isUpgrading())
        {
            /*if (unit->canUpgrade(BWAPI::TechTypes::Psionic_Storm))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Armor);
                continue;
            }

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
                continue;
            }

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Plasma_Shields))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
                continue;
            }*/
        }
        else if (type == BWAPI::UnitTypes::Protoss_Observatory)
        {
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Sensor_Array) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Sensor_Array);
            }
            else if (unit->canUpgrade(BWAPI::UpgradeTypes::Gravitic_Boosters) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Gravitic_Boosters);
            }
        }
    }
}

BWAPI::Unit BuildManager::getUnitToBuild(BWAPI::Position position)
{
    return commanderReference->getUnitToBuild(position);
}

std::vector<NexusEconomy> BuildManager::getNexusEconomies()
{
    return commanderReference->getNexusEconomies();
}

std::vector<Builder> BuildManager::getBuilders()
{
    //Need to check if this is not passing back a reference.
    return builders;
}