#pragma once
#include <BWAPI.h>
#include <variant>
#include "../src/starterbot/Tools.h"

class ProtoBotCommander;
class SpenderManager;

//Sould change this to be a smart reuqest but for now this works.
struct BuildRequest
{
    std::variant<BWAPI::UnitType, BWAPI::UpgradeType> unitRequest;
};

class BuildManager
{
public:
    ProtoBotCommander* commanderReference;
    SpenderManager spenderManager;
    BWAPI::Unitset buildings;
    bool buildOrderCompleted = false;

    

    BuildManager(ProtoBotCommander* commanderReference);
    void onStart();
    void onFrame();
    void assignBuilding(BWAPI::Unit unit);
    bool isBuildOrderCompleted();
    void buildBuilding(BWAPI::Unit unit, BWAPI::UnitType building);
    void buildUnitType(BWAPI::UnitType unit);
    void buildUpgadeType(BWAPI::UpgradeType upgradeToBuild);
};

class SpenderManager
{
    std::vector<BuildRequest> buildReuqests;
    ProtoBotCommander* commanderReference;

    SpenderManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
    {

    }

    void addRequest(BWAPI::UnitType unitType)
    {
        BuildRequest request;
        request.unitRequest = unitType;

        buildReuqests.push_back(request);
    }

    bool canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas)
    {
        if (((currentMinerals - mineralPrice) >= 0) && ((currentGas - gasPrice) >= 0))
        {
            return true;
        }
        
        return false;
    }

    void OnFrame()
    {
        int currentMineralCount = BWAPI::Broodwar->self()->minerals();
        int currentGasCount = BWAPI::Broodwar->self()->gas();

        int mineralPrice = 0;
        int gasPrice = 0;

        for(std::vector<BuildRequest>::iterator it = buildReuqests.begin(); it < buildReuqests.end(); ++it)
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
};