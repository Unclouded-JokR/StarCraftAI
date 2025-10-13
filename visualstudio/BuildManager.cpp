#include "BuildManager.h"
#include "../src/starterbot/Tools.h"
#include "../src/starterbot/MapTools.h"

// constructor for BuildManager
BuildManager::BuildManager()
{
    
}

void BuildManager::onStart()
{

}

void BuildManager::onFrame()
{
    const BWAPI::UnitType workerType = BWAPI::Broodwar->self()->getRace().getWorker();
    int workersOwned = Tools::CountUnitsOfType(workerType, BWAPI::Broodwar->self()->getUnits());
    BWAPI::Unitset myUnits = BWAPI::Broodwar->self()->getUnits();
    switch (workersOwned)
    {
        case 10:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Gateway);
    break;
        case 12:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Assimilator);
    break;
        case 14:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
    break;
        case 17:
	//gateway->train(BWAPI::UnitTypes::Protoss_Zealot);}   
    break;
         case 25:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Robotics_Facility);
    break;
        case 29:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Gateway);
    break;
        case 38:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Observatory);
    break;
    }
}
