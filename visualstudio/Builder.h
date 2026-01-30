#pragma once
#include <BWAPI.h>
#include <vector>

class Builder
{
private:
    BWAPI::Unit unitReference;
    std::vector<BWAPI::TilePosition> path;

    std::vector<BWAPI::TilePosition> getPathToConstruct();
public:
    BWAPI::Position positionToBuild;
    BWAPI::UnitType buildingToConstruct;

    Builder(BWAPI::Unit, BWAPI::UnitType, BWAPI::Position);
    ~Builder();
    void onFrame();
    BWAPI::Unit getUnitReference();
    void setUnitReference(BWAPI::Unit);
};

