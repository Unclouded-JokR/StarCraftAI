#include "BuildingPlacer.h"
#include "ProtoBotCommander.h"

BuildingPlacer::BuildingPlacer()
{

}

//[TODO] Add a reserve system into the BuildingPlacer since the BWEB's reserve system does not properlly have a reserving system for tiles.
bool BuildingPlacer::alreadyUsingTiles(BWAPI::TilePosition position, int width, int height)
{
    const BWAPI::UnitType typeFound = BWEB::Map::isUsed(position, width, height);

    if (BWAPI::UnitTypes::None != typeFound)
    {
        return true;
    }

    return false;
}

BWAPI::Position BuildingPlacer::getPositionToBuild(BWAPI::UnitType type)
{
    if (type == BWAPI::UnitTypes::Protoss_Nexus)
    {
        const BWAPI::TilePosition ProtoBot_MainBase = BWAPI::Broodwar->self()->getStartLocation();
        const BWEM::Area* mainArea = theMap.GetArea(ProtoBot_MainBase);

        int distance = INT_MAX;
        BWAPI::TilePosition closestDistance;

        //To Avoid expanding to bases father than expected and generating paths for every base. Create a sorted list.
        for (const BWEM::Area& area : theMap.Areas())
        {
            for (const BWEM::Base& base : area.Bases())
            {
                if (base.Location() == BWAPI::Broodwar->self()->getStartLocation()) continue;

                int distanceToNewBase = 0;
                const BWEM::CPPath pathToExpansion = theMap.GetPath(BWAPI::Position(ProtoBot_MainBase), BWAPI::Position(base.Location()), &distanceToNewBase);

                if (distanceToNewBase == -1) continue;

                if (distanceToNewBase < distance)
                {
                    distance = distanceToNewBase;
                    closestDistance = base.Location();
                }
            }
        }

        //std::cout << "Closest Location at " << closestDistance.x << ", " << closestDistance.y << "\n";
        //BWEB::Map::addReserve(closestDistance, BWAPI::UnitTypes::Protoss_Nexus.tileWidth(), BWAPI::UnitTypes::Protoss_Nexus.tileHeight());
        return BWAPI::Position(closestDistance);
    }
    else if (type == BWAPI::UnitTypes::Protoss_Assimilator)
    {
        for (const BWEM::Area& area : theMap.Areas())
        {
            for (const BWEM::Base& base : area.Bases())
            {
                BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRectangle(BWAPI::Position(base.Location()), 
                    BWAPI::Position(32 * (base.Location().x + (BWAPI::UnitTypes::Protoss_Nexus.tileWidth())), 32 * (base.Location().y + (BWAPI::UnitTypes::Protoss_Nexus.tileHeight()))));
                bool nexusOnLocation = false;

                /*BWAPI::Broodwar->drawBoxMap(
                    base.Location().x * 32,
                    base.Location().y * 32,
                    base.Location().x * 32 + (BWAPI::UnitTypes::Protoss_Nexus.tileWidth() * 32),
                    base.Location().y * 32 + (BWAPI::UnitTypes::Protoss_Nexus.tileHeight() * 32),
                    BWAPI::Color::Color(255, 0, 0));*/

                for (BWAPI::Unit unit : units)
                {
                    if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus && unit->getPlayer() == BWAPI::Broodwar->self())
                    {
                        nexusOnLocation = true;
                        break;
                    }
                }

                if (nexusOnLocation == false) continue;

                bool gyserAvalible = false;
                auto gysers = base.Geysers();

                for (BWEM::Geyser* gyser : gysers)
                {
                    units = BWAPI::Broodwar->getUnitsInRectangle(gyser->Pos(),
                        BWAPI::Position(32 * (gyser->Pos().x + BWAPI::UnitTypes::Resource_Vespene_Geyser.tileWidth()), 32 * (gyser->Pos().y + BWAPI::UnitTypes::Resource_Vespene_Geyser.tileHeight())));

                    for (BWAPI::Unit unit : units)
                    {
                        if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
                        {
                            gyserAvalible = true;
                            //BWEB::Map::addReserve(gyser->TopLeft(), BWAPI::UnitTypes::Resource_Vespene_Geyser.tileWidth(), BWAPI::UnitTypes::Resource_Vespene_Geyser.tileHeight());
                            return BWAPI::Position(gyser->TopLeft());
                            break;
                        }
                    }
                }
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
                    && !alreadyUsingTiles(placement, type.tileWidth(), type.tileHeight()))
                {
                    //std::cout << "Found position to place\n";
                    distance = distanceToPlacement;
                    closestDistance = placement;
                }
            }
        }

        //BWEB::Map::addReserve(closestDistance, type.tileWidth(), type.tileHeight());
        return BWAPI::Position(closestDistance);
    }

    return BWAPI::Position(0, 0);
}

void BuildingPlacer::onUnitCreate(BWAPI::Unit unit)
{
    BWEB::Map::addUsed(unit->getTilePosition(), unit->getType());
}

void BuildingPlacer::onUnitDestroy(BWAPI::Unit unit)
{
    BWEB::Map::onUnitDestroy(unit);
}

void BuildingPlacer::onUnitMorph(BWAPI::Unit unit)
{
    BWEB::Map::onUnitMorph(unit);
}

void BuildingPlacer::onUnitDiscover(BWAPI::Unit unit)
{
    BWEB::Map::onUnitDiscover(unit);
}