#pragma once
#include <BWAPI.h>
#include <vector>
#include "A-StarPathfinding.h"

#define PATH_DISTANCE_THRESHOLD 32
#define CONSTRUCT_DISTANCE_THRESHOLD 64

class Builder
{
private:
    BWAPI::Unit unitReference;
    Path referencePath;
    size_t pathIndex = 0;
    bool debug = false;

    std::vector<BWAPI::TilePosition> getPathToConstruct();
public:
    BWAPI::Position requestedPositionToBuild;
    BWAPI::TilePosition requestedTileToBuild; // for exploration + reservation cleanup

    BWAPI::UnitType buildingToConstruct;

    // Timeout / give-up state
    int framesSinceAssigned = 0;
    int lastProgressFrame = 0;
    double lastDistance = 1e9;
    bool gaveUp = false;

    // Tuning
    int maxFramesToRevealOrReach = 24 * 15;   // 15 seconds
    int progressCheckWindow = 24 * 3;         // 3 seconds
    double minProgressPixels = 64.0;          // must close at least 2 tiles per window


    Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToPlace, BWAPI::Position requestedLocation, Path path);
    ~Builder();

    void onFrame();
    bool hasGivenUp() const { return gaveUp; }
    BWAPI::Unit getUnitReference();
    void setUnitReference(BWAPI::Unit);
};

