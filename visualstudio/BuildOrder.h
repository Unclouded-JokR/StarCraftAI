#pragma once

#include <vector>
#include <BWAPI.h>

enum class BuildTriggerType { Immediately, AtSupply };

struct BuildTrigger
{
    BuildTriggerType type = BuildTriggerType::Immediately;
    int value = 0;
};

enum class BuildStepType
{
    Build,               // build a building (unit field)
    ScoutWorker,         // call commanderReference->getUnitToScout()
    SupplyRampNatural,   // place a pylon on/near natural ramp using findNaturalRampPlacement
    NaturalWall          // create a wall at natural choke using BWEB
};

struct BuildOrderStep
{
    BuildStepType type = BuildStepType::Build;
    BWAPI::UnitType unit = BWAPI::UnitTypes::None;
    int count = 1;                                 
    BuildTrigger trigger;
};

struct BuildOrder
{
    int name = 0;
    int id = 0;

    BWAPI::Race vsRace = BWAPI::Races::Unknown;

    std::vector<BuildOrderStep> steps;
};

namespace BuildOrders
{
    std::vector<BuildOrder> createAll();
}
