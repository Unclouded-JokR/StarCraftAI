#pragma once
#include <BWAPI.h>
#include <vector>
#include "A-StarPathfinding.h"

#define DISTANCE_THRESHOLD 10

class Builder
{
private:
    BWAPI::Unit unitReference;
    std::vector<BWAPI::Position> path;
    size_t pathIndex = 0;

    BWAPI::Position initialPosition; //used for debug
    Path referencePath;

    std::vector<BWAPI::TilePosition> getPathToConstruct();
public:
    BWAPI::Position requestedPositionToBuild;
    BWAPI::UnitType buildingToConstruct;
    bool isConstructing = false;

    Builder(BWAPI::Unit, BWAPI::UnitType, BWAPI::Position);
    ~Builder();
    void onFrame();
    BWAPI::Unit getUnitReference();
    void setUnitReference(BWAPI::Unit);
};

