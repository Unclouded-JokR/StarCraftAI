#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "SpenderManager.h"
#include "BuildingPlacer.h"
#include "Builder.h"

// Returns true if tile is valid, in bounds, and BWAPI considers it buildable for the given type.
static bool isValidBuildTile(BWAPI::UnitType type, const BWAPI::TilePosition& t)
{
    if (!t.isValid()) return false;

    const int w = type.tileWidth();
    const int h = type.tileHeight();

    if (t.x < 0 || t.y < 0) return false;
    if ((t.x + w) > BWAPI::Broodwar->mapWidth())  return false;
    if ((t.y + h) > BWAPI::Broodwar->mapHeight()) return false;

    if (!BWAPI::Broodwar->canBuildHere(t, type)) return false;

    return true;
}

// Returns true if the footprint is within bounds and all underlying tiles are buildable terrain.
// This intentionally ignores Protoss power (so we can plan walls before the pylon is finished).
static bool isTerrainBuildable(BWAPI::UnitType type, const BWAPI::TilePosition& t)
{
    if (!t.isValid()) return false;
    const int w = type.tileWidth();
    const int h = type.tileHeight();
    if (t.x < 0 || t.y < 0) return false;
    if ((t.x + w) > BWAPI::Broodwar->mapWidth())  return false;
    if ((t.y + h) > BWAPI::Broodwar->mapHeight()) return false;

    for (int dx = 0; dx < w; dx++) {
        for (int dy = 0; dy < h; dy++) {
            BWAPI::TilePosition tt(t.x + dx, t.y + dy);
            if (!BWAPI::Broodwar->isBuildable(tt))
                return false;
        }
    }
    return true;
}

static bool footprintContainsTile(BWAPI::UnitType type, const BWAPI::TilePosition& topLeft, const BWAPI::TilePosition& tile)
{
    return tile.x >= topLeft.x && tile.x < (topLeft.x + type.tileWidth())
        && tile.y >= topLeft.y && tile.y < (topLeft.y + type.tileHeight());
}

static int footprintPathOverlapCount(BWAPI::UnitType type, const BWAPI::TilePosition& topLeft, const std::vector<BWAPI::TilePosition>& pathTiles)
{
    int overlap = 0;
    for (const auto& tile : pathTiles)
    {
        if (footprintContainsTile(type, topLeft, tile))
            overlap++;
    }
    return overlap;
}

static int minTileDistanceToPath(const BWAPI::TilePosition& tile, const std::vector<BWAPI::TilePosition>& pathTiles)
{
    if (!tile.isValid() || pathTiles.empty())
        return INT_MAX / 4;

    int best = INT_MAX / 4;
    for (const auto& pt : pathTiles)
        best = std::min(best, std::abs(pt.x - tile.x) + std::abs(pt.y - tile.y));
    return best;
}

static int footprintDistanceToTile(BWAPI::UnitType type, const BWAPI::TilePosition& topLeft, const BWAPI::TilePosition& tile)
{
    if (!topLeft.isValid() || !tile.isValid())
        return INT_MAX / 4;

    int best = INT_MAX / 4;
    for (int dx = 0; dx < type.tileWidth(); dx++)
    {
        for (int dy = 0; dy < type.tileHeight(); dy++)
        {
            const BWAPI::TilePosition footprintTile(topLeft.x + dx, topLeft.y + dy);
            best = std::min(best, std::abs(footprintTile.x - tile.x) + std::abs(footprintTile.y - tile.y));
        }
    }
    return best;
}

static int wallDistanceToAnchor(BWEB::Wall* wall, const BWAPI::TilePosition& anchor)
{
    if (!wall || !anchor.isValid())
        return INT_MAX / 4;

    int best = INT_MAX / 4;
    for (const auto& t : wall->getSmallTiles())
        best = std::min(best, footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Pylon, t, anchor));
    for (const auto& t : wall->getMediumTiles())
        best = std::min(best, footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Gateway, t, anchor));
    for (const auto& t : wall->getLargeTiles())
        best = std::min(best, footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Gateway, t, anchor));
    return best;
}

static std::vector<BWAPI::TilePosition> uniquePathTiles(const Path& path)
{
    std::vector<BWAPI::TilePosition> tiles;
    for (const auto& pos : path.positions)
    {
        const BWAPI::TilePosition tile(pos);
        if (!tile.isValid())
            continue;
        if (std::find(tiles.begin(), tiles.end(), tile) == tiles.end())
            tiles.push_back(tile);
    }
    return tiles;
}


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
    //std::cout << "Builder Manager Initialized" << "\n";

    // Reset per-game state
    buildOrders.clear();
    activeBuildOrderIndex = -1;
    activeBuildOrderStep = 0;
    //Make false at the start of a game.
    buildOrderActive = false;
    buildOrderCompleted = false;

    resetNaturalWallPlan();

    spenderManager.onStart();
    buildingPlacer.onStart();
    builders.clear();

    initBuildOrdersOnStart();
    selectRandomBuildOrder();
}


void BuildManager::onFrame() {
    for (std::vector<ResourceRequest>::iterator it = resourceRequests.begin(); it != resourceRequests.end();)
    {
        if (it->state == ResourceRequest::State::Accepted_Completed || it->attempts == MAX_ATTEMPTS)
        {
            //if (it->state == ResourceRequest::State::Accepted_Completed) std::cout << "Completed Request\n";
            //if (it->attempts == MAX_ATTEMPTS) std::cout << "Killing request to build " << it->unit << "\n";

            it = resourceRequests.erase(it);
        }
        else
        {
            it++;
        }
    }

    runBuildOrderOnFrame();

    spenderManager.OnFrame(resourceRequests);
    buildingPlacer.drawPoweredTiles();

    const auto* naturalChoke = BWEB::Map::getNaturalChoke();
    if (naturalChoke)
    {

        if (naturalWallOpeningTile.isValid())
        {
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(naturalWallOpeningTile), BWAPI::Position(naturalWallOpeningTile) + BWAPI::Position(32, 32), BWAPI::Colors::Orange, false);
            BWAPI::Broodwar->drawTextMap(BWAPI::Position(naturalWallOpeningTile) + BWAPI::Position(4, 4), "Wall gap / A*");
        }

        for (const auto& t : naturalWallPathTiles)
        {
            if (!t.isValid())
                continue;

            const BWAPI::Position tl = BWAPI::Position(t);
            const BWAPI::Position br = tl + BWAPI::Position(32, 32);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Yellow, false);
        }

        if (naturalWallPylonTile.isValid())
        {
            const BWAPI::Position tl = BWAPI::Position(naturalWallPylonTile);
            const BWAPI::Position br = tl + BWAPI::Position(64, 64);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Green, false);
            BWAPI::Broodwar->drawTextMap(tl + BWAPI::Position(4, 4), "Wall pylon");
        }

        for (const auto& t : naturalWallCannonTiles)
        {
            if (!t.isValid())
                continue;

            const BWAPI::Position tl = BWAPI::Position(t);
            const BWAPI::Position br = tl + BWAPI::Position(64, 64);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Purple, false);
            BWAPI::Broodwar->drawTextMap(tl + BWAPI::Position(4, 4), "Wall cannon");
        }

        for (const auto& t : naturalWallGatewayTiles)
        {
            if (!t.isValid())
                continue;

            const BWAPI::Position tl = BWAPI::Position(t);
            const BWAPI::Position br = tl + BWAPI::Position(128, 96);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Red, false);
            BWAPI::Broodwar->drawTextMap(tl + BWAPI::Position(4, 4), "Wall gateway");
        }
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
                    request.requestedBuilding->isCompleted() &&
                    request.requestedBuilding->isPowered())
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
                    //  for natural walling, bypass PlacementInfo and use forcedTile calculation to build.

                    BWAPI::Position placementPos = BWAPI::Positions::Invalid;
                    PlacementInfo placementInfo;

                    if (request.useForcedTile)
                    {
                        const BWAPI::TilePosition tileToPlace = request.forcedTile;

                        // If forced tile is out of bounds, drop it
                        if (!tileToPlace.isValid()
                            || tileToPlace.x < 0 || tileToPlace.y < 0
                            || (tileToPlace.x + request.unit.tileWidth()) > BWAPI::Broodwar->mapWidth()
                            || (tileToPlace.y + request.unit.tileHeight()) > BWAPI::Broodwar->mapHeight())
                        {
                            request.useForcedTile = false;
                            request.forcedTile = BWAPI::TilePositions::Invalid;
                            continue;
                        }

                        // If terrain itself isn't buildable, drop forced tile
                        if (!isTerrainBuildable(request.unit, tileToPlace))
                        {
                            request.useForcedTile = false;
                            request.forcedTile = BWAPI::TilePositions::Invalid;
                            continue;
                        }

                        // ...or if missing pylon, wait.
                        if (!BWAPI::Broodwar->canBuildHere(tileToPlace, request.unit))
                        {
                            const bool needsPower = (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss && request.unit.requiresPsi());
                            if (needsPower && !BWAPI::Broodwar->hasPower(tileToPlace, request.unit))
                            {
                                continue;
                            }

                            // Otherwise it may be blocked by a moving unit, retry later
                            continue;
                        }

                        placementPos = BWAPI::Position(tileToPlace);
                    }
                    else
                    {
                        placementInfo = buildingPlacer.getPositionToBuild(request.unit);

                        if (placementInfo.position == BWAPI::Positions::Invalid)
                        {
                            const PlacementInfo::PlacementFlag flag_info = placementInfo.flag;

                            switch (flag_info)
                            {
                            case PlacementInfo::NO_POWER:
                                //should create a new pylon request
                                //std::cout << "FAILED: NO POWER\n";
                                break;
                            case PlacementInfo::NO_BLOCKS:
                                //Wait for a bit and kill the request if no blocks are added
                                //std::cout << "FAILED: NO BLOCKS\n";
                                break;
                            case PlacementInfo::NO_GYSERS:
                                //Shouldnt happen but okay
                                //std::cout << "FAILED: NO GYSERS\n";
                                break;
                            case PlacementInfo::NO_PLACEMENTS:
                                //Wait for a bit and kill the request if no placements are added.
                                //std::cout << "FAILED: NO PLACEMENTS\n";
                                break;
                            case PlacementInfo::NO_EXPANSIONS:
                                //kill the expansion request.
                                //std::cout << "FAILED: NO EXPANSION\n";
                                break;
                            }

                            request.attempts++;

                            continue;
                        }

                        placementPos = placementInfo.position;
                    }

                    const BWAPI::Unit workerAvalible = getUnitToBuild(placementPos);

                    if (workerAvalible == nullptr) continue;


                    Path pathToLocation;
                    if (request.unit.isResourceDepot())
                    {
                        //std::cout << "Trying to build Nexus\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), placementPos);
                    }
                    else if(request.unit.isRefinery())
                    {
                        //std::cout << "Trying to build assimlator\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), placementPos, true);
                    }
                    else
                    {                                                                                                                           
                        //std::cout << "Trying to build regular building\n";
                        pathToLocation = AStar::GeneratePath(workerAvalible->getPosition(), workerAvalible->getType(), placementPos);
                    }

                    Builder temp = Builder(workerAvalible, request.unit, placementPos, pathToLocation);
                    builders.push_back(temp);

                    request.state = ResourceRequest::State::Approved_BeingBuilt;

                    BWEB::Map::addUsed(placementInfo.topLeft, request.unit);
                }
                break;
            }
            case ResourceRequest::Type::Upgrade:
            {
                if (request.requestedBuilding->canUpgrade(request.upgrade) &&
                    !request.requestedBuilding->isUpgrading() &&
                    request.requestedBuilding->isCompleted() &&
                    request.requestedBuilding->isPowered())
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
                    request.requestedBuilding->isCompleted() &&
                    request.requestedBuilding->isPowered())
                {
                    request.requestedBuilding->upgrade(request.upgrade);
                    request.state = ResourceRequest::State::Accepted_Completed;
                }
                break;
            }
        }
    }

    for (Builder& builder : builders)
    {
        builder.onFrame();
    }

    //Debug
    //Will most likely need to add a building data class to make this easier to be able to keep track of buildings and what units they are creating.
    /*for (BWAPI::Unit building : buildings)
    {
        BWAPI::Broodwar->drawTextMap(building->getPosition(), std::to_string(building->getID()).c_str());
    }*/

    pumpUnit();
}

void BuildManager::onUnitCreate(BWAPI::Unit unit)
{
    if (unit == nullptr) return;

    buildingPlacer.onUnitCreate(unit);

    if (unit->getPlayer() == BWAPI::Broodwar->self())
    {
        switch (unit->getType())
        {
            case BWAPI::UnitTypes::Protoss_Gateway:
                if (requestCounter.gateway_requests > 0)
                    --requestCounter.gateway_requests;
                break;
            case BWAPI::UnitTypes::Protoss_Nexus:
                if (requestCounter.nexus_requests > 0)
                    --requestCounter.nexus_requests;
                break;
            case BWAPI::UnitTypes::Protoss_Forge:
                if (requestCounter.forge_requests > 0)
                    --requestCounter.forge_requests;
                break;
            case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
                if (requestCounter.cybernetics_requests > 0)
                    --requestCounter.cybernetics_requests;
                break;
            case BWAPI::UnitTypes::Protoss_Robotics_Facility:
                if (requestCounter.robotics_requests > 0)
                    --requestCounter.robotics_requests;
                break;
            case BWAPI::UnitTypes::Protoss_Observatory:
                if (requestCounter.observatory_requests > 0)
                    --requestCounter.observatory_requests;
                break;

            case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
                if (requestCounter.citadel_requests > 0)
                    --requestCounter.citadel_requests;
                break;

            case BWAPI::UnitTypes::Protoss_Templar_Archives:
                if (requestCounter.templarArchives_requests > 0)
                    --requestCounter.templarArchives_requests;
                break;
            default:
                break;
        }
    }

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

    if (unit->getPlayer() != BWAPI::Broodwar->self()) return;

    //std::cout << unit->getType() << " placed down at tile position " << unit->getTilePosition() << "\n";

    //Remove worker once a building is being warped in.
    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end(); ++it)
    {
        if (unit->getTilePosition() == BWAPI::TilePosition(it->requestedPositionToBuild) && unit->getType() == it->buildingToConstruct)
        {
            //std::cout << "Builder placed down " << unit->getType() << ", removing from builders\n";
            it = builders.erase(it);
            break;
        }
    }

    if (unit->getType().isBuilding() && !unit->isCompleted()) buildings.insert(unit);
}


void BuildManager::onUnitDestroy(BWAPI::Unit unit)
{
    buildingPlacer.onUnitDestroy(unit);

    if (unit->getPlayer() != BWAPI::Broodwar->self())
        return;

    for (std::vector<Builder>::iterator it = builders.begin(); it != builders.end();)
    {
        if (it->getUnitReference()->getID() == unit->getID())
        {
            //std::cout << "Builder has died\n";

            const BWAPI::Unit unitAvalible = getUnitToBuild(it->requestedPositionToBuild);

            if (unitAvalible != nullptr)
            {
                Path pathToLocation;
                if (it->buildingToConstruct.isResourceDepot())
                {
                    //std::cout << "Trying to build Nexus\n";
                    pathToLocation = AStar::GeneratePath(unitAvalible->getPosition(), unitAvalible->getType(), it->requestedPositionToBuild);
                }
                else if (it->buildingToConstruct.isRefinery())
                {
                    //std::cout << "Trying to build assimlator\n";
                    pathToLocation = AStar::GeneratePath(unitAvalible->getPosition(), unitAvalible->getType(), it->requestedPositionToBuild, true);
                }
                else
                {
                    //std::cout << "Trying to build regular building\n";
                    pathToLocation = AStar::GeneratePath(unitAvalible->getPosition(), unitAvalible->getType(), it->requestedPositionToBuild);
                }

                it->setUnitReference(unitAvalible);
                it->updatePath(pathToLocation);
                //std::cout << "Replacement found and updated path\n";
            }
            else
            {
                //std::cout << "Replacement could not be found\n";
            }

            break;
            /*else
            {
               switch (it->buildingToConstruct)
               {
                    case BWAPI::UnitTypes::Protoss_Gateway:
                        if (requestCounter.gateway_requests > 0)
                            --requestCounter.gateway_requests;
                        break;
                    case BWAPI::UnitTypes::Protoss_Nexus:
                        if (requestCounter.nexus_requests > 0)
                            --requestCounter.nexus_requests;
                        break;
                    case BWAPI::UnitTypes::Protoss_Forge:
                        if (requestCounter.forge_requests > 0)
                            --requestCounter.forge_requests;
                        break;
                    case BWAPI::UnitTypes::Protoss_Cybernetics_Core:
                        if (requestCounter.cybernetics_requests > 0)
                            --requestCounter.cybernetics_requests;
                        break;
                    case BWAPI::UnitTypes::Protoss_Robotics_Facility:
                        if (requestCounter.robotics_requests > 0)
                            --requestCounter.robotics_requests;
                        break;
                    case BWAPI::UnitTypes::Protoss_Observatory:
                        if (requestCounter.observatory_requests > 0)
                            --requestCounter.observatory_requests;
                        break;

                    case BWAPI::UnitTypes::Protoss_Citadel_of_Adun:
                        if (requestCounter.citadel_requests > 0)
                            --requestCounter.citadel_requests;
                        break;

                    case BWAPI::UnitTypes::Protoss_Templar_Archives:
                        if (requestCounter.templarArchives_requests > 0)
                            --requestCounter.templarArchives_requests;
                        break;
                    default:
                        break;
               }


            }
            */
        }

        it++;
    }

    BWAPI::UnitType unitType = unit->getType();

    if (!unitType.isBuilding()) return;

    //Check if a building has been killed
    for (const BWAPI::Unit building : buildings)
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

    //std::cout << "Created " << unit->getType() << " (On Morph)\n";

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
    buildingPlacer.onUnitComplete(unit);
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
    if (isBuildOrderActive() && isRestrictedTechBuilding(building))
    {
        return;
    }

    ResourceRequest request;
    request.type = ResourceRequest::Type::Building;
    request.unit = building;
    request.fromBuildOrder = false;

    switch (building)
    {
        case BWAPI::UnitTypes::Protoss_Gateway: 
            requestCounter.gateway_requests++; 
            break;
        case BWAPI::UnitTypes::Protoss_Nexus: 
            requestCounter.nexus_requests++; 
            break;
        case BWAPI::UnitTypes::Protoss_Forge: 
            requestCounter.forge_requests++; 
            break;
        case BWAPI::UnitTypes::Protoss_Cybernetics_Core: 
            requestCounter.cybernetics_requests++; 
            break;
        case BWAPI::UnitTypes::Protoss_Robotics_Facility: 
            requestCounter.robotics_requests++; 
            break;
        case BWAPI::UnitTypes::Protoss_Observatory: 
            requestCounter.observatory_requests++; 
            break;
        case BWAPI::UnitTypes::Protoss_Citadel_of_Adun: 
            requestCounter.citadel_requests++; 
            break;
        case BWAPI::UnitTypes::Protoss_Templar_Archives: 
            requestCounter.templarArchives_requests++; 
            break;
        default: 
            break;
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
    request.fromBuildOrder = false;

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
    const FriendlyUnitCounter ProtoBot_Units = commanderReference->informationManager.getFriendlyUnitCounter();
    const FriendlyBuildingCounter ProtoBot_Buildings = commanderReference->informationManager.getFriendlyBuildingCounter();
    const FriendlyUpgradeCounter ProtoBot_Upgrades = commanderReference->informationManager.getFriendlyUpgradeCounter();
    const int totalMinerals = BWAPI::Broodwar->self()->minerals();
    const int totalGas = BWAPI::Broodwar->self()->gas();

    for (BWAPI::Unit unit : buildings)
    {
        const BWAPI::UnitType type = unit->getType();
        if (type == BWAPI::UnitTypes::Protoss_Gateway && !unit->isTraining() && !alreadySentRequest(unit->getID()))
        {
            if (ProtoBot_Buildings.cyberneticsCore >= 1 && ProtoBot_Units.zealot >= 7)
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Dragoon, unit);
            }
            else
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Zealot, unit);
            }
        }
        else if (unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility && !unit->isTraining() && !alreadySentRequest(unit->getID()) && unit->canTrain(BWAPI::UnitTypes::Protoss_Observer))
        {
            if (ProtoBot_Units.observer < 4)
            {
                trainUnit(BWAPI::UnitTypes::Protoss_Observer, unit);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Cybernetics_Core && !unit->isUpgrading())
        {
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Singularity_Charge) && !upgradeAlreadyRequested(unit) && ProtoBot_Units.dragoon >= 1)
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Singularity_Charge);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun && !unit->isUpgrading())
        {
            if (ProtoBot_Buildings.gateway < 8 || ProtoBot_Buildings.templarArchives < 1) continue;

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Leg_Enhancements) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Leg_Enhancements);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Forge && !unit->isUpgrading())
        {
            if (ProtoBot_Buildings.gateway < 8) continue;

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
            }
            
            if (ProtoBot_Upgrades.singularityCharge != 1) continue;
            
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Protoss_Ground_Armor);
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
            /*if (unit->canUpgrade(BWAPI::UpgradeTypes::Sensor_Array) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Sensor_Array);
            }
            else if (unit->canUpgrade(BWAPI::UpgradeTypes::Gravitic_Boosters) && !upgradeAlreadyRequested(unit))
            {
                buildUpgadeType(unit, BWAPI::UpgradeTypes::Gravitic_Boosters);
            }*/
        }
    }
}

std::pair<int, int> BuildManager::getPlannedResources()
{
    return std::make_pair(spenderManager.getPlannedMinerals(resourceRequests), spenderManager.getPlannedGas(resourceRequests));
}

BWAPI::Unit BuildManager::getUnitToBuild(BWAPI::Position position)
{
    return commanderReference->getUnitToBuild(position);
}

std::vector<NexusEconomy> BuildManager::getNexusEconomies()
{
    return commanderReference->getNexusEconomies();
}


// ---------------------------
// Build order helpers / runner
// ---------------------------

bool BuildManager::isBuildOrderActive() const
{
    return buildOrderActive && !buildOrderCompleted && activeBuildOrderIndex >= 0 && activeBuildOrderIndex < (int)buildOrders.size();
}

bool BuildManager::isRestrictedTechBuilding(BWAPI::UnitType type) const
{
    // Hack: During an active build order, suspend most tech/production buildings calls outside the build order until build order is completed.
    if (type == BWAPI::UnitTypes::Protoss_Pylon) return false;
    if (type == BWAPI::UnitTypes::Protoss_Nexus) return false;
    if (type == BWAPI::UnitTypes::Protoss_Assimilator) return true;
    if (type == BWAPI::UnitTypes::Protoss_Gateway) return true;
    if (type == BWAPI::UnitTypes::Protoss_Forge) return true;
    if (type == BWAPI::UnitTypes::Protoss_Cybernetics_Core) return true;
    if (type == BWAPI::UnitTypes::Protoss_Robotics_Facility) return true;
    if (type == BWAPI::UnitTypes::Protoss_Observatory) return true;
    if (type == BWAPI::UnitTypes::Protoss_Stargate) return true;
    return false;
}

std::string BuildManager::buildOrderNameToString(int name) const
{
    switch (name)
    {
        case 1: return "2 Gateway Observer";
        case 2: return "3 Gate Robo";
        case 3: return "Proxy Gateways vs. Zerg";
        case 4: return "12 Nexus vs Terran";
        case 5: return "28 Nexus vs. Terran";
        case 6: return "9/9 Gateways vs. Zerg";
        case 7: return "9/10 Gateways vs. Zerg";
        case 8: return "10/12 Gateways vs. Zerg";
        case 9: return "9/9 Gateways vs. Protoss";
        case 10: return "9/10 Gateways vs. Protoss";
        case 11: return "10/12 Gateways vs. Protoss";
        case 12: return "4 Gate Goon vs. Protoss";
        default: return "Unknown";
    }
}

void BuildManager::initBuildOrdersOnStart()
{
    buildOrders = BuildOrders::createAll();
}

void BuildManager::selectBuildOrderAgainstRace(BWAPI::Race enemyRace)
{
    srand(time(0));

    if (buildOrders.empty())
    {
        buildOrderActive = false;
        buildOrderCompleted = true;
        return;
    }

    std::vector<int> candidates;

    if (enemyRace != BWAPI::Races::Unknown)
    {
        for (int i = 0; i < (int)buildOrders.size(); i++)
        {
            if (buildOrders[i].vsRace == enemyRace)
                candidates.push_back(i);
        }
    }

    if (candidates.empty())
    {
        for (int i = 0; i < (int)buildOrders.size(); i++)
            candidates.push_back(i);
    }

    //activeBuildOrderIndex = candidates[std::rand() % candidates.size()];
    activeBuildOrderIndex = 4;
    activeBuildOrderStep = 0;
    buildOrderActive = true;
    buildOrderCompleted = false;

    //std::cout << "Selected Build Order: " << buildOrderNameToString(buildOrders[activeBuildOrderIndex].name) << "\n";
}

void BuildManager::selectRandomBuildOrder()
{
    const auto enemyRace = (BWAPI::Broodwar->enemy() ? BWAPI::Broodwar->enemy()->getRace() : BWAPI::Races::Unknown);
    selectBuildOrderAgainstRace(enemyRace);
}

void BuildManager::clearBuildOrder(bool clearPendingRequests)
{
    activeBuildOrderIndex = -1;
    activeBuildOrderStep = 0;
    buildOrderActive = false;
    buildOrderCompleted = true;

    if (clearPendingRequests)
    {
        // Remove any pending build-order building requests that haven't started
        for (auto it = resourceRequests.begin(); it != resourceRequests.end();)
        {
            if (it->fromBuildOrder && it->state == ResourceRequest::State::PendingApproval)
                it = resourceRequests.erase(it);
            else
                ++it;
        }
    }
}

void BuildManager::overrideBuildOrder(int buildOrderId)
{
    // Current setup: clear current build order requests, then replace with another chosen build order ID
    for (auto it = resourceRequests.begin(); it != resourceRequests.end();)
    {
        if (it->fromBuildOrder && it->state == ResourceRequest::State::PendingApproval)
            it = resourceRequests.erase(it);
        else
            ++it;
    }

    for (int i = 0; i < (int)buildOrders.size(); i++)
    {
        if (buildOrders[i].id == buildOrderId)
        {
            activeBuildOrderIndex = i;
            activeBuildOrderStep = 0;
            buildOrderActive = true;
            buildOrderCompleted = false;
            //std::cout << "Overriding Build Order: " << buildOrderNameToString(buildOrders[i].name) << "\n";
            return;
        }
    }

    selectRandomBuildOrder();
}

bool BuildManager::enqueueBuildOrderBuilding(BWAPI::UnitType type, int count)
{
    for (int i = 0; i < count; i++)
    {
        ResourceRequest req;
        req.type = ResourceRequest::Type::Building;
        req.unit = type;
        req.fromBuildOrder = true;
        req.priority = 0;
        resourceRequests.push_back(req);
    }
    return true;
}

void BuildManager::runBuildOrderOnFrame()
{
    if (!isBuildOrderActive())
        return;

    BuildOrder& bo = buildOrders[activeBuildOrderIndex];
    const int supply = BWAPI::Broodwar->self()->supplyUsed() / 2;

    // Issue steps in order; at most 1 step per frame to avoid spikes
    if (activeBuildOrderStep >= bo.steps.size())
    {
        buildOrderCompleted = true;
        buildOrderActive = false;
        //std::cout << "Build Order Completed: " << buildOrderNameToString(bo.name) << "\n";
        return;
    }

    BuildOrderStep& step = bo.steps[activeBuildOrderStep];

    bool triggerMet = false;
    if (step.trigger.type == BuildTriggerType::Immediately) triggerMet = true;
    if (step.trigger.type == BuildTriggerType::AtSupply && supply >= step.trigger.value) triggerMet = true;

    if (!triggerMet)
        return;

    
bool issued = false;

switch (step.type)
{
    case BuildStepType::ScoutWorker:
        if (commanderReference) { commanderReference->getUnitToScout(); issued = true; }
        break;

    case BuildStepType::SupplyRampNatural:
        issued = enqueueSupplyAtNaturalRamp();
        break;

    case BuildStepType::NaturalWall:
        issued = enqueueNaturalWallAtChoke();
        break;

    case BuildStepType::Build:
    default:
        issued = enqueueBuildOrderBuilding(step.unit, step.count);
        break;
}

if (issued)
    activeBuildOrderStep++;

}

// BWEB placement helpers

BWAPI::TilePosition BuildManager::findNaturalRampPlacement(BWAPI::UnitType type) const
{
    const auto* choke = BWEB::Map::getNaturalChoke();
    if (!choke)
        return BWAPI::TilePositions::Invalid;

    // Anchor on the choke tile closest to our natural, then inch a few tiles toward the main
    const BWAPI::Position natPos = BWEB::Map::getNaturalPosition();
    const BWAPI::TilePosition chokeTile(BWEB::Map::getClosestChokeTile(choke, natPos));
    const BWAPI::TilePosition mainTile(BWEB::Map::getMainTile());

    const int dirX = (mainTile.x > chokeTile.x) ? 1 : (mainTile.x < chokeTile.x ? -1 : 0);
    const int dirY = (mainTile.y > chokeTile.y) ? 1 : (mainTile.y < chokeTile.y ? -1 : 0);

    const int w = type.tileWidth();
    const int h = type.tileHeight();

    auto inBounds = [&](const BWAPI::TilePosition& t) {
        return t.isValid()
            && t.x >= 0 && t.y >= 0
            && (t.x + w) <= BWAPI::Broodwar->mapWidth()
            && (t.y + h) <= BWAPI::Broodwar->mapHeight();
    };

    // Try a few anchors, from closest to choke to slightly deeper toward main in case closest anchor is unplaceable on some maps
    const BWAPI::TilePosition anchors[] = {
        chokeTile + BWAPI::TilePosition(dirX, dirY) * 1,
        chokeTile + BWAPI::TilePosition(dirX, dirY) * 2,
        chokeTile + BWAPI::TilePosition(dirX, dirY) * 3
    };

    const int maxR = 10;
    for (const auto& anchor : anchors)
    {
        for (int r = 0; r <= maxR; r++)
        {
            for (int dx = -r; dx <= r; dx++)
            {
                for (int dy = -r; dy <= r; dy++)
                {
                    if (std::abs(dx) != r && std::abs(dy) != r) continue;

                    const BWAPI::TilePosition t = anchor + BWAPI::TilePosition(dx, dy);
                    if (!inBounds(t)) continue;
                    if (!BWEB::Map::isPlaceable(type, t)) continue;
                    if (BWEB::Map::isReserved(t, w, h)) continue; // avoid reserved BWEB blocks
                    if (BWEB::Map::isUsed(t, w, h) != BWAPI::UnitTypes::None) continue;

                    return t;
                }
            }
        }
    }

    return BWAPI::TilePositions::Invalid;
}



void BuildManager::resetNaturalWallPlan()
{
    naturalWallPlanned = false;
    naturalWallPylonEnqueued = false;
    naturalWallGatewaysEnqueued = false;
    naturalWallStartLogged = false;
    naturalWallPylonTile = BWAPI::TilePositions::Invalid;
    naturalWallOpeningTile = BWAPI::TilePositions::Invalid;
    naturalWallChokeAnchorTile = BWAPI::TilePositions::Invalid;
    naturalWallGatewayTiles.clear();
    naturalWallCannonTiles.clear();
    naturalWallPathTiles.clear();
}

// Find a good pylon tile very near the natural choke for wall power.
BWAPI::TilePosition BuildManager::findNaturalChokePylonTile() const
{
    const auto* choke = BWEB::Map::getNaturalChoke();
    if (!choke) return BWAPI::TilePositions::Invalid;

    const BWAPI::Position mainPos = BWEB::Map::getMainPosition();
    const BWAPI::TilePosition anchor(BWEB::Map::getClosestChokeTile(choke, mainPos));
    const BWAPI::TilePosition centerTarget = naturalWallOpeningTile.isValid() ? naturalWallOpeningTile : anchor;

    const auto pylon = BWAPI::UnitTypes::Protoss_Pylon;
    const int w = pylon.tileWidth();
    const int h = pylon.tileHeight();

    auto inBounds = [&](const BWAPI::TilePosition& t) {
        return t.isValid()
            && t.x >= 0 && t.y >= 0
            && (t.x + w) <= BWAPI::Broodwar->mapWidth()
            && (t.y + h) <= BWAPI::Broodwar->mapHeight();
    };

    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
    int bestScore = INT_MAX;

    for (int r = 0; r <= 12; r++) {
        for (int dx = -r; dx <= r; dx++) {
            for (int dy = -r; dy <= r; dy++) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                BWAPI::TilePosition t = centerTarget + BWAPI::TilePosition(dx, dy);
                if (!inBounds(t)) continue;
                if (!BWAPI::Broodwar->canBuildHere(t, pylon)) continue;
                if (BWEB::Map::isUsed(t, w, h) != BWAPI::UnitTypes::None) continue;

                const int gapDist = footprintDistanceToTile(pylon, t, centerTarget);
                const int pathOverlap = footprintPathOverlapCount(pylon, t, naturalWallPathTiles);
                int score = gapDist * 1000 + pathOverlap * 10;
                if (gapDist > 1)
                    score += 100000;

                if (score < bestScore)
                {
                    bestScore = score;
                    best = t;
                }
            }
        }
    }

    return best;
}

bool BuildManager::enqueueSupplyAtNaturalRamp()
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Protoss)
        return false;

    const auto type = BWAPI::UnitTypes::Protoss_Pylon;

    const BWAPI::TilePosition t = findNaturalRampPlacement(type);
    if (!isValidBuildTile(type, t))
        return false;

    ResourceRequest req;
    req.type = ResourceRequest::Type::Building;
    req.unit = type;
    req.fromBuildOrder = true;
    req.priority = 0;

    req.useForcedTile = true;
    req.forcedTile = t;

    // Reserve immediately so other placement logic doesn't steal the spot
    BWEB::Map::addReserve(t, type.tileWidth(), type.tileHeight());

    resourceRequests.push_back(req);
    return true;
}


// Call to request walling outside the build order
bool BuildManager::requestNaturalWallBuild(bool resetPlan)
{
    if (resetPlan)
        resetNaturalWallPlan();
    return enqueueNaturalWallAtChoke();
}

bool BuildManager::enqueueNaturalWallAtChoke()
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Protoss)
        return false;

    const auto* choke = BWEB::Map::getNaturalChoke();
    const auto* area  = BWEB::Map::getNaturalArea();
    if (!choke || !area)
        return false;

    // If we haven't planned the wall layout yet, generate it once (and cache tiles)
    if (!naturalWallPlanned)
    {
        naturalWallChokeAnchorTile = BWAPI::TilePosition(BWEB::Map::getClosestChokeTile(choke, BWEB::Map::getMainPosition()));

        const Path chokePath = AStar::GeneratePath(BWEB::Map::getMainPosition(), BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(choke->Center()));
        const std::vector<BWAPI::TilePosition> pathTiles = uniquePathTiles(chokePath);
        naturalWallPathTiles = pathTiles;

        bool hasCompletedForge = false;
        for (auto u : BWAPI::Broodwar->self()->getUnits())
        {
            if (u->getType() == BWAPI::UnitTypes::Protoss_Forge && u->isCompleted())
            {
                hasCompletedForge = true;
                break;
            }
        }
        // Could be formatted better, list possible permutations of gateways and pylons wall layout, with or w/o cannons
        std::vector<std::vector<BWAPI::UnitType>> candidates = {
            // 2 Gate + 1 Pylon wall options
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon },
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway },
            { BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway },
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon },

            // Custom 1 Pylon + 3 Gateway wall options
            { BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway },
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway },
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway },
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon }
        };

        if (hasCompletedForge)
        {
            std::vector<std::vector<BWAPI::UnitType>> cannonCandidates = {
                { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Photon_Cannon },
                { BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Pylon },
                { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Gateway },
                { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Photon_Cannon },
                { BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway },
                { BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon },
                { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Photon_Cannon },
                { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Pylon },
                { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway },
                { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Gateway },
                { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Photon_Cannon },
                { BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway },
                { BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway },
                { BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon },
                { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Gateway },
                { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Photon_Cannon },
                { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway },
                { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon },
                { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Photon_Cannon },
                { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Pylon }
            };
            candidates.insert(candidates.end(), cannonCandidates.begin(), cannonCandidates.end());
        }
		// Score above candidates based on multiple factors: path overlap (avoid blocking A* path), distance of wall pieces to choke, distance of wall opening to choke
        std::vector<BWAPI::UnitType> bestCandidate;
        int bestScore = INT_MAX;
        const std::vector<BWAPI::UnitType> emptyBuildings;

        for (std::vector<std::vector<BWAPI::UnitType>>::iterator it = candidates.begin(); it != candidates.end(); ++it)
        {
            std::vector<BWAPI::UnitType> mutableCandidate = *it;
            BWEB::Wall* testWall = BWEB::Walls::createWall(mutableCandidate, area, choke, BWAPI::UnitTypes::None, emptyBuildings, true, false);
            if (!testWall)
                continue;

            int overlapPenalty = 0;
            for (const auto& t : testWall->getMediumTiles())
                overlapPenalty += footprintPathOverlapCount(BWAPI::UnitTypes::Protoss_Gateway, t, pathTiles);
            for (const auto& t : testWall->getLargeTiles())
                overlapPenalty += footprintPathOverlapCount(BWAPI::UnitTypes::Protoss_Gateway, t, pathTiles);
            for (const auto& t : testWall->getSmallTiles())
                overlapPenalty += footprintPathOverlapCount(BWAPI::UnitTypes::Protoss_Pylon, t, pathTiles);

            const BWAPI::TilePosition opening = testWall->getOpening();
            const int openingDist = minTileDistanceToPath(opening, pathTiles);
            const int wallAnchorDist = wallDistanceToAnchor(testWall, naturalWallChokeAnchorTile);
            const int openingAnchorDist = naturalWallChokeAnchorTile.isValid() && opening.isValid()
                ? std::abs(opening.x - naturalWallChokeAnchorTile.x) + std::abs(opening.y - naturalWallChokeAnchorTile.y)
                : INT_MAX / 4;

            // First anchor the wall on the choke, then keep the A* path as the gap (as much as possible)
            const int score = (wallAnchorDist * 100000)
                + (openingAnchorDist * 5000)
                + (overlapPenalty * 1000)
                + (openingDist * 100);
            if (score < bestScore)
            {
                bestScore = score;
                bestCandidate = mutableCandidate;
            }
        }

        if (bestCandidate.empty())
            return false;

        std::vector<BWAPI::UnitType> mutableBestCandidate = bestCandidate;
        BWEB::Wall* wall = BWEB::Walls::createWall(mutableBestCandidate, area, choke, BWAPI::UnitTypes::None, emptyBuildings, true, false);
        if (!wall)
            return false;

        naturalWallOpeningTile = wall->getOpening();
        naturalWallGatewayTiles.clear();
        naturalWallCannonTiles.clear();

        // pick the pylon tile closest to the A* gap, treat remaining small tiles as cannons.
        if (!wall->getSmallTiles().empty())
        {
            std::vector<BWAPI::TilePosition> small(wall->getSmallTiles().begin(), wall->getSmallTiles().end());

            BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
            int bestPylonScore = INT_MAX;

            for (const auto& t : small)
            {
                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                    continue;

                const int gapDist = naturalWallOpeningTile.isValid()
                    ? footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Pylon, t, naturalWallOpeningTile)
                    : INT_MAX / 4;
                const int pathOverlap = footprintPathOverlapCount(BWAPI::UnitTypes::Protoss_Pylon, t, pathTiles);

                int score = (gapDist * 1000) + pathOverlap;
                if (gapDist > 1)
                    score += 100000;

                if (score < bestPylonScore)
                {
                    bestPylonScore = score;
                    best = t;
                }
            }

            naturalWallPylonTile = best.isValid() ? best : BWAPI::TilePositions::Invalid;

            int cannonCount = 0;
            bool hasCompletedForgeForCannons = false;
            for (auto u : BWAPI::Broodwar->self()->getUnits())
            {
                if (u->getType() == BWAPI::UnitTypes::Protoss_Forge && u->isCompleted())
                {
                    hasCompletedForgeForCannons = true;
                    break;
                }
            }
            if (hasCompletedForgeForCannons)
            {
                for (std::vector<BWAPI::UnitType>::const_iterator it = mutableBestCandidate.begin(); it != mutableBestCandidate.end(); ++it)
                {
                    if (*it == BWAPI::UnitTypes::Protoss_Photon_Cannon)
                        cannonCount++;
                }
            }

            if (cannonCount > 0)
            {
                std::sort(small.begin(), small.end(), [&](const BWAPI::TilePosition& a, const BWAPI::TilePosition& b) {
                    const int da = naturalWallOpeningTile.isValid() ? footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Photon_Cannon, a, naturalWallOpeningTile) : 0;
                    const int db = naturalWallOpeningTile.isValid() ? footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Photon_Cannon, b, naturalWallOpeningTile) : 0;
                    return da < db;
                });

                for (const auto& t : small)
                {
                    if (t == naturalWallPylonTile)
                        continue;
                    if ((int)naturalWallCannonTiles.size() >= cannonCount)
                        break;
                    naturalWallCannonTiles.push_back(t);
                }
            }
        }

        // Get gateway tiles and prioritize the pieces that stay off the A* path
        for (std::set<BWAPI::TilePosition>::iterator it = wall->getMediumTiles().begin(); it != wall->getMediumTiles().end(); ++it)
            naturalWallGatewayTiles.push_back(*it);
        for (std::set<BWAPI::TilePosition>::iterator it = wall->getLargeTiles().begin(); it != wall->getLargeTiles().end(); ++it)
            naturalWallGatewayTiles.push_back(*it);

        std::sort(naturalWallGatewayTiles.begin(), naturalWallGatewayTiles.end(), [&](const BWAPI::TilePosition& a, const BWAPI::TilePosition& b) {
            const int overlapA = footprintPathOverlapCount(BWAPI::UnitTypes::Protoss_Gateway, a, pathTiles);
            const int overlapB = footprintPathOverlapCount(BWAPI::UnitTypes::Protoss_Gateway, b, pathTiles);
            if (overlapA != overlapB)
                return overlapA < overlapB;

            const int anchorA = naturalWallChokeAnchorTile.isValid() ? footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Gateway, a, naturalWallChokeAnchorTile) : 0;
            const int anchorB = naturalWallChokeAnchorTile.isValid() ? footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Gateway, b, naturalWallChokeAnchorTile) : 0;
            if (anchorA != anchorB)
                return anchorA < anchorB;

            const int distA = naturalWallOpeningTile.isValid() ? footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Gateway, a, naturalWallOpeningTile) : 0;
            const int distB = naturalWallOpeningTile.isValid() ? footprintDistanceToTile(BWAPI::UnitTypes::Protoss_Gateway, b, naturalWallOpeningTile) : 0;
            return distA < distB;
        });

        naturalWallPlanned = true;
        naturalWallPylonEnqueued = false;
        naturalWallGatewaysEnqueued = false;
        naturalWallStartLogged = false;
    }

    if (!naturalWallPylonEnqueued)
    {
        // If BWEB didn't provide a good pylon tile, fall back to searching near choke
        BWAPI::TilePosition pylonTile = naturalWallPylonTile.isValid() ? naturalWallPylonTile : findNaturalChokePylonTile();
        if (!pylonTile.isValid() || !isValidBuildTile(BWAPI::UnitTypes::Protoss_Pylon, pylonTile))
            return false;

        ResourceRequest req;
        req.type = ResourceRequest::Type::Building;
        req.unit = BWAPI::UnitTypes::Protoss_Pylon;
        req.fromBuildOrder = true;
        req.useForcedTile = true;
        req.forcedTile = pylonTile;
        req.priority = 0;

        // Then reserve
        BWEB::Map::addReserve(pylonTile, req.unit.tileWidth(), req.unit.tileHeight());

        resourceRequests.push_back(req);
        naturalWallPylonEnqueued = true;
        naturalWallPylonTile = pylonTile;

        if (!naturalWallStartLogged)
        {
            const int supply = BWAPI::Broodwar->self()->supplyUsed() / 2;
            std::cout << "[WallDebug] Starting wall construction at supply " << supply
                      << " | pylon tile (" << pylonTile.x << "," << pylonTile.y << ")";
            if (naturalWallOpeningTile.isValid())
                std::cout << " | gap tile (" << naturalWallOpeningTile.x << "," << naturalWallOpeningTile.y << ")";
            std::cout << std::endl;

            BWAPI::Broodwar->printf("[WallDebug] Starting wall construction at supply %d", supply);
            naturalWallStartLogged = true;
        }

        return false;
    }

    // Wait for the pylon to be completed so gateways can be powered
    bool pylonComplete = false;
    for (auto u : BWAPI::Broodwar->self()->getUnits())
    {
        if (u->getType() == BWAPI::UnitTypes::Protoss_Pylon && u->isCompleted())
        {
            if (BWAPI::TilePosition(u->getPosition()).getDistance(naturalWallPylonTile) <= 3)
            {
                pylonComplete = true;
                break;
            }
        }
    }
    if (!pylonComplete)
        return false;

    if (!naturalWallGatewaysEnqueued)
    {
        bool enqueuedAny = false;

        auto enqueueForced = [&](BWAPI::UnitType ut, const BWAPI::TilePosition& t)
        {
            if (!t.isValid()) return;

            // Check if reachable
            if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                return;
            if (!isTerrainBuildable(ut, t))
                return;

            if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss && ut.requiresPsi())
            {
                // nudge placement around the choke a bit if invalid or unpowered
                if (!BWAPI::Broodwar->hasPower(t, ut))
                {
                    bool found = false;
                    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                    int bestD = INT_MAX;

                    for (int r = 1; r <= 5 && !found; r++)
                    {
                        for (int dx = -r; dx <= r; dx++)
                        {
                            for (int dy = -r; dy <= r; dy++)
                            {
                                if (std::abs(dx) != r && std::abs(dy) != r) continue;

                                BWAPI::TilePosition tt = t + BWAPI::TilePosition(dx, dy);
                                if (!tt.isValid()) continue;

                                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(tt) + BWAPI::Position(16, 16)))
                                    continue;

                                if (!isTerrainBuildable(ut, tt))
                                    continue;

                                if (!BWAPI::Broodwar->hasPower(tt, ut))
                                    continue;

                                int d = std::abs(tt.x - naturalWallPylonTile.x) + std::abs(tt.y - naturalWallPylonTile.y);
                                if (d < bestD)
                                {
                                    bestD = d;
                                    best = tt;
                                }
                                found = true;
                            }
                        }
                    }

                    if (best.isValid())
                    {
                        ResourceRequest req;
                        req.type = ResourceRequest::Type::Building;
                        req.unit = ut;
                        req.fromBuildOrder = true;
                        req.useForcedTile = true;
                        req.forcedTile = best;
                        req.priority = 0;

                        resourceRequests.push_back(req);
                        enqueuedAny = true;
                    }
                    return;
                }
            }

            ResourceRequest req;
            req.type = ResourceRequest::Type::Building;
            req.unit = ut;
            req.fromBuildOrder = true;
            req.useForcedTile = true;
            req.forcedTile = t;
            req.priority = 0;

            resourceRequests.push_back(req);
            enqueuedAny = true;
        };

        bool hasCompletedForgeForCannons = false;
        for (auto u : BWAPI::Broodwar->self()->getUnits())
        {
            if (u->getType() == BWAPI::UnitTypes::Protoss_Forge && u->isCompleted())
            {
                hasCompletedForgeForCannons = true;
                break;
            }
        }
        if (hasCompletedForgeForCannons)
        {
            for (const auto& t : naturalWallCannonTiles)
                enqueueForced(BWAPI::UnitTypes::Protoss_Photon_Cannon, t);
        }

        for (const auto& t : naturalWallGatewayTiles)
            enqueueForced(BWAPI::UnitTypes::Protoss_Gateway, t);

        if (!enqueuedAny)
            return false;

        naturalWallGatewaysEnqueued = true;
        return true;
    }

    return true;
}