#pragma once
#include <BWAPI.h>
#include <vector>
#include "A-StarPathfinding.h"

#define DISTANCE_THRESHOLD 32

class Builder
{
private:
    BWAPI::Unit unitReference;
    Path referencePath;
    size_t pathIndex = 0;

    std::vector<BWAPI::TilePosition> getPathToConstruct();
public:
    BWAPI::Position requestedPositionToBuild;
    BWAPI::UnitType buildingToConstruct;

    Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToPlace, BWAPI::Position requestedLocation, Path path);
    ~Builder();

    void onFrame();
    BWAPI::Unit getUnitReference();
    void setUnitReference(BWAPI::Unit);
};

