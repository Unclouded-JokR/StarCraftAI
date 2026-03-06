#pragma once
#include <BWAPI.h>
#include <vector>
#include "A-StarPathfinding.h"

#define PATH_DISTANCE_THRESHOLD 32
#define CONSTRUCT_DISTANCE_THRESHOLD 64
#define MAXIMUM_PIXEL_DIFFERENCE 5
#define IDLE_FRAMES_BEFORE_FORCE_NEXT_POSITION 240

class Builder
{
private:
    BWAPI::Unit unitReference;
    BWAPI::Position lastPosition;

    Path referencePath;
    size_t pathIndex = 0;
    int idleFrames = 0;
    bool debug = false;

    std::vector<BWAPI::TilePosition> getPathToConstruct();
public:
    BWAPI::Position requestedPositionToBuild;
    BWAPI::UnitType buildingToConstruct;

    Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToPlace, BWAPI::Position requestedLocation, Path path);
    ~Builder();

    void onFrame();
    BWAPI::Unit getUnitReference();
    void setUnitReference(BWAPI::Unit);
    void updatePath(Path& path);
};

