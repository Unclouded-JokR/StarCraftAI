#include "BuildOrder.h"

namespace BuildOrders
{
    std::vector<BuildOrder> createAll()
    {
        std::vector<BuildOrder> buildOrders;

        // 1) 2 Gateway Observer vs Terran
        {
            BuildOrder bo;
            bo.name = 1;
            bo.id = 1;
            bo.vsRace = BWAPI::Races::Terran;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 14} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 15} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 16} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 21} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Robotics_Facility, 1, {BuildTriggerType::AtSupply, 25} });
            //bo.steps.push_back({ BuildStepType::NaturalWall, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 27} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 29} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 31} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Observatory, 1, {BuildTriggerType::AtSupply, 38} });

            buildOrders.push_back(std::move(bo));
        }

        // 2) 3 Gate Robo vs Protoss
        {
            BuildOrder bo;
            bo.name = 2;
            bo.id = 2;
            bo.vsRace = BWAPI::Races::Protoss;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 14} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 16} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 20} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 21} });
            //bo.steps.push_back({ BuildStepType::NaturalWall, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 23} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Robotics_Facility, 1, {BuildTriggerType::AtSupply, 26} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 29} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 29} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Observatory, 1, {BuildTriggerType::AtSupply, 33} });

            buildOrders.push_back(std::move(bo));
        }

        // 3) 9/9 Proxy Gateways vs Zerg
        {
            BuildOrder bo;
            bo.name = 3;
            bo.id = 3;
            bo.vsRace = BWAPI::Races::Zerg;

            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 7} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });          
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 13} });
            //bo.steps.push_back({ BuildStepType::NaturalWall, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 16} });

            buildOrders.push_back(std::move(bo));
        }

        // 4) 9/9 Gateways vs Protoss
        {
            BuildOrder bo;
            bo.name = 4;
            bo.id = 4;
            bo.vsRace = BWAPI::Races::Protoss;

            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 13} });
            //bo.steps.push_back({ BuildStepType::NaturalWall, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 16} });

            buildOrders.push_back(std::move(bo));
        }

        // 5) 10/12 Gateways vs Zerg
        {
            BuildOrder bo;
			bo.name = 5;
            bo.vsRace = BWAPI::Races::Protoss;

            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 15} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 21} });
            //bo.steps.push_back({ BuildStepType::NaturalWall, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 23} });

            buildOrders.push_back(std::move(bo));
        }
        return buildOrders;
    }
}
