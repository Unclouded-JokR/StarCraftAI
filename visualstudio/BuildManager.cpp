#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "SpenderManager.h"
#include "BuildingPlacer.h"
#include "Builder.h"

BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
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
    spenderManager.onStart();
    builders.clear();
}

void BuildManager::onFrame() {
    spenderManager.OnFrame(resourceRequests);


    for (std::vector<ResourceRequest>::iterator it = resourceRequests.begin(); it != resourceRequests.end();)
    {
        (it->state == ResourceRequest::State::Accepted_Completed) ? it = resourceRequests.erase(it) : it++;
    }

    for (ResourceRequest& request : resourceRequests)
    {
        if (request.state != ResourceRequest::State::Approved_InProgress)
        {
            continue;
        }

        switch (request.type)
        {
            case ResourceRequest::Type::Unit:
            {
                if (request.requestedBuilding->canTrain(request.unit) &&
                    !request.requestedBuilding->isTraining() &&
                    request.requestedBuilding->isCompleted())
                {
                    request.requestedBuilding->train(request.unit);
                    request.state = ResourceRequest::State::Accepted_Completed;
                }

                break;
            }
            case ResourceRequest::Type::Building:
            {
                //Should change this to consider distance measure but is fine for now.
                const BWAPI::Position locationToPlace = buildingPlacer.getPositionToBuild(request.unit);
                const BWAPI::Unit workerAvalible = getUnitToBuild(locationToPlace);

                if (workerAvalible == nullptr) continue;

                //Skip path generation for now until bug is fixed.
                //const Path pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), locationToPlace);
                Path pathToLocation;

                Builder temp = Builder(workerAvalible, request.unit, locationToPlace, pathToLocation);
                builders.push_back(temp);
               
                std::cout << "Building " << request.unit << "\n";
                std::cout << request.state << "\n";
                request.state = ResourceRequest::State::Approved_BeingBuilt;
                std::cout << request.state << "\n";
                break;
            }
            case ResourceRequest::Type::Upgrade:
            {
                if (request.requestedBuilding->canUpgrade(request.upgrade) &&
                    !request.requestedBuilding->isUpgrading() &&
                    request.requestedBuilding->isCompleted())
                {
                    request.requestedBuilding->upgrade(request.upgrade);
                    request.state = ResourceRequest::State::Accepted_Completed;
                }
                break;
            }
            case ResourceRequest::Type::Tech:
            {
                if (request.requestedBuilding->canResearch(request.upgrade) &&
                    !request.requestedBuilding->isResearching() &&
                    request.requestedBuilding->isCompleted())
                {
                    request.requestedBuilding->upgrade(request.upgrade);
                    request.state = ResourceRequest::State::Accepted_Completed;
                }
                break;
            }
        }
    }


    //build order check here

    for (Builder builder : builders)
    {
        builder.onFrame();
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

    //std::cout << "Created " << unit->getType() << "\n";

    buildingPlacer.onUnitCreate(unit);

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

    //Remove worker once a building is being warped in.
    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
    {
        if (unit->getType() == it->buildingToConstruct)
        {
            it = builders.erase(it);
            break;
        }
    }

    if (unit->getType().isBuilding() && !unit->isCompleted()) buildings.insert(unit);
}


void BuildManager::onUnitDestroy(BWAPI::Unit unit)
{
    buildingPlacer.onUnitDestroy(unit);

    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end();)
    {
        if (it->getUnitReference()->getID() == unit->getID())
        {
            const BWAPI::Unit unitAvalible = getUnitToBuild(it->requestedPositionToBuild);
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

    //Check if a building has been killed
    for (BWAPI::Unit building : buildings)
    {
        if (building == unit)
        {
            buildings.erase(unit);
            return;
        }
    }
}

void BuildManager::onUnitMorph(BWAPI::Unit unit)
{
    buildingPlacer.onUnitMorph(unit);

    std::cout << "Created " << unit->getType() << "\n";

    if (unit->getType() == BWAPI::UnitTypes::Protoss_Assimilator && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
        {
            if (unit->getType() == it->buildingToConstruct)
            {
                it = builders.erase(it);
                break;
            }
        }

        buildings.insert(unit);
    }
}

void BuildManager::onUnitComplete(BWAPI::Unit unit)
{

}

void BuildManager::onUnitDiscover(BWAPI::Unit unit)
{
    buildingPlacer.onUnitDiscover(unit);
}
#pragma endregion

#pragma region Spender Manager Methods
/// <summary>
/// Using these methods for now to get this working but it should be refactored later.
/// </summary>
/// <param name="building"></param>
void BuildManager::buildBuilding(BWAPI::UnitType building)
{
    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = building;

    resourceRequests.push_back(request);
    std::cout << "Making Building request\n";
}

void BuildManager::trainUnit(BWAPI::UnitType unitToTrain, BWAPI::Unit unit)
{
    ResourceRequest request;
    request.type = ResourceRequest::Type::Unit;
    request.unit = unitToTrain;
    request.requestedBuilding = unit;

    resourceRequests.push_back(request);
    std::cout << "Making Unit request\n";
}

void BuildManager::buildUpgadeType(BWAPI::Unit unit, BWAPI::UpgradeType upgrade)
{
    ResourceRequest request;
    request.type = ResourceRequest::Type::Upgrade;
    request.upgrade = upgrade;
    request.requestedBuilding = unit;

    resourceRequests.push_back(request);
}

bool BuildManager::alreadySentRequest(int unitID)
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

bool BuildManager::requestedBuilding(BWAPI::UnitType building)
{
    for (const ResourceRequest& request : resourceRequests)
    {
        if (building == request.unit) return true;
    }
    return false;
}

bool BuildManager::upgradeAlreadyRequested(BWAPI::Unit building)
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

bool BuildManager::checkUnitIsPlanned(BWAPI::UnitType building)
{
    for (const ResourceRequest& request : resourceRequests)
    {
        if (building == request.unit && request.state == ResourceRequest::State::Approved_InProgress) return true;
    }
    return false;
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
    return spenderManager.plannedSupply(resourceRequests);
}
#pragma endregion

void BuildManager::createBuilder(BWAPI::Unit unit, BWAPI::UnitType building, BWAPI::Position positionToBuild)
{
    /*Builder temp = Builder(unit, building, positionToBuild);
    builders.push_back(temp);*/
}

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
}

bool BuildManager::checkUnitIsBeingWarpedIn(BWAPI::UnitType unit)
{
    for (BWAPI::Unit building : buildings)
    {
        if (unit == building->getType() && !building->isCompleted())
        {
            return true;
        }
    }

    return false;
}

void BuildManager::buildingDoneWarping(BWAPI::Unit unit)
{
    

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