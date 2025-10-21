#pragma once

#include <BWAPI.h>
#include <set>
#include <map>
#include <iostream>

class ProtoBotCommander;

struct EnemyBuildingInfo
{
    BWAPI::UnitType type;
    BWAPI::Position lastKnownPosition;
    bool destroyed = false;
};

class InformationManager
{
private:
    std::set<BWAPI::Unit> _knownEnemies;
    std::map<BWAPI::Unit, EnemyBuildingInfo> _knownEnemyBuildings;

public:
    ProtoBotCommander* commanderReference;

    InformationManager(ProtoBotCommander* commanderReference);
    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);

    // Utility / debug
    const std::set<BWAPI::Unit>& getKnownEnemies() const { return _knownEnemies; }
    const std::map<BWAPI::Unit, EnemyBuildingInfo>& getKnownEnemyBuildings() const { return _knownEnemyBuildings; }

    void printKnownEnemies() const;
    void printKnownEnemyBuildings() const;
};