#pragma once
#include <vector>
#include <BWAPI.h>

class InfluenceMap
{
public:
    InfluenceMap();

    void onStart();
    void onFrame();
    bool drawEnabled = true;

    void toggleDraw();
    void draw() const;

    void addEnemyInfluence(const BWAPI::Position& pos, int strength);
    void addAllyInfluence(const BWAPI::Position& pos, int strength);
    void addEnemyBuildingInfluence(const BWAPI::Position& pos, const BWAPI::UnitType& type);
    void addAllyBuildingInfluence(const BWAPI::Position& pos, const BWAPI::UnitType& type);

    int getEnemyInfluence(int x, int y) const;
    int getAllyInfluence(int x, int y) const;
    int buildingInfluenceValue(const BWAPI::UnitType& type) const;
    int getHeight() const;
    int getWidth() const;

    BWAPI::Position findWeakBuildingSpot();
    std::vector<BWAPI::Position> getNoMansAreas() const;
    std::string toStringSummary() const;

private:
    int width;
    int height;

    std::vector<std::vector<int>> enemyMap;
    std::vector<std::vector<int>> allyMap;

    void decayMaps();
    void normalizeMaps();
};
