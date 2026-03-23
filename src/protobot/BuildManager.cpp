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

static BWAPI::TilePosition chooseWallGapOnPath(BWEB::Wall* wall, const std::vector<BWAPI::TilePosition>& pathTiles)
{
    if (!wall)
        return BWAPI::TilePositions::Invalid;

    const BWAPI::TilePosition opening = wall->getOpening();
    const BWAPI::TilePosition centroid = BWAPI::TilePosition(wall->getCentroid());

    if (pathTiles.empty())
        return opening.isValid() ? opening : centroid;

    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
    int bestScore = INT_MAX;

    auto consider = [&](const BWAPI::TilePosition& candidate) {
        if (!candidate.isValid())
            return;

        bool blocked = false;
        for (const auto& t : wall->getSmallTiles()) {
            if (footprintContainsTile(BWAPI::UnitTypes::Protoss_Pylon, t, candidate)) {
                blocked = true;
                break;
            }
        }
        if (!blocked) {
            for (const auto& t : wall->getMediumTiles()) {
                if (footprintContainsTile(BWAPI::UnitTypes::Protoss_Gateway, t, candidate)) {
                    blocked = true;
                    break;
                }
            }
        }
        if (!blocked) {
            for (const auto& t : wall->getLargeTiles()) {
                if (footprintContainsTile(BWAPI::UnitTypes::Protoss_Gateway, t, candidate)) {
                    blocked = true;
                    break;
                }
            }
        }
        if (blocked)
            return;

        int score = 0;
        if (opening.isValid())
            score += 10 * (std::abs(candidate.x - opening.x) + std::abs(candidate.y - opening.y));
        if (centroid.isValid())
            score += std::abs(candidate.x - centroid.x) + std::abs(candidate.y - centroid.y);

        if (score < bestScore)
        {
            bestScore = score;
            best = candidate;
        }
    };

    for (const auto& pt : pathTiles)
    {
        const int openingDist = opening.isValid() ? (std::abs(pt.x - opening.x) + std::abs(pt.y - opening.y)) : 0;
        const int centroidDist = centroid.isValid() ? (std::abs(pt.x - centroid.x) + std::abs(pt.y - centroid.y)) : 0;
        if (openingDist <= 3 || centroidDist <= 2)
            consider(pt);
    }

    if (best.isValid())
        return best;

    for (const auto& pt : pathTiles)
        consider(pt);

    return best.isValid() ? best : (opening.isValid() ? opening : centroid);
}

static bool footprintOverlapsProtectedTiles(BWAPI::UnitType type, const BWAPI::TilePosition& tile, const std::vector<BWAPI::TilePosition>& protectedTiles)
{
    for (const auto& pt : protectedTiles)
    {
        if (footprintContainsTile(type, tile, pt))
            return true;
    }
    return false;
}

static int footprintMinDistanceToProtectedTiles(BWAPI::UnitType type, const BWAPI::TilePosition& tile, const std::vector<BWAPI::TilePosition>& protectedTiles)
{
    if (!tile.isValid() || protectedTiles.empty())
        return INT_MAX / 4;

    int best = INT_MAX / 4;
    for (int dx = 0; dx < type.tileWidth(); ++dx)
    {
        for (int dy = 0; dy < type.tileHeight(); ++dy)
        {
            const BWAPI::TilePosition ft(tile.x + dx, tile.y + dy);
            for (const auto& pt : protectedTiles)
                best = std::min(best, std::abs(ft.x - pt.x) + std::abs(ft.y - pt.y));
        }
    }
    return best;
}

static std::vector<BWAPI::TilePosition> chooseProtectedGapCorridor(const BWAPI::TilePosition& openingTile, const std::vector<BWAPI::TilePosition>& pathTiles)
{
    std::vector<BWAPI::TilePosition> result;
    if (!openingTile.isValid())
        return result;

    const auto addUnique = [&](const BWAPI::TilePosition& tile) {
        if (!tile.isValid())
            return;
        if (tile.x < 0 || tile.y < 0 || tile.x >= BWAPI::Broodwar->mapWidth() || tile.y >= BWAPI::Broodwar->mapHeight())
            return;
        if (std::find(result.begin(), result.end(), tile) == result.end())
            result.push_back(tile);
    };

    // Attempt to keep the wall gap one tile wide for units to pass through
    addUnique(openingTile);

    if (pathTiles.empty())
        return result;

    int bestIndex = -1;
    int bestDistance = INT_MAX;
    for (int i = 0; i < (int)pathTiles.size(); ++i)
    {
        const auto& pt = pathTiles[i];
        if (!pt.isValid())
            continue;

        const int d = std::abs(pt.x - openingTile.x) + std::abs(pt.y - openingTile.y);
        if (d < bestDistance)
        {
            bestDistance = d;
            bestIndex = i;
        }
    }

    if (bestIndex < 0)
        return result;

    const int corridorHalfLength = 4;
    for (int i = std::max(0, bestIndex - corridorHalfLength); i <= std::min((int)pathTiles.size() - 1, bestIndex + corridorHalfLength); ++i)
    {
        const auto& pt = pathTiles[i];
        if (!pt.isValid())
            continue;
        addUnique(pt);
    }

    return result;
}

static bool overlapsExistingWallPiece(BWAPI::UnitType type, const BWAPI::TilePosition& tile, const BWAPI::TilePosition& pylonTile, const std::vector<BWAPI::TilePosition>& forgeTiles, const std::vector<BWAPI::TilePosition>& gatewayTiles)
{
    for (int dx = 0; dx < type.tileWidth(); ++dx)
    {
        for (int dy = 0; dy < type.tileHeight(); ++dy)
        {
            const BWAPI::TilePosition ft(tile.x + dx, tile.y + dy);
            if (pylonTile.isValid() && footprintContainsTile(BWAPI::UnitTypes::Protoss_Pylon, pylonTile, ft))
                return true;
            for (const auto& forgeTile : forgeTiles)
                if (footprintContainsTile(BWAPI::UnitTypes::Protoss_Forge, forgeTile, ft))
                    return true;
            for (const auto& gatewayTile : gatewayTiles)
                if (footprintContainsTile(BWAPI::UnitTypes::Protoss_Gateway, gatewayTile, ft))
                    return true;
        }
    }
    return false;
}

static BWAPI::TilePosition chooseCompactWallForgeTile(const BWAPI::TilePosition& pylonTile, const std::vector<BWAPI::TilePosition>& gatewayTiles, const BWAPI::TilePosition& openingTile, const std::vector<BWAPI::TilePosition>& protectedTiles)
{
    const BWAPI::UnitType forge = BWAPI::UnitTypes::Protoss_Forge;
    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
    int bestScore = INT_MAX;

    BWAPI::TilePosition anchor = pylonTile;
    if (!anchor.isValid() && !gatewayTiles.empty())
        anchor = gatewayTiles.front();
    if (!anchor.isValid())
        return best;

    for (int r = 0; r <= 8; ++r)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            for (int dy = -r; dy <= r; ++dy)
            {
                if (r > 0 && std::abs(dx) != r && std::abs(dy) != r)
                    continue;
                BWAPI::TilePosition t(anchor.x + dx, anchor.y + dy);
                if (!isTerrainBuildable(forge, t))
                    continue;
                if (overlapsExistingWallPiece(forge, t, pylonTile, std::vector<BWAPI::TilePosition>(), gatewayTiles))
                    continue;
                if (footprintOverlapsProtectedTiles(forge, t, protectedTiles))
                    continue;
                if (footprintMinDistanceToProtectedTiles(forge, t, protectedTiles) <= 1)
                    continue;
                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                    continue;

                int score = 0;
                if (pylonTile.isValid())
                    score += 120 * (std::abs(t.x - pylonTile.x) + std::abs(t.y - pylonTile.y));
                if (!gatewayTiles.empty())
                    score += 35 * footprintDistanceToTile(forge, t, gatewayTiles.front());
                if (openingTile.isValid())
                    score += 8 * footprintDistanceToTile(forge, t, openingTile);
                if (score < bestScore)
                {
                    bestScore = score;
                    best = t;
                }
            }
        }
        if (best.isValid())
            break;
    }

    return best;
}

static BWAPI::TilePosition chooseCompactWallGatewayTile(const BWAPI::TilePosition& pylonTile, const std::vector<BWAPI::TilePosition>& forgeTiles, const BWAPI::TilePosition& existingGatewayAnchor, const BWAPI::TilePosition& openingTile, const std::vector<BWAPI::TilePosition>& protectedTiles)
{
    const BWAPI::UnitType gateway = BWAPI::UnitTypes::Protoss_Gateway;
    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
    int bestScore = INT_MAX;

    BWAPI::TilePosition anchor = pylonTile;
    if (!anchor.isValid() && !forgeTiles.empty())
        anchor = forgeTiles.front();
    if (!anchor.isValid() && existingGatewayAnchor.isValid())
        anchor = existingGatewayAnchor;
    if (!anchor.isValid())
        return best;

    std::vector<BWAPI::TilePosition> noGateways;
    for (int r = 0; r <= 10; ++r)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            for (int dy = -r; dy <= r; ++dy)
            {
                if (r > 0 && std::abs(dx) != r && std::abs(dy) != r)
                    continue;
                BWAPI::TilePosition t(anchor.x + dx, anchor.y + dy);
                if (!isTerrainBuildable(gateway, t))
                    continue;
                if (overlapsExistingWallPiece(gateway, t, pylonTile, forgeTiles, noGateways))
                    continue;
                if (footprintOverlapsProtectedTiles(gateway, t, protectedTiles))
                    continue;
                if (footprintMinDistanceToProtectedTiles(gateway, t, protectedTiles) <= 1)
                    continue;
                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                    continue;

                int score = 0;
                if (pylonTile.isValid())
                    score += 150 * footprintDistanceToTile(gateway, t, pylonTile);
                if (!forgeTiles.empty())
                    score += 60 * footprintDistanceToTile(gateway, t, forgeTiles.front());
                if (openingTile.isValid())
                    score += 12 * footprintDistanceToTile(gateway, t, openingTile);
                if (score < bestScore)
                {
                    bestScore = score;
                    best = t;
                }
            }
        }
        if (best.isValid())
            break;
    }

    return best;
}

static BWAPI::TilePosition chooseCompactWallCannonTile(const BWAPI::TilePosition& pylonTile, const std::vector<BWAPI::TilePosition>& forgeTiles, const std::vector<BWAPI::TilePosition>& gatewayTiles, const BWAPI::TilePosition& openingTile, const std::vector<BWAPI::TilePosition>& protectedTiles)
{
    const BWAPI::UnitType cannon = BWAPI::UnitTypes::Protoss_Photon_Cannon;
    BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
    int bestScore = INT_MAX;

    BWAPI::TilePosition anchor = pylonTile;
    if (!anchor.isValid() && !forgeTiles.empty())
        anchor = forgeTiles.front();
    if (!anchor.isValid() && !gatewayTiles.empty())
        anchor = gatewayTiles.front();
    if (!anchor.isValid())
        return best;

    for (int r = 0; r <= 8; ++r)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            for (int dy = -r; dy <= r; ++dy)
            {
                if (r > 0 && std::abs(dx) != r && std::abs(dy) != r)
                    continue;
                BWAPI::TilePosition t(anchor.x + dx, anchor.y + dy);
                if (!isTerrainBuildable(cannon, t))
                    continue;
                if (overlapsExistingWallPiece(cannon, t, pylonTile, forgeTiles, gatewayTiles))
                    continue;
                if (footprintOverlapsProtectedTiles(cannon, t, protectedTiles))
                    continue;
                if (footprintOverlapsProtectedTiles(cannon, t, protectedTiles))
                    continue;
                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                    continue;

                int score = 0;
                if (pylonTile.isValid())
                    score += 100 * (std::abs(t.x - pylonTile.x) + std::abs(t.y - pylonTile.y));
                if (!forgeTiles.empty())
                    score += 60 * footprintDistanceToTile(cannon, t, forgeTiles.front());
                if (!gatewayTiles.empty())
                    score += 20 * footprintDistanceToTile(cannon, t, gatewayTiles.front());
                if (openingTile.isValid())
                    score += 5 * footprintDistanceToTile(cannon, t, openingTile);
                if (score < bestScore)
                {
                    bestScore = score;
                    best = t;
                }
            }
        }
        if (best.isValid())
            break;
    }
    return best;
}


static std::vector<BWAPI::TilePosition> chooseCompactWallCannonArcTiles(int count, const BWAPI::TilePosition& pylonTile, const std::vector<BWAPI::TilePosition>& forgeTiles, const std::vector<BWAPI::TilePosition>& gatewayTiles, const BWAPI::TilePosition& openingTile, const std::vector<BWAPI::TilePosition>& protectedTiles)
{
    std::vector<BWAPI::TilePosition> result;
    if (count <= 0)
        return result;

    const BWAPI::UnitType cannon = BWAPI::UnitTypes::Protoss_Photon_Cannon;
    BWAPI::TilePosition anchor = !forgeTiles.empty() ? forgeTiles.front() : (pylonTile.isValid() ? pylonTile : (!gatewayTiles.empty() ? gatewayTiles.front() : BWAPI::TilePositions::Invalid));
    if (!anchor.isValid())
        return result;

    int fx = 1, fy = 0;
    if (openingTile.isValid())
    {
        fx = openingTile.x - anchor.x;
        fy = openingTile.y - anchor.y;
        if (fx == 0 && fy == 0)
            fx = 1;
    }

    std::vector<std::pair<int, BWAPI::TilePosition>> scored;
    for (int r = 0; r <= 8; ++r)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            for (int dy = -r; dy <= r; ++dy)
            {
                if (r > 0 && std::abs(dx) != r && std::abs(dy) != r)
                    continue;
                BWAPI::TilePosition t(anchor.x + dx, anchor.y + dy);
                if (!isTerrainBuildable(cannon, t))
                    continue;
                if (overlapsExistingWallPiece(cannon, t, pylonTile, forgeTiles, gatewayTiles))
                    continue;
                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                    continue;

                int score = 0;
                if (pylonTile.isValid())
                    score += 120 * footprintDistanceToTile(cannon, t, pylonTile);
                if (!forgeTiles.empty())
                    score += 80 * footprintDistanceToTile(cannon, t, forgeTiles.front());
                if (!gatewayTiles.empty())
                    score += 30 * footprintDistanceToTile(cannon, t, gatewayTiles.front());
                if (openingTile.isValid())
                    score += 15 * footprintDistanceToTile(cannon, t, openingTile);

                const int vx = t.x - anchor.x;
                const int vy = t.y - anchor.y;
                const int dot = vx * fx + vy * fy;
                score -= dot * 20; // prefer choke-facing side

                scored.push_back({ score, t });
            }
        }
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& entry : scored)
    {
        const auto& t = entry.second;
        bool tooClose = false;
        for (const auto& existing : result)
        {
            if (std::abs(existing.x - t.x) <= 1 && std::abs(existing.y - t.y) <= 1)
            {
                tooClose = true;
                break;
            }
        }
        if (tooClose)
            continue;
        result.push_back(t);
        if ((int)result.size() >= count)
            break;
    }

    return result;
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

    buildingPlacer.onStart();
    builders.clear();

    initBuildOrdersOnStart();
    selectRandomBuildOrder();
}


void BuildManager::onFrame(std::vector<ResourceRequest>& resourceRequests) 
{
    runBuildOrderOnFrame();

    if (naturalWallPlanned && (!naturalWallPylonEnqueued || !naturalWallGatewaysEnqueued || !naturalWallCannonsEnqueued))
        enqueueNaturalWallAtChoke();

    buildingPlacer.drawPoweredTiles();

    //Have to have loop that can check if a building can create a unit and is powered.
    //Might also need to have some data that shows what each building is doing but might be too complictaed for this approach.
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
                                //should create a new pylon request by inserting a pylon in the front of the queue.
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

    pumpUnit();

    //Debug
    //Will most likely need to add a building data class to make this easier to be able to keep track of buildings and what units they are creating.
    /*for (BWAPI::Unit building : buildings)
    {
        BWAPI::Broodwar->drawTextMap(building->getPosition(), std::to_string(building->getID()).c_str());
    }*/
}

void BuildManager::pumpUnit()
{
    const FriendlyUnitCounter ProtoBot_Units = InformationManager::Instance().getFriendlyUnitCounter();
    const FriendlyBuildingCounter ProtoBot_Buildings = InformationManager::Instance().getFriendlyBuildingCounter();
    const FriendlyUpgradeCounter ProtoBot_Upgrades = InformationManager::Instance().getFriendlyUpgradeCounter();
    const int totalMinerals = BWAPI::Broodwar->self()->minerals();
    const int totalGas = BWAPI::Broodwar->self()->gas();

    for (BWAPI::Unit unit : buildings)
    {
        const BWAPI::UnitType type = unit->getType();
        if (type == BWAPI::UnitTypes::Protoss_Gateway && !unit->isTraining() && !commanderReference->alreadySentRequest(unit->getID()))
        {
            if (ProtoBot_Buildings.cyberneticsCore >= 1 && ProtoBot_Units.zealot >= 7)
            {
                commanderReference->requestUnit(BWAPI::UnitTypes::Protoss_Dragoon, unit);
            }
            else
            {
                commanderReference->requestUnit(BWAPI::UnitTypes::Protoss_Zealot, unit);
            }
        }
        else if (unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility && !unit->isTraining() && !commanderReference->alreadySentRequest(unit->getID()) && unit->canTrain(BWAPI::UnitTypes::Protoss_Observer))
        {
            if (ProtoBot_Units.observer < 4)
            {
                commanderReference->requestUnit(BWAPI::UnitTypes::Protoss_Observer, unit);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Cybernetics_Core && !unit->isUpgrading())
        {
            if (unit->canUpgrade(BWAPI::UpgradeTypes::Singularity_Charge) && !commanderReference->upgradeAlreadyRequested(unit) && ProtoBot_Units.dragoon >= 1)
            {
                commanderReference->requestUpgrade(unit, BWAPI::UpgradeTypes::Singularity_Charge);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun && !unit->isUpgrading())
        {
            if (ProtoBot_Buildings.gateway < 8 || ProtoBot_Buildings.templarArchives < 1) continue;

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Leg_Enhancements) && !commanderReference->upgradeAlreadyRequested(unit))
            {
                commanderReference->requestUpgrade(unit, BWAPI::UpgradeTypes::Leg_Enhancements);
            }
        }
        else if (type == BWAPI::UnitTypes::Protoss_Forge && !unit->isUpgrading())
        {
            if (ProtoBot_Buildings.gateway < 8) continue;

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) && !commanderReference->upgradeAlreadyRequested(unit))
            {
                commanderReference->requestUpgrade(unit, BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
            }

            if (ProtoBot_Upgrades.singularityCharge != 1) continue;

            if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && !commanderReference->upgradeAlreadyRequested(unit))
            {
                commanderReference->requestUpgrade(unit, BWAPI::UpgradeTypes::Protoss_Ground_Armor);
            }
            else if (unit->canUpgrade(BWAPI::UpgradeTypes::Protoss_Plasma_Shields) && !commanderReference->upgradeAlreadyRequested(unit))
            {
                commanderReference->requestUpgrade(unit, BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
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

void BuildManager::onUnitCreate(BWAPI::Unit unit)
{
    buildingPlacer.onUnitCreate(unit);

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
bool BuildManager::checkWorkerIsConstructing(BWAPI::Unit unit)
{
    for (Builder& builder : builders)
    {
        if (builder.getUnitReference()->getID() == unit->getID()) return true;
    }

    return false;
}

BWAPI::Unitset BuildManager::getBuildings()
{
    return buildings;
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

    /*if (clearPendingRequests)
    {
        // Remove any pending build-order building requests that haven't started
        for (auto it = resourceRequests.begin(); it != resourceRequests.end();)
        {
            if (it->fromBuildOrder && it->state == ResourceRequest::State::PendingApproval)
                it = resourceRequests.erase(it);
            else
                ++it;
        }
    }*/
}

void BuildManager::overrideBuildOrder(int buildOrderId)
{
    // Current setup: clear current build order requests, then replace with another chosen build order ID
    /*for (auto it = resourceRequests.begin(); it != resourceRequests.end();)
    {
        if (it->fromBuildOrder && it->state == ResourceRequest::State::PendingApproval)
            it = resourceRequests.erase(it);
        else
            ++it;
    }*/

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
        /*ResourceRequest req;
        req.type = ResourceRequest::Type::Building;
        req.unit = type;
        req.fromBuildOrder = true;
        resourceRequests.push_back(req);*/

        commanderReference->requestBuilding(type, true);
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
    naturalWallCannonsEnqueued = false;
    naturalWallStartLogged = false;
    naturalWallPylonTile = BWAPI::TilePositions::Invalid;
    naturalWallPylonTiles.clear();
    naturalWallOpeningTile = BWAPI::TilePositions::Invalid;
    naturalWallChokeAnchorTile = BWAPI::TilePositions::Invalid;
    naturalWallForgeTiles.clear();
    naturalWallGatewayTiles.clear();
    naturalWallCannonTiles.clear();
    naturalWallPathTiles.clear();
    naturalWallGapTiles.clear();
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

    /*ResourceRequest req;
    req.type = ResourceRequest::Type::Building;
    req.unit = type;
    req.fromBuildOrder = true;
    req.useForcedTile = true;
    req.forcedTile = t;*/

    // Reserve immediately so other placement logic doesn't steal the spot
    BWEB::Map::addReserve(t, type.tileWidth(), type.tileHeight());

    commanderReference->requestBuilding(type, true, true, t);
    return true;
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
        std::vector<std::vector<BWAPI::UnitType>> candidates = {
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon },
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway },
            { BWAPI::UnitTypes::Protoss_Pylon,   BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Gateway },
            // fallback smaller wall : 1 gate + pylon
            { BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Pylon }
        };

        BWEB::Wall* wall = nullptr;
        for (auto& b : candidates)
        {
            wall = BWEB::Walls::createWall(b, area, choke, BWAPI::UnitTypes::None, {}, true, false);
            if (wall) break;
        }

        if (!wall)
            return false;

        // Choose pylon tile (small tile closest to choke center)
        if (!wall->getSmallTiles().empty())
        {
            const auto& small = wall->getSmallTiles();
            if (small.empty()) return false;

            const BWAPI::TilePosition chokeTile(BWAPI::TilePosition(choke->Center()));
            const BWEM::Area* mainArea = BWEB::Map::getMainArea();

            auto score = [&](const BWAPI::TilePosition& t) {
                return std::abs(t.x - chokeTile.x) + std::abs(t.y - chokeTile.y);
            };

            BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
            int bestDist = INT_MAX;

            for (const auto& t : small)
            {
                if (mainArea && BWEB::Map::mapBWEM.GetArea(t) != mainArea)
                    continue;
                if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                    continue;

                const int d = score(t);
                if (d > 4)
                    continue;

                if (d < bestDist)
                {
                    bestDist = d;
                    best = t;
                }
            }

            if (!best.isValid())
            {
                for (const auto& t : small)
                {
                    if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                        continue;
                    const int d = score(t);
                    if (d > 4) continue;
                    if (d < bestDist)
                    {
                        bestDist = d;
                        best = t;
                    }
                }
            }

            if (!best.isValid())
            {
                bestDist = INT_MAX;
                for (const auto& t : small)
                {
                    if (!BWAPI::Broodwar->hasPath(BWEB::Map::getMainPosition(), BWAPI::Position(t) + BWAPI::Position(16, 16)))
                        continue;
                    const int d = score(t);
                    if (d < bestDist)
                    {
                        bestDist = d;
                        best = t;
                    }
                }
            }

            // If still invalid, we will fall back to findNaturalChokePylonTile() later
            naturalWallPylonTile = best.isValid() ? best : BWAPI::TilePositions::Invalid;
        }

        // Get gateway tiles
        naturalWallGatewayTiles.clear();
        for (const auto& t : wall->getMediumTiles())
            naturalWallGatewayTiles.push_back(t);
        for (const auto& t : wall->getLargeTiles())
            naturalWallGatewayTiles.push_back(t);

        naturalWallPlanned = true;
        naturalWallPylonEnqueued = false;
        naturalWallGatewaysEnqueued = false;
    }

    if (!naturalWallPylonEnqueued)
    {
        // If BWEB didn't provide a good pylon tile, fall back to searching near choke
        BWAPI::TilePosition pylonTile = naturalWallPylonTile.isValid() ? naturalWallPylonTile : findNaturalChokePylonTile();
        if (!pylonTile.isValid() || !isValidBuildTile(BWAPI::UnitTypes::Protoss_Pylon, pylonTile))
            return false;

        /*ResourceRequest req;
        req.type = ResourceRequest::Type::Building;
        req.unit = BWAPI::UnitTypes::Protoss_Pylon;
        req.fromBuildOrder = true;
        req.useForcedTile = true;
        req.forcedTile = pylonTile;*/

        // Then reserve
        BWEB::Map::addReserve(pylonTile, BWAPI::UnitTypes::Protoss_Pylon.tileWidth(), BWAPI::UnitTypes::Protoss_Pylon.tileHeight());

        commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Pylon, true, true, pylonTile);

        naturalWallPylonEnqueued = true;
        naturalWallPylonTile = pylonTile;
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
                        /*ResourceRequest req;
                        req.type = ResourceRequest::Type::Building;
                        req.unit = ut;
                        req.fromBuildOrder = true;
                        req.useForcedTile = true;
                        req.forcedTile = best;*/

                        commanderReference->requestBuilding(ut, true, true, best);
                        enqueuedAny = true;
                    }
                    return;
                }
            }

            /*ResourceRequest req;
            req.type = ResourceRequest::Type::Building;
            req.unit = ut;
            req.fromBuildOrder = true;
            req.useForcedTile = true;
            req.forcedTile = t;*/

            commanderReference->requestBuilding(ut, true, true, t);
            enqueuedAny = true;
        };

        if (naturalWallGatewayTiles.empty())
            return false;

        int placed = 0;
        for (const auto& t : naturalWallGatewayTiles)
        {
            enqueueForced(BWAPI::UnitTypes::Protoss_Gateway, t);
            placed++;
            if (placed >= 2) break;
        }

        if (!enqueuedAny)
            return false;

        naturalWallGatewaysEnqueued = true;
        return true;
    }

    return true;
}