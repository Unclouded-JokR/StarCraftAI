#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "SpenderManager.h"
#include "BuildingPlacer.h"
#include "Builder.h"

using namespace BWAPI;
using namespace Tools;
using namespace All;
using namespace UnitTypes;
using namespace std;

BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference), spenderManager(new SpenderManager(this)), buildingPlacer(new BuildingPlacer(this))
{

}

#pragma region BWAPI EVENTS
void BuildManager::onStart()
{
    //Make false at the start of a game.
    std::cout << "Builder Manager Initialized" << "\n";
    buildOrderCompleted = false;
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

    //Remove everything under here
    buildQueue.clear();

    if (!buildOrderCompleted)
    {
        updateBuild();
        runBuildQueue();
        runUnitQueue();
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

    if (unit->getType() != BWAPI::UnitTypes::Protoss_Pylon) buildingWarps.insert(unit);
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

void BuildManager::onUnitMorph(BWAPI::Unit unit)
{
    spenderManager->onUnitMorph(unit);
}

void BuildManager::onUnitComplete(BWAPI::Unit unit)
{
    buildings.insert(unit);
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
    Builder temp = Builder(unit, building);
    temp.positionToBuild = positionToBuild;

    builders.push_back(temp);
}

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
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
            if (unit->canTrain(Protoss_High_Templar))
            {
                trainUnit(BWAPI::UnitTypes::Protoss_High_Templar, unit);
            }
            else if (unit->canTrain(Protoss_Dragoon))
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
        else if (unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility && !unit->isTraining() && !alreadySentRequest(unit->getID()) && unit->canTrain(Protoss_Observer))
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
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Singularity_Charge) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Singularity_Charge);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun && !unit->isUpgrading())
        {
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Leg_Enhancements) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Leg_Enhancements);
            }

        }
        else if (type == BWAPI::UnitTypes::Protoss_Forge && !unit->isUpgrading())
        {
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && !upgradeAlreadyRequested(unit))
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
            }
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

void BuildManager::PvT_2Gateway_Observer() {
    currentBuild = "PvT_2Gateway_Observer";

    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;
    buildOrderCompleted = true;
    return;

    buildQueue[Protoss_Pylon] = (currentSupply >= 8) + (currentSupply >= 15) + (currentSupply >= 22) + ((currentSupply >= 31));
    buildQueue[Protoss_Gateway] = (currentSupply >= 10) + (currentSupply >= 29);
    buildQueue[Protoss_Assimilator] = (currentSupply >= 12);
    buildQueue[Protoss_Cybernetics_Core] = (currentSupply >= 14);
    buildQueue[Protoss_Robotics_Facility] = (currentSupply >= 25);
    buildQueue[Protoss_Observatory] = (currentSupply >= 38);
    //buildQueue[Protoss_Nexus] = (currentSupply >= 20);

    /*unitQueue[Protoss_Dragoon] = com(Protoss_Cybernetics_Core) > 0;
    unitQueue[Protoss_Observer] = com(Protoss_Observatory) > 0;*/

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

vector<BuildManager::BuildList> BuildManager::getBuildOrders(BWAPI::Race race) {
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