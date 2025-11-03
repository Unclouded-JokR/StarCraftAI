#include "ProtoBotCommander.h"
#include "SpenderManager.h"

SpenderManager::SpenderManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
    std::cout << "Spender Manager Made!" << "\n";
}

void SpenderManager::addRequest(BWAPI::UnitType unitType)
{
    BuildRequest request;
    request.unitRequest = unitType;

    buildReuqests.push_back(request);
}

bool SpenderManager::canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas)
{
    if (((currentMinerals - mineralPrice) >= 0) && ((currentGas - gasPrice) >= 0))
    {
        return true;
    }

    return false;
}

void SpenderManager::OnFrame()
{
    int currentMineralCount = BWAPI::Broodwar->self()->minerals();
    int currentGasCount = BWAPI::Broodwar->self()->gas();

    int mineralPrice = 0;
    int gasPrice = 0;

    for (std::vector<BuildRequest>::iterator it = buildReuqests.begin(); it < buildReuqests.end(); ++it)
    {
        if (holds_alternative<BWAPI::UnitType>(it->unitRequest))
        {
            BWAPI::UnitType temp = get<BWAPI::UnitType>(it->unitRequest);
            mineralPrice = temp.mineralPrice();
            gasPrice = temp.gasPrice();

            if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
            {
                BWAPI::Unit unitAvalible = commanderReference->getUnitToBuild();
                Tools::BuildBuilding(unitAvalible, temp);

                currentMineralCount -= mineralPrice;
                currentGasCount -= gasPrice;
                buildReuqests.erase(it);
            }
        }
    }

    //for (BuildRequest& buildRequest : buildReuqests)
    //{
    //    if (holds_alternative<BWAPI::UnitType>(buildRequest.unitRequest))
    //    {
    //        BWAPI::UnitType temp = get<BWAPI::UnitType>(buildRequest.unitRequest);
    //        mineralPrice = temp.mineralPrice();
    //        gasPrice = temp.gasPrice();

    //        if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
    //        {
    //            BWAPI::Unit unitAvalible = commanderReference->getUnitToBuild();
    //            Tools::BuildBuilding(unitAvalible, temp);
    //            buildReuqests.erase(buildReuqests.begin());
    //        }
    //    }
    //    /*else if (holds_alternative<BWAPI::UpgradeType>(buildRequest.unitRequest))
    //    {
    //        BWAPI::UpgradeType temp = get<BWAPI::UpgradeType>(buildRequest.unitRequest);
    //        mineralPrice = temp.mineralPrice();
    //        gasPrice = temp.gasPrice();

    //        if (canAfford(mineralPrice, gasPrice, currentMineralCount, currentGasCount))
    //        {
    //            BWAPI::Unit unitAvalible = commanderReference->getUnitToBuild();
    //            Tools::BuildBuilding(unitAvalible, temp);
    //            buildReuqests.erase(buildReuqests.begin());
    //        }
    //    }*/
    //}
}