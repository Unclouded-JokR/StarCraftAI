#pragma once

#include <BWAPI.h>
#include <set>
#include <map>
#include <iostream>
#include "InfluenceMap.h"


class ProtoBotCommander;

struct EnemyBuildingInfo
{
    BWAPI::UnitType type;
    BWAPI::Position lastKnownPosition;
    bool destroyed = false;
};

struct TrackedEnemy {
    BWAPI::UnitType type;
    BWAPI::Position lastSeenPos;
    int id;                 // Unit ID
    bool isBuilding;
    bool destroyed = false;
};

class InformationManager
{
private:
    std::set<BWAPI::Unit> _knownEnemies;
    std::map<BWAPI::Unit, EnemyBuildingInfo> _knownEnemyBuildings;
    std::unordered_map<int, TrackedEnemy> knownEnemies;
    InfluenceMap influenceMap;
    double gameState;

public:
    ProtoBotCommander* commanderReference;

    InformationManager(ProtoBotCommander* commanderReference);
    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);
    double evaluateGameState() const;

    // Utility / debug
    const std::set<BWAPI::Unit>& getKnownEnemies() const { return _knownEnemies; }
    const std::map<BWAPI::Unit, EnemyBuildingInfo>& getKnownEnemyBuildings() const { return _knownEnemyBuildings; }

    void printKnownEnemies() const;
    void printKnownEnemyBuildings() const;
};