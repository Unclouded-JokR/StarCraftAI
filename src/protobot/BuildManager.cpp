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

    // Attempt to keep the wall gap open for units to pass through
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

    const auto* naturalChoke = BWEB::Map::getNaturalChoke();
    if (naturalChoke)
    {
        if (naturalWallOpeningTile.isValid())
        {
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(naturalWallOpeningTile), BWAPI::Position(naturalWallOpeningTile) + BWAPI::Position(32, 32), BWAPI::Colors::Orange, false);
            BWAPI::Broodwar->drawTextMap(BWAPI::Position(naturalWallOpeningTile) + BWAPI::Position(4, 4), "Wall gap / A*");
        }
        for (const auto& gapTile : naturalWallGapTiles)
        {
            if (!gapTile.isValid()) continue;
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(gapTile), BWAPI::Position(gapTile) + BWAPI::Position(32, 32), BWAPI::Colors::Green, false);
        }
        for (const auto& t : naturalWallPathTiles)
        {
            if (!t.isValid()) continue;
            const BWAPI::Position tl = BWAPI::Position(t);
            const BWAPI::Position br = tl + BWAPI::Position(32, 32);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Yellow, false);
        }
        BWEB::Walls::draw();
        for (const auto& pylonTile : naturalWallPylonTiles)
        {
            if (!pylonTile.isValid()) continue;
            const BWAPI::Position tl = BWAPI::Position(pylonTile);
            const BWAPI::Position br = tl + BWAPI::Position(64, 64);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Green, false);
            BWAPI::Broodwar->drawTextMap(tl + BWAPI::Position(4, 4), "Wall pylon");
        }
        for (const auto& t : naturalWallCannonTiles)
        {
            if (!t.isValid()) continue;
            const BWAPI::Position tl = BWAPI::Position(t);
            const BWAPI::Position br = tl + BWAPI::Position(64, 64);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Purple, false);
            BWAPI::Broodwar->drawTextMap(tl + BWAPI::Position(4, 4), "Wall cannon");
        }
        for (const auto& t : naturalWallForgeTiles)
        {
            if (!t.isValid()) continue;
            const BWAPI::Position tl = BWAPI::Position(t);
            const BWAPI::Position br = tl + BWAPI::Position(96, 64);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Cyan, false);
            BWAPI::Broodwar->drawTextMap(tl + BWAPI::Position(4, 4), "Wall forge");
        }
        for (const auto& t : naturalWallGatewayTiles)
        {
            if (!t.isValid()) continue;
            const BWAPI::Position tl = BWAPI::Position(t);
            const BWAPI::Position br = tl + BWAPI::Position(128, 96);
            BWAPI::Broodwar->drawBoxMap(tl, br, BWAPI::Colors::Red, false);
            BWAPI::Broodwar->drawTextMap(tl + BWAPI::Position(4, 4), "Wall gateway");
        }
    }

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

bool BuildManager::shouldPreventUnitTraining(int currentSupply) const
{
    if (!isBuildOrderActive())
        return false;

    const BuildOrder& bo = buildOrders[activeBuildOrderIndex];
    return bo.blockUnitTrainingUntilSupply > 0 && currentSupply < bo.blockUnitTrainingUntilSupply;
}

void BuildManager::pumpUnit()
{
    const FriendlyUnitCounter ProtoBot_Units = InformationManager::Instance().getFriendlyUnitCounter();
    const FriendlyBuildingCounter ProtoBot_Buildings = InformationManager::Instance().getFriendlyBuildingCounter();
    const FriendlyUpgradeCounter ProtoBot_Upgrades = InformationManager::Instance().getFriendlyUpgradeCounter();
    const int totalMinerals = BWAPI::Broodwar->self()->minerals();
    const int totalGas = BWAPI::Broodwar->self()->gas();
    const int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;
    const bool preventCombatUnitTraining = shouldPreventUnitTraining(currentSupply);

    for (BWAPI::Unit unit : buildings)
    {
        const BWAPI::UnitType type = unit->getType();
        if (type == BWAPI::UnitTypes::Protoss_Gateway && !preventCombatUnitTraining && !unit->isTraining() && !commanderReference->alreadySentRequest(unit->getID()))
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
        else if (unit->getType() == BWAPI::UnitTypes::Protoss_Robotics_Facility && !preventCombatUnitTraining && !unit->isTraining() && !commanderReference->alreadySentRequest(unit->getID()) && unit->canTrain(BWAPI::UnitTypes::Protoss_Observer))
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

bool BuildManager::enqueueBuildOrderUnit(BWAPI::UnitType type, int count)
{
    if (!commanderReference)
        return false;

    const BWAPI::UnitType trainerType = type.whatBuilds().first;
    if (trainerType == BWAPI::UnitTypes::None)
        return false;

    int requestsIssued = 0;
    for (BWAPI::Unit building : buildings)
    {
        if (!building || building->getType() != trainerType)
            continue;
        if (!building->isCompleted() || !building->isPowered() || building->isTraining())
            continue;
        if (!building->canTrain(type) || commanderReference->alreadySentRequest(building->getID()))
            continue;

        commanderReference->requestUnit(type, building, true);
        requestsIssued++;

        if (requestsIssued >= count)
            return true;
    }

    return false;
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

    case BuildStepType::Train:
        issued = enqueueBuildOrderUnit(step.unit, step.count);
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

    BWEB::Map::addReserve(t, type.tileWidth(), type.tileHeight());
    commanderReference->requestBuilding(type, true, true, t);
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
    const auto* area = BWEB::Map::getNaturalArea();
    if (!choke || !area)
        return false;

    if (!naturalWallPlanned)
    {
        const int supply = BWAPI::Broodwar->self()->supplyUsed() / 2;
        std::cout << "[WallDebug] Attempting natural wall plan at supply " << supply << std::endl;
        BWAPI::Broodwar->printf("[WallDebug] Attempting natural wall plan at supply %d", supply);
        resetNaturalWallPlan();
        naturalWallChokeAnchorTile = BWAPI::TilePosition(BWEB::Map::getClosestChokeTile(choke, BWEB::Map::getNaturalPosition()));

        struct WallLayoutSpec
        {
            std::vector<BWAPI::UnitType> buildings;
            std::vector<BWAPI::UnitType> defenses;
        };

        std::vector<WallLayoutSpec> layouts = {
            { { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway }, { } },
            { { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Forge, BWAPI::UnitTypes::Protoss_Gateway }, { } },
            { { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::UnitTypes::Protoss_Forge }, { } },
            { { BWAPI::UnitTypes::Protoss_Pylon, BWAPI::UnitTypes::Protoss_Forge }, { } },
            { { BWAPI::UnitTypes::Protoss_Pylon }, { } }
        };

        naturalWallPathTiles.clear();
        Path naturalPath = AStar::GeneratePath(BWEB::Map::getMainPosition(), BWAPI::UnitTypes::Protoss_Probe, BWEB::Map::getNaturalPosition());
        naturalWallPathTiles = uniquePathTiles(naturalPath);

        BWEB::Wall* wall = BWEB::Walls::getWall(choke);
        if (!wall)
        {
            BWEB::Walls::onEnd();
            for (const auto& layout : layouts)
            {
                std::vector<BWAPI::UnitType> buildings = layout.buildings;
                std::cout << "[WallDebug] Trying natural wall layout: buildings=" << buildings.size()
                          << " defenses=" << layout.defenses.size() << std::endl;
                wall = BWEB::Walls::createWall(buildings, area, choke, BWAPI::UnitTypes::None, layout.defenses, true, false);
                if (!wall)
                    wall = BWEB::Walls::getWall(choke);
                if (!wall)
                    wall = BWEB::Walls::getClosestWall(BWAPI::TilePosition(BWEB::Map::getNaturalPosition()));

                if (wall && wall->getChokePoint() == choke)
                {
                    std::cout << "[WallDebug] Created natural wall using " << wall->getRawBuildings().size()
                              << " raw buildings, small=" << wall->getSmallTiles().size()
                              << " medium=" << wall->getMediumTiles().size()
                              << " large=" << wall->getLargeTiles().size()
                              << " defenses=" << wall->getDefenses(0).size() << std::endl;
                    break;
                }

                std::cout << "[WallDebug] createWall failed for layout buildings=" << buildings.size()
                          << " defenses=" << layout.defenses.size() << std::endl;
                wall = nullptr;
                BWEB::Walls::onEnd();
            }
        }
        naturalWallPylonTiles.clear();
        naturalWallGatewayTiles.clear();
        naturalWallForgeTiles.clear();
        naturalWallCannonTiles.clear();

        if (!wall)
        {
            const int supply = BWAPI::Broodwar->self()->supplyUsed() / 2;
            std::cout << "[WallDebug] createWall failed for all core variants at supply " << supply << ", synthesizing compact fallback wall" << std::endl;
            BWAPI::Broodwar->printf("[WallDebug] createWall failed, synthesizing compact fallback wall");

            naturalWallPylonTile = findNaturalChokePylonTile();
            if (!naturalWallPylonTile.isValid())
            {
                std::cout << "[WallDebug] Fallback wall failed: no valid pylon tile" << std::endl;
                return false;
            }
            naturalWallPylonTiles.push_back(naturalWallPylonTile);
            naturalWallOpeningTile = naturalWallPylonTile;
            naturalWallGapTiles = chooseProtectedGapCorridor(naturalWallOpeningTile, naturalWallPathTiles);

            BWAPI::TilePosition synthGateway = chooseCompactWallGatewayTile(naturalWallPylonTile, naturalWallForgeTiles, BWAPI::TilePositions::Invalid, naturalWallOpeningTile, naturalWallGapTiles);
            if (synthGateway.isValid())
                naturalWallGatewayTiles.push_back(synthGateway);
            else
                std::cout << "[WallDebug] Fallback wall missing gateway synthesis" << std::endl;

            BWAPI::TilePosition synthForge = chooseCompactWallForgeTile(naturalWallPylonTile, naturalWallGatewayTiles, naturalWallOpeningTile, naturalWallGapTiles);
            if (synthForge.isValid())
                naturalWallForgeTiles.push_back(synthForge);
            else
                std::cout << "[WallDebug] Fallback wall missing forge synthesis" << std::endl;
        }
        else
        {
            naturalWallPylonTiles.assign(wall->getSmallTiles().begin(), wall->getSmallTiles().end());
            naturalWallGatewayTiles.assign(wall->getLargeTiles().begin(), wall->getLargeTiles().end());
            naturalWallForgeTiles.assign(wall->getMediumTiles().begin(), wall->getMediumTiles().end());
            naturalWallOpeningTile = chooseWallGapOnPath(wall, naturalWallPathTiles);
            if (!naturalWallOpeningTile.isValid())
                naturalWallOpeningTile = wall->getOpening();
            naturalWallPylonTile = naturalWallPylonTiles.empty() ? BWAPI::TilePositions::Invalid : naturalWallPylonTiles.front();
        }

        naturalWallGapTiles = chooseProtectedGapCorridor(naturalWallOpeningTile, naturalWallPathTiles);

        if (naturalWallPylonTile.isValid() && naturalWallPylonTiles.empty())
            naturalWallPylonTiles.push_back(naturalWallPylonTile);

        if (naturalWallGatewayTiles.empty())
        {
            BWAPI::TilePosition gatewayTile = chooseCompactWallGatewayTile(naturalWallPylonTile, naturalWallForgeTiles, BWAPI::TilePositions::Invalid, naturalWallOpeningTile, naturalWallGapTiles);
            if (gatewayTile.isValid())
                naturalWallGatewayTiles.push_back(gatewayTile);
            else
                std::cout << "[WallDebug] No compact gateway tile synthesized during wall planning" << std::endl;
        }

        if (naturalWallForgeTiles.empty())
        {
            BWAPI::TilePosition forgeTile = chooseCompactWallForgeTile(naturalWallPylonTile, naturalWallGatewayTiles, naturalWallOpeningTile, naturalWallGapTiles);
            if (forgeTile.isValid())
                naturalWallForgeTiles.push_back(forgeTile);
            else
                std::cout << "[WallDebug] No compact forge tile synthesized during wall planning" << std::endl;
        }

        naturalWallCannonTiles = chooseCompactWallCannonArcTiles(3, naturalWallPylonTile, naturalWallForgeTiles, naturalWallGatewayTiles, naturalWallOpeningTile, naturalWallGapTiles);
        if ((int)naturalWallCannonTiles.size() < 3)
            std::cout << "[WallDebug] Only synthesized " << naturalWallCannonTiles.size() << " / 3 cannon tiles during wall planning" << std::endl;
        naturalWallPlanned = true;
        naturalWallPylonEnqueued = false;
        naturalWallGatewaysEnqueued = false;
        naturalWallCannonsEnqueued = false;
        naturalWallStartLogged = false;

        const BWAPI::TilePosition centroidTile = wall ? BWAPI::TilePosition(wall->getCentroid()) : BWAPI::TilePositions::Invalid;
        const BWAPI::TilePosition rawOpeningTile = wall ? wall->getOpening() : BWAPI::TilePositions::Invalid;
        std::cout << "[WallDebug] Planned natural wall at opening (" << naturalWallOpeningTile.x << "," << naturalWallOpeningTile.y
                  << ") raw opening (" << rawOpeningTile.x << "," << rawOpeningTile.y << ") centroid (" << centroidTile.x << "," << centroidTile.y << ") using "
                  << (naturalWallPylonTiles.size() + naturalWallGatewayTiles.size() + naturalWallForgeTiles.size() + naturalWallCannonTiles.size()) << " pieces"
                  << " [p=" << naturalWallPylonTiles.size() << " g=" << naturalWallGatewayTiles.size() << " f=" << naturalWallForgeTiles.size() << " c=" << naturalWallCannonTiles.size() << "]" << std::endl;
    }

    if (!naturalWallPylonEnqueued)
    {
        bool enqueuedAny = false;
        for (const auto& pylonTile : naturalWallPylonTiles)
        {
            if (!isValidBuildTile(BWAPI::UnitTypes::Protoss_Pylon, pylonTile))
                continue;

            commanderReference->requestBuilding(BWAPI::UnitTypes::Protoss_Pylon, true, true, pylonTile);
            BWEB::Map::addReserve(pylonTile, BWAPI::UnitTypes::Protoss_Pylon.tileWidth(), BWAPI::UnitTypes::Protoss_Pylon.tileHeight());
            enqueuedAny = true;
        }

        if (!enqueuedAny)
        {
            const int supply = BWAPI::Broodwar->self()->supplyUsed() / 2;
            std::cout << "[WallDebug] Natural wall exists but no pylon tile was buildable at supply " << supply << std::endl;
            for (const auto& pylonTile : naturalWallPylonTiles)
            {
                std::cout << "[WallDebug]   pylon tile (" << pylonTile.x << "," << pylonTile.y << ")"
                          << " terrain=" << isTerrainBuildable(BWAPI::UnitTypes::Protoss_Pylon, pylonTile)
                          << " valid=" << isValidBuildTile(BWAPI::UnitTypes::Protoss_Pylon, pylonTile)
                          << " reserved=" << BWEB::Map::isReserved(pylonTile, BWAPI::UnitTypes::Protoss_Pylon.tileWidth(), BWAPI::UnitTypes::Protoss_Pylon.tileHeight())
                          << std::endl;
            }
            BWAPI::Broodwar->printf("[WallDebug] Natural wall exists but no pylon tile was buildable at supply %d", supply);
            return false;
        }

        naturalWallPylonEnqueued = true;

        if (!naturalWallStartLogged)
        {
            const int supply = BWAPI::Broodwar->self()->supplyUsed() / 2;
            std::cout << "[WallDebug] Starting wall construction at supply " << supply << std::endl;
            BWAPI::Broodwar->printf("[WallDebug] Starting wall construction at supply %d", supply);
            naturalWallStartLogged = true;
        }

        return true;
    }

    bool pylonComplete = false;
    for (auto u : BWAPI::Broodwar->self()->getUnits())
    {
        if (u->getType() != BWAPI::UnitTypes::Protoss_Pylon || !u->isCompleted())
            continue;

        const BWAPI::TilePosition unitTile = u->getTilePosition();
        for (const auto& pylonTile : naturalWallPylonTiles)
        {
            if (unitTile == pylonTile)
            {
                pylonComplete = true;
                break;
            }
        }
        if (pylonComplete)
            break;
    }

    if (!pylonComplete)
        return false;

    if (!naturalWallGatewaysEnqueued)
    {
        bool enqueuedAny = false;

        auto enqueueForced = [&](BWAPI::UnitType ut, const BWAPI::TilePosition& t)
        {
            if (!t.isValid())
                return;
            if (!isTerrainBuildable(ut, t))
                return;

            commanderReference->requestBuilding(ut, true, true, t);
            BWEB::Map::addReserve(t, ut.tileWidth(), ut.tileHeight());
            enqueuedAny = true;
        };

        for (const auto& t : naturalWallForgeTiles)
            enqueueForced(BWAPI::UnitTypes::Protoss_Forge, t);
        for (const auto& t : naturalWallGatewayTiles)
            enqueueForced(BWAPI::UnitTypes::Protoss_Gateway, t);

        if (!enqueuedAny && naturalWallCannonTiles.empty())
        {
            const int supply = BWAPI::Broodwar->self()->supplyUsed() / 2;
            std::cout << "[WallDebug] Wall pylon finished but no follow-up wall building was buildable at supply " << supply << std::endl;
            for (const auto& t : naturalWallForgeTiles)
                std::cout << "[WallDebug]   forge tile (" << t.x << "," << t.y << ") terrain=" << isTerrainBuildable(BWAPI::UnitTypes::Protoss_Forge, t) << " valid=" << isValidBuildTile(BWAPI::UnitTypes::Protoss_Forge, t) << std::endl;
            for (const auto& t : naturalWallGatewayTiles)
                std::cout << "[WallDebug]   gateway tile (" << t.x << "," << t.y << ") terrain=" << isTerrainBuildable(BWAPI::UnitTypes::Protoss_Gateway, t) << " valid=" << isValidBuildTile(BWAPI::UnitTypes::Protoss_Gateway, t) << std::endl;
            BWAPI::Broodwar->printf("[WallDebug] Wall pylon finished but no follow-up wall building was buildable at supply %d", supply);
            return false;
        }

        naturalWallGatewaysEnqueued = true;
    }

    bool forgeReadyForCannons = false;
    if (naturalWallForgeTiles.empty())
    {
        for (auto u : BWAPI::Broodwar->self()->getUnits())
        {
            if (u->getType() == BWAPI::UnitTypes::Protoss_Forge && u->isCompleted())
            {
                forgeReadyForCannons = true;
                break;
            }
        }
    }
    else
    {
        for (auto u : BWAPI::Broodwar->self()->getUnits())
        {
            if (u->getType() != BWAPI::UnitTypes::Protoss_Forge || !u->isCompleted())
                continue;

            const BWAPI::TilePosition unitTile = u->getTilePosition();
            for (const auto& forgeTile : naturalWallForgeTiles)
            {
                if (unitTile == forgeTile)
                {
                    forgeReadyForCannons = true;
                    break;
                }
            }
            if (forgeReadyForCannons)
                break;
        }
    }

    if (!naturalWallCannonsEnqueued && forgeReadyForCannons)
    {
        bool enqueuedAny = false;
        for (const auto& originalTile : naturalWallCannonTiles)
        {
            BWAPI::TilePosition t = originalTile;
            const BWAPI::UnitType cannon = BWAPI::UnitTypes::Protoss_Photon_Cannon;
            if (!t.isValid() || !isTerrainBuildable(cannon, t) || !BWAPI::Broodwar->hasPower(t, cannon))
            {
                BWAPI::TilePosition fallback = chooseCompactWallCannonTile(naturalWallPylonTile, naturalWallForgeTiles, naturalWallGatewayTiles, naturalWallOpeningTile, naturalWallGapTiles);
                if (fallback.isValid() && BWAPI::Broodwar->hasPower(fallback, cannon))
                    t = fallback;
            }
            if (!t.isValid())
                continue;
            if (!isTerrainBuildable(cannon, t))
                continue;
            if (!BWAPI::Broodwar->hasPower(t, cannon))
                continue;

            commanderReference->requestBuilding(cannon, true, true, t);
            BWEB::Map::addReserve(t, cannon.tileWidth(), cannon.tileHeight());
            enqueuedAny = true;
        }

        if (enqueuedAny)
            naturalWallCannonsEnqueued = true;
        else if (!naturalWallCannonTiles.empty())
        {
            std::cout << "[WallDebug] Forge is ready but no wall cannon tile was buildable/powered" << std::endl;
            for (const auto& originalTile : naturalWallCannonTiles)
            {
                const BWAPI::UnitType cannon = BWAPI::UnitTypes::Protoss_Photon_Cannon;
                std::cout << "[WallDebug]   cannon tile (" << originalTile.x << "," << originalTile.y << ")"
                          << " terrain=" << isTerrainBuildable(cannon, originalTile)
                          << " power=" << BWAPI::Broodwar->hasPower(originalTile, cannon)
                          << " valid=" << isValidBuildTile(cannon, originalTile)
                          << std::endl;
            }
            BWAPI::Broodwar->printf("[WallDebug] Forge ready but no wall cannon tile was buildable/powered");
        }
    }

    return true;
}