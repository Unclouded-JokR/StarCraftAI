#include "BuildOrder.h"

namespace BuildOrders
{
    std::vector<BuildOrder> createAll()
    {
        std::vector<BuildOrder> buildOrders;
        // Note: For build orders that end before placing a 3rd pylon, use BuildManager::findNaturalRampPlacement to get the custom TilePosition
        // 1) 2 Gateway Observer vs Terran
        {
            BuildOrder bo;
            bo.name = 1;
            bo.id = 1;
            bo.vsRace = BWAPI::Races::Terran;
            bo.blockUnitTrainingUntilSupply = 18;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 14} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 15} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 16} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 17} });
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
            bo.blockUnitTrainingUntilSupply = 30;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 14} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 14} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 16} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 18} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 20} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 21} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 22} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Robotics_Facility, 1, {BuildTriggerType::AtSupply, 26} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 29} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 29} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 29} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Observatory, 1, {BuildTriggerType::AtSupply, 33} });

            buildOrders.push_back(std::move(bo));
        }

        // 3) 9/9 Proxy Gateways vs Zerg
        {
            BuildOrder bo;
            bo.name = 3;
            bo.id = 3;
            bo.vsRace = BWAPI::Races::Zerg;
            bo.blockUnitTrainingUntilSupply = 15;

            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 7} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });          
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 11} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 15} });

            buildOrders.push_back(std::move(bo));
        }

        // 4) 12 Nexus vs. Terran
        {
            BuildOrder bo;
			bo.name = 4;
            bo.vsRace = BWAPI::Races::Terran;
            bo.blockUnitTrainingUntilSupply = 17;

            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Nexus, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 14} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 15} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 15} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 17} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 17} });

            buildOrders.push_back(std::move(bo));
        }

        // 5) 28 Nexus vs. Terran
        {
            BuildOrder bo;
			bo.name = 5;
            bo.vsRace = BWAPI::Races::Terran;
            bo.blockUnitTrainingUntilSupply = 26;

            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 11} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 15} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 16} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 21} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 24} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 25} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Nexus, 1, {BuildTriggerType::AtSupply, 28} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Robotics_Facility, 1, {BuildTriggerType::AtSupply, 29} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 32} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 33} });

            buildOrders.push_back(std::move(bo));
        }

        // 6) 9/9 Gateways vs. Zerg
        {
            BuildOrder bo;
			bo.name = 6;
            bo.vsRace = BWAPI::Races::Zerg;
            bo.blockUnitTrainingUntilSupply = 15;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 11} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 15} });

            buildOrders.push_back(std::move(bo));
        }

        // 7) 9/10 Gateways vs. Zerg
        {
            BuildOrder bo;
			bo.name = 7;
            bo.vsRace = BWAPI::Races::Zerg;
            bo.blockUnitTrainingUntilSupply = 15;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 11} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 15} });

            buildOrders.push_back(std::move(bo));
        }

        // 8) 10/12 Gateways vs. Zerg
        {
            BuildOrder bo;
			bo.name = 8;
            bo.vsRace = BWAPI::Races::Zerg;
            bo.blockUnitTrainingUntilSupply = 20;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 15} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 17} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 19} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 21} });

            buildOrders.push_back(std::move(bo));
        }

        
        // 9) 9/9 Gateways vs. Protoss
        {
            BuildOrder bo;
			bo.name = 9;
            bo.vsRace = BWAPI::Races::Protoss;
            bo.blockUnitTrainingUntilSupply = 15;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 2, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 11} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 15} });

            buildOrders.push_back(std::move(bo));
        }

        // 10) 9/10 Gateways vs. Protoss
        {
            BuildOrder bo;
			bo.name = 10;
            bo.vsRace = BWAPI::Races::Protoss;
            bo.blockUnitTrainingUntilSupply = 15;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 9} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 11} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 15} });

            buildOrders.push_back(std::move(bo));
        }

        // 11) 10/12 Gateways vs. Protoss
        {
            BuildOrder bo;
			bo.name = 11;
            bo.vsRace = BWAPI::Races::Protoss;
            bo.blockUnitTrainingUntilSupply = 14;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 16} });

            buildOrders.push_back(std::move(bo));
        }

        // 12) 4 Gate Goon vs. Protoss
        {
            BuildOrder bo;
			bo.name = 12;
            bo.vsRace = BWAPI::Races::Protoss;
            bo.blockUnitTrainingUntilSupply = 41;

            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 8} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::ScoutWorker, BWAPI::UnitTypes::None, 1, {BuildTriggerType::AtSupply, 10} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 12} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 13} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Assimilator, 1, {BuildTriggerType::AtSupply, 16} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, {BuildTriggerType::AtSupply, 17} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Zealot, 1, {BuildTriggerType::AtSupply, 18} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 22} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 23} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 27} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Gateway, 3, {BuildTriggerType::AtSupply, 31} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 1, {BuildTriggerType::AtSupply, 31} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 33} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 4, {BuildTriggerType::AtSupply, 33} });
            bo.steps.push_back({ BuildStepType::Build, BWAPI::UnitTypes::Protoss_Pylon, 1, {BuildTriggerType::AtSupply, 41} });
            bo.steps.push_back({ BuildStepType::Train, BWAPI::UnitTypes::Protoss_Dragoon, 4, {BuildTriggerType::AtSupply, 41} });

            buildOrders.push_back(std::move(bo));
        }
        return buildOrders;
    }
}
