#pragma once
#include <BWAPI.h>

class ProtoBotCommander;

class BuildManager{
public:
    ProtoBotCommander* commanderReference;
    bool buildOrderCompleted = false;

    BuildManager(ProtoBotCommander* commanderReference);

    void onStart();
    void onFrame();
    void assignBuilding(BWAPI::Unit unit);
    bool isBuildOrderCompleted();
    void buildBuilding(BWAPI::Unit unit, BWAPI::UnitType building);
};