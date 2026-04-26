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

/// <summary>
/// Build Orders instructions you would see in the description for a BuildOrder define what a player should do to emulate a BuildOrder. When players create Build Orders they tell players what to build at certain supply.
/// </summary>
enum class BuildStepType
{
    Build,               // build a building (unit field)
    Train,               // train a unit
    ScoutWorker,         // call commanderReference->getUnitToScout()
};

/// <summary>
/// Represent the instruction ProtoBot should perform for a BuildOrder step.
/// </summary>
struct BuildOrderStep
{
    BuildStepType type = BuildStepType::Build;
    BWAPI::UnitType unit = BWAPI::UnitTypes::None;
    int count = 1;                                 
    BuildTrigger trigger;
};

/// <summary>
/// BuildOrders are a similar line of thinking to opening you would see in chess. ProtoBot emulates oepnings that proffesional StarCraft players and the community have refined over the course of the games lifetime.
/// \n
/// \nProtoBot performs 2 opening against all 3 races
/// \nProtoss - 2 Gateway Dark Templar
/// \nTerran - 2 Gateway Dark Templar
/// \nZerg - 2 Gateway Observer
/// \n\n
/// To see other oepning that have been created you can view them in the link below:
/// \n https://liquipedia.net/starcraft/Protoss_Strategy
/// </summary>
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
