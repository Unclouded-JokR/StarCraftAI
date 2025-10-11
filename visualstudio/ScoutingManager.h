#pragma once
#include <BWAPI.h>
class ScoutingManager
{
public:
    void onStart();
    void onFrame();
    
private:
    BWAPI::Unit scout = nullptr;
    std::vector<BWAPI::TilePosition> targets;
    int nextTarget = 0;
};

