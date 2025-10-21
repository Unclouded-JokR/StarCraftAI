#pragma once
#include <BWAPI.h>

class ProtoBotCommander;

class ScoutingManager
{
public:
    ProtoBotCommander* commanderReference;

    ScoutingManager(ProtoBotCommander* commanderReference);
    void onStart();
    void onFrame();
    
private:
    BWAPI::Unit scout = nullptr;
    std::vector<BWAPI::TilePosition> targets;
    int nextTarget = 0;
};

