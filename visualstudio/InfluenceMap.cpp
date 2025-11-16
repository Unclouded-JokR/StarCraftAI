#include "InfluenceMap.h"
#include <sstream>

InfluenceMap::InfluenceMap() {}

void InfluenceMap::onStart()
{
    width = BWAPI::Broodwar->mapWidth() * 4;   // 1 tile = 4 walktiles
    height = BWAPI::Broodwar->mapHeight() * 4;

    enemyMap.assign(width, std::vector<int>(height, 0));
    allyMap.assign(width, std::vector<int>(height, 0));
}

void InfluenceMap::onFrame()
{
    // Decay slightly to keep map "fresh"
    decayMaps();
}

void InfluenceMap::addEnemyInfluence(const BWAPI::Position& pos, int strength)
{
    int x = pos.x / 8;
    int y = pos.y / 8;

    int radius = 6;
    for (int dx = -radius; dx <= radius; dx++)
    {
        for (int dy = -radius; dy <= radius; dy++)
        {
            int nx = x + dx;
            int ny = y + dy;

            if (nx >= 0 && nx < width && ny >= 0 && ny < height)
            {
                int dist = dx * dx + dy * dy;
                if (dist <= radius * radius)
                    enemyMap[nx][ny] += strength;
            }
        }
    }
}

void InfluenceMap::addAllyInfluence(const BWAPI::Position& pos, int strength)
{
    int x = pos.x / 8;
    int y = pos.y / 8;

    int radius = 5;
    for (int dx = -radius; dx <= radius; dx++)
    {
        for (int dy = -radius; dy <= radius; dy++)
        {
            int nx = x + dx;
            int ny = y + dy;

            if (nx >= 0 && nx < width && ny >= 0 && ny < height)
            {
                int dist = dx * dx + dy * dy;
                if (dist <= radius * radius)
                    allyMap[nx][ny] += strength;
            }
        }
    }
}

int InfluenceMap::getEnemyInfluence(int x, int y) const
{
    return enemyMap[x][y];
}

int InfluenceMap::getAllyInfluence(int x, int y) const
{
    return allyMap[x][y];
}

void InfluenceMap::decayMaps()
{
    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++)
        {
            enemyMap[x][y] *= 0.98;
            allyMap[x][y] *= 0.98;
        }
}

BWAPI::Position InfluenceMap::findWeakBuildingSpot()
{
    int bestScore = 9999999;
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;

    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++)
        {
            int score = enemyMap[x][y] - allyMap[x][y];
            if (score < bestScore)
            {
                bestScore = score;
                bestPos = BWAPI::Position(x * 8, y * 8);
            }
        }

    return bestPos;
}

std::vector<BWAPI::Position> InfluenceMap::getNoMansAreas() const
{
    std::vector<BWAPI::Position> spots;

    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++)
            if (enemyMap[x][y] < 10 && allyMap[x][y] < 10)
                spots.push_back(BWAPI::Position(x * 8, y * 8));

    return spots;
}

std::string InfluenceMap::toStringSummary() const
{
    std::stringstream ss;

    int enemyMax = 0, allyMax = 0;
    long long enemySum = 0, allySum = 0;
    int count = width * height;

    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++)
        {
            int e = enemyMap[x][y];
            int a = allyMap[x][y];
            enemySum += e;
            allySum += a;
            if (e > enemyMax) enemyMax = e;
            if (a > allyMax) allyMax = a;
        }

    ss << "Influence Map Summary:\n"
        << "  Size: " << width << " x " << height << "\n"
        << "  Enemy Influence: max=" << enemyMax << " avg=" << (enemySum / count) << "\n"
        << "  Ally Influence:  max=" << allyMax << " avg=" << (allySum / count) << "\n";

    return ss.str();
}

void InfluenceMap::toggleDraw()
{
    drawEnabled = !drawEnabled;
}

BWAPI::Color influenceColor(int value)
{
    if (value <= 0) return BWAPI::Colors::Black;

    if (value < 10) return BWAPI::Colors::Blue;
    if (value < 30) return BWAPI::Colors::Green;
    if (value < 60) return BWAPI::Colors::Yellow;
    if (value < 120) return BWAPI::Colors::Orange;
    return BWAPI::Colors::Red;
}

void InfluenceMap::draw() const
{
    if (!drawEnabled)
        return;

    const int step = 8;  // skip walktiles to reduce lag

    for (int x = 0; x < width; x += step)
    {
        for (int y = 0; y < height; y += step)
        {
            int enemyInf = enemyMap[x][y];
            int allyInf = allyMap[x][y];

            if (enemyInf == 0 && allyInf == 0)
                continue;

            BWAPI::Position pos(x * 8, y * 8);

            // Draw ally influence in blue-green
            BWAPI::Broodwar->drawBoxMap(
                pos,
                BWAPI::Position(pos.x + 8, pos.y + 8),
                influenceColor(allyInf),
                false
            );

            // Draw enemy influence border in red
            BWAPI::Broodwar->drawBoxMap(
                pos,
                BWAPI::Position(pos.x + 8, pos.y + 8),
                influenceColor(enemyInf + allyInf),
                false
            );

            // Print influence values
            BWAPI::Broodwar->drawTextMap(
                pos,
                "%d/%d", allyInf, enemyInf
            );
        }
    }
}

int InfluenceMap::buildingInfluenceValue(const BWAPI::UnitType& type) const
{
    //This can be expanded upon to have different buildings return different values
    // but I'm not sure what those values should be, so there's just a placeholder for now
    return 10;
}

void InfluenceMap::addEnemyBuildingInfluence(const BWAPI::Position& pos, const BWAPI::UnitType& type)
{
    int strength = buildingInfluenceValue(type);
    addEnemyInfluence(pos, strength);
}

void InfluenceMap::addAllyBuildingInfluence(const BWAPI::Position& pos, const BWAPI::UnitType& type)
{
    int strength = buildingInfluenceValue(type);
    addAllyInfluence(pos, strength);
}

int InfluenceMap::getHeight() const
{
    return height;
}

int InfluenceMap::getWidth() const
{
    return width;
}
