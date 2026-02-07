#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "SpenderManager.h"
#include "BuildingPlacer.h"
#include "Builder.h"
#include <cmath>

BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

int BuildManager::countMyUnits(BWAPI::UnitType type) const
{
    int c = 0;
    for (auto u : BWAPI::Broodwar->self()->getUnits())
    {
        if (u && u->getType() == type) c++;
    }
    return c;
}

int BuildManager::countPlannedBuildings(BWAPI::UnitType type) const
{
    int c = 0;
    for (const auto& r : resourceRequests)
    {
        if (r.type == ResourceRequest::Type::Building && r.unit == type)
        {
            if (r.state != ResourceRequest::State::Accepted_Completed) c++;
        }
    }
    return c;
}

BWAPI::TilePosition BuildManager::findNaturalRampPlacement(BWAPI::UnitType type) const
{
    const auto* choke = BWEB::Map::getNaturalChoke();
    if (!choke) return BWAPI::TilePositions::Invalid;

    // Prefer the main-side (upper/inner) part of the main<->natural ramp.
    const BWAPI::TilePosition mainTile = BWEB::Map::getMainTile();
    const BWAPI::Position mainPos = BWEB::Map::getMainPosition();

    const BWAPI::Position chokeMainPos = BWEB::Map::getClosestChokeTile(choke, mainPos);
    const BWAPI::TilePosition chokeTile(chokeMainPos);

    const int dirX = (mainTile.x > chokeTile.x) ? 1 : (mainTile.x < chokeTile.x ? -1 : 0);
    const int dirY = (mainTile.y > chokeTile.y) ? 1 : (mainTile.y < chokeTile.y ? -1 : 0);

    // Step further toward main so we don't place in the center of the choke (avoid blocking).
    const BWAPI::TilePosition anchor = chokeTile + BWAPI::TilePosition(dirX, dirY) * 6;

    const BWAPI::TilePosition chokeCenterTile(choke->Center());
    const int noBuildRadius = 2;

    const int w = type.tileWidth();
    const int h = type.tileHeight();

    auto inBounds = [&](const BWAPI::TilePosition& t) {
        return t.isValid()
            && t.x >= 0 && t.y >= 0
            && (t.x + w) <= BWAPI::Broodwar->mapWidth()
            && (t.y + h) <= BWAPI::Broodwar->mapHeight();
    };

    const BWEM::Area* mainArea = BWEB::Map::getMainArea();
    const BWEM::Area* natArea  = BWEB::Map::getNaturalArea();

    auto isInArea = [&](const BWAPI::TilePosition& t, const BWEM::Area* a) {
        return a && BWEB::Map::mapBWEM.GetArea(t) == a;
    };

    auto acceptable = [&](const BWAPI::TilePosition& t) {
        if (!inBounds(t)) return false;

        // Don't place right on the choke center region.
        if (std::abs(t.x - chokeCenterTile.x) <= noBuildRadius &&
            std::abs(t.y - chokeCenterTile.y) <= noBuildRadius)
            return false;

        if (!BWEB::Map::isPlaceable(type, t)) return false;
        if (BWEB::Map::isReserved(t, w, h)) return false;
        if (BWEB::Map::isUsed(t, w, h) != BWAPI::UnitTypes::None) return false;
        return true;
    };

    const int maxR = 10;

    // Pass 1: main area only (upper/inner).
    for (int r = 0; r <= maxR; r++)
    {
        for (int dx = -r; dx <= r; dx++)
        {
            for (int dy = -r; dy <= r; dy++)
            {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                const BWAPI::TilePosition t = anchor + BWAPI::TilePosition(dx, dy);
                if (!isInArea(t, mainArea)) continue;
                if (acceptable(t)) return t;
            }
        }
    }

    // Pass 2: fallback to natural area if needed.
    for (int r = 0; r <= maxR; r++)
    {
        for (int dx = -r; dx <= r; dx++)
        {
            for (int dy = -r; dy <= r; dy++)
            {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                const BWAPI::TilePosition t = anchor + BWAPI::TilePosition(dx, dy);
                if (!isInArea(t, natArea)) continue;
                if (acceptable(t)) return t;
            }
        }
    }

    return BWAPI::TilePositions::Invalid;
}

void BuildManager::buildSupplyAtNaturalRamp()
{
    BWAPI::UnitType supply;
    if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
        supply = BWAPI::UnitTypes::Protoss_Pylon;
    else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
        supply = BWAPI::UnitTypes::Terran_Supply_Depot;
    else
        return;

    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = supply;

    const BWAPI::TilePosition t = findNaturalRampPlacement(supply);
    if (t.isValid())
    {
        request.useForcedTile = true;
        request.forcedTile = t;
        BWEB::Map::addReserve(t, supply.tileWidth(), supply.tileHeight());
    }

    resourceRequests.push_back(request);
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
    for (std::vector<ResourceRequest>::iterator it = resourceRequests.begin(); it != resourceRequests.end();)
    {
        (it->state == ResourceRequest::State::Accepted_Completed) ? it = resourceRequests.erase(it) : it++;
    }

    spenderManager.OnFrame(resourceRequests);

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
                if (request.isCheese)
                {
                    request.state = ResourceRequest::State::Approved_BeingBuilt;
                }
                else
                {
                    BWAPI::Position locationToPlace;
                    if (request.useForcedTile && request.forcedTile.isValid())
                    {
                        locationToPlace = BWAPI::Position(request.forcedTile);
                    }
                    else
                    {
                        locationToPlace = buildingPlacer.getPositionToBuild(request.unit);
                    }
                    const BWAPI::Unit workerAvalible = getUnitToBuild(locationToPlace);

                    if (workerAvalible == nullptr) continue;

                    //For now dont use Astar to get path to location
                    Path pathToLocation;
                    if (request.unit.isResourceDepot())
                    {
                        //do nothing for now
                    }
                    else if(request.unit.isRefinery())
                    {
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), locationToPlace, true);
                    }
                    else
                    {
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), locationToPlace);
                    }

                    Builder temp = Builder(workerAvalible, request.unit, locationToPlace, pathToLocation);
                    builders.push_back(temp);

                    request.state = ResourceRequest::State::Approved_BeingBuilt;
                }
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

    for (auto it = builders.begin(); it != builders.end(); )
    {
        it->onFrame();

        if (it->hasGivenUp())
        {
            // Release any BWEB reservation for the target tile to avoid permanent locks.
            if (it->requestedTileToBuild.isValid())
            {
                BWEB::Map::removeReserve(it->requestedTileToBuild, it->buildingToConstruct.tileWidth(), it->buildingToConstruct.tileHeight());
            }
            it = builders.erase(it);
        }
        else
        {
            ++it;
        }
    }

    //Debug
    //Will most likely need to add a building data class to make this easier to be able to keep track of buildings and what units they are creating.
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

    // Force the pylon to be placed on the main-side of the natural ramp.
    if (building == BWAPI::UnitTypes::Protoss_Pylon
        && BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss
        && !firstPylonForced)
    {
        const int have = countMyUnits(BWAPI::UnitTypes::Protoss_Pylon);
        const int planned = countPlannedBuildings(BWAPI::UnitTypes::Protoss_Pylon);
        if (have + planned == 0)
        {
            const BWAPI::TilePosition t = findNaturalRampPlacement(building);
            if (t.isValid())
            {
                request.useForcedTile = true;
                request.forcedTile = t;
                BWEB::Map::addReserve(t, 2, 2);
                firstPylonForced = true;
            }
        }
    }

    resourceRequests.push_back(request);
}

void BuildManager::buildBuilding(BWAPI::UnitType building, BWAPI::Unit scout)
{
    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = building;
    request.scoutToPlaceBuilding = scout;
    request.isCheese = true;

    resourceRequests.push_back(request);
}

void BuildManager::trainUnit(BWAPI::UnitType unitToTrain, BWAPI::Unit unit)
{
    ResourceRequest request;
    request.type = ResourceRequest::Type::Unit;
    request.unit = unitToTrain;
    request.requestedBuilding = unit;

    resourceRequests.push_back(request);
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
        if (building == request.unit && !request.isCheese) return true;
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
        if (building == request.unit && request.state == ResourceRequest::State::Approved_InProgress && !request.isCheese) return true;
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
    return spenderManager.plannedSupply(resourceRequests, buildings);
}
#pragma endregion

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

bool BuildManager::cheeseIsApproved(BWAPI::Unit unit)
{
    for (ResourceRequest& request : resourceRequests)
    {
        if (request.type != ResourceRequest::Type::Building && request.isCheese) continue;
        
        if (request.scoutToPlaceBuilding == unit && request.state == ResourceRequest::State::Approved_BeingBuilt) return true;
    }

    return false;
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

BWAPI::TilePosition BuildManager::getNaturalRampTile(bool preferMainSide) const
{
    const auto* choke = BWEB::Map::getNaturalChoke();
    if (!choke) return BWAPI::TilePositions::Invalid;

    const BWAPI::Position refPos = preferMainSide
        ? BWEB::Map::getMainPosition()
        : BWEB::Map::getNaturalPosition();

    const BWAPI::Position chokePos = BWEB::Map::getClosestChokeTile(choke, refPos);
    return BWAPI::TilePosition(chokePos);
}

BWAPI::TilePosition BuildManager::getNaturalRampTileTowardMain(int tilesTowardMain) const
{
    const auto* choke = BWEB::Map::getNaturalChoke();
    if (!choke) return BWAPI::TilePositions::Invalid;

    const BWAPI::TilePosition mainTile = BWEB::Map::getMainTile();
    const BWAPI::Position mainPos = BWEB::Map::getMainPosition();

    // A ramp tile closest to main
    const BWAPI::TilePosition rampTile(BWEB::Map::getClosestChokeTile(choke, mainPos));

    // Direction from ramp toward main
    const int dx = (mainTile.x > rampTile.x) ? 1 : (mainTile.x < rampTile.x ? -1 : 0);
    const int dy = (mainTile.y > rampTile.y) ? 1 : (mainTile.y < rampTile.y ? -1 : 0);

    BWAPI::TilePosition anchor = rampTile + BWAPI::TilePosition(dx, dy) * tilesTowardMain;

    // Clamp to map bounds
    anchor.x = std::max(0, std::min(anchor.x, BWAPI::Broodwar->mapWidth() - 1));
    anchor.y = std::max(0, std::min(anchor.y, BWAPI::Broodwar->mapHeight() - 1));

    return anchor;
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
