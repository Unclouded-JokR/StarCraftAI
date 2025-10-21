#pragma once
#include <BWAPI.h>

class ProtoBotCommander;

class BuildManager{
public:
    ProtoBotCommander* commanderReference;

    BuildManager(ProtoBotCommander* commanderReference);

    void onStart();
    void onFrame();
    void assignBuilding(BWAPI::Unit unit);
};