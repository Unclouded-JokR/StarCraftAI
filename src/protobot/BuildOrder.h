#pragma once

#include <vector>
#include <string>
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
    Train,               // train a unit
    ScoutWorker,         // call commanderReference->getUnitToScout()
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
    int blockUnitTrainingUntilSupply = 0;

    std::vector<BuildOrderStep> steps;
};

namespace BuildOrders
{
    std::vector<BuildOrder> createAll();
}
