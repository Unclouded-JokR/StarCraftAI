#include "BuildingPlacer.h"
#include "ProtoBotCommander.h"
#include "BuildManager.h"
#include "Builder.h"

BuildingPlacer::BuildingPlacer(BuildManager* buildManagerReference) : buildManagerReference(buildManagerReference)
{

}

//[TODO] changine this to check the tiles being reserved instead of the builders.
bool BuildingPlacer::alreadyUsingTiles(BWAPI::TilePosition positon)
{
    for (const Builder& builder : buildManagerReference->getBuilders())
    {
        if (builder.positionToBuild == BWAPI::Position(positon)) return true;
    }

    return false;
}

BWAPI::Position BuildingPlacer::getPositionToBuild(BWAPI::UnitType type)
{
    if (type == BWAPI::UnitTypes::Protoss_Nexus)
    {
        int distance = INT_MAX;
        BWAPI::TilePosition closestDistance;

        for (const BWEM::Area& area : theMap.Areas())
        {
            for (const BWEM::Base& base : area.Bases())
            {
                if (BWAPI::Broodwar->self()->getStartLocation().getApproxDistance(base.Location()) < distance
                    && base.Location() != BWAPI::Broodwar->self()->getStartLocation()
                    && BWAPI::Broodwar->canBuildHere(base.Location(), type))
                {
                    distance = BWAPI::Broodwar->self()->getStartLocation().getApproxDistance(base.Location());
                    closestDistance = base.Location();
                }
            }
        }

        //std::cout << "Closest Location at " << closestDistance.x << ", " << closestDistance.y << "\n";
        return BWAPI::Position(closestDistance);
    }
    else if (type == BWAPI::UnitTypes::Protoss_Assimilator)
    {
        std::vector<NexusEconomy> nexusEconomies = buildManagerReference->getNexusEconomies();

        for (const NexusEconomy& nexusEconomy : nexusEconomies)
        {
            if (nexusEconomy.vespeneGyser != nullptr && nexusEconomy.assimilator == nullptr)
            {
                //std::cout << "Nexus " << nexusEconomy.nexusID << " needs assimilator\n";
                //std::cout << "Gyser position = " << nexusEconomy.vespeneGyser->getPosition() << "\n";

                BWAPI::Position position = BWAPI::Position(nexusEconomy.vespeneGyser->getPosition().x - 55, nexusEconomy.vespeneGyser->getPosition().y - 25);
                return position;
            }
        }
    }
    else
    {
        int distance = INT_MAX;
        BWAPI::TilePosition closestDistance;
        std::vector<BWEB::Block> blocks = BWEB::Blocks::getBlocks();

        for (BWEB::Block block : blocks)
        {
            std::set<BWAPI::TilePosition> placements = block.getPlacements(type);

            for (const BWAPI::TilePosition placement : placements)
            {
                const int distanceToPlacement = BWAPI::Broodwar->self()->getStartLocation().getApproxDistance(placement);

                if (BWAPI::Broodwar->canBuildHere(placement, type)
                    && distanceToPlacement < distance
                    && !alreadyUsingTiles(placement))
                {
                    distance = distanceToPlacement;
                    closestDistance = placement;
                }
            }
        }

        //std::cout << distance << "\n";
        return BWAPI::Position(closestDistance);
    }

    return BWAPI::Position(0, 0);
}