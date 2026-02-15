#include "BuildingPlacer.h"
#include "ProtoBotCommander.h"

BuildingPlacer::BuildingPlacer()
{

}

//[TODO] Add a reserve system into the BuildingPlacer since the BWEB's reserve system does not properlly have a reserving system for tiles.
bool BuildingPlacer::alreadyUsingTiles(BWAPI::TilePosition position, int width, int height, bool assimilator = false)
{
    const BWAPI::UnitType typeFound = BWEB::Map::isUsed(position, width, height);

    if (!assimilator)
    {
        if (BWAPI::UnitTypes::None != typeFound) return true;
    }
    else
    {
        if (typeFound == BWAPI::UnitTypes::Protoss_Assimilator || typeFound == BWAPI::UnitTypes::Terran_Refinery || typeFound == BWAPI::UnitTypes::Zerg_Extractor)
        {
            return true;
        }
    }

    return false;
}

//Shouldnt really need this since we are only checking blocks that are powered.
bool BuildingPlacer::checkPower(BWAPI::TilePosition location, BWAPI::UnitType buildingType)
{
    //const int x_location = location.x;
    //const int y_location = location.y;

    //float tilesPowered = 0.0;

    //for (int row = y_location; row < x_location + buildingType.tileHeight(); row++)
    //{
    //    for (int column = x_location; column < x_location + buildingType.tileWidth(); column++)
    //    {
    //        if (poweredTiles[row][column] >= 1) tilesPowered++;
    //    }
    //}

    //const float totalTiles = float(buildingType.tileWidth() * buildingType.tileHeight());

    ////BWEB should have us covered for block placement, since it seems like powering units isnt consistent with the amount of tiles a unit has.
    //if (tilesPowered / totalTiles > .5) return true;

    return false;
}

BWAPI::TilePosition BuildingPlacer::checkBuildingBlocks()
{
    //Power up blocks that are not power reserve blocks.
    int distanceToPowerBlock = INT_MAX;
    BWAPI::TilePosition powerTilePosition = BWAPI::TilePositions::Invalid;

    for (BWEB::Block block : ProtoBot_Blocks)
    {
        BlockData data = Block_Information[block.getTilePosition()];

        if (data.Blocksize != BlockData::SUPPLY && data.Power_State != BlockData::FULLY_POWERED)
        {
            //Loop through all power tiles. Blocks can have more than 1 power tile.
            for (const BWAPI::TilePosition powerPlacements : data.PowerTiles)
            {
                if (BWEB::Map::isUsed(powerPlacements) != BWAPI::UnitTypes::None) continue;

                int distance = 0;
                theMap.GetPath(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), BWAPI::Position(powerPlacements), &distance);

                if (distance == -1) continue;

                if (distance < distanceToPowerBlock)
                {
                    distanceToPowerBlock = distance;
                    powerTilePosition = powerPlacements;
                }
            }
        }
    }

    return powerTilePosition;
}

BWAPI::TilePosition BuildingPlacer::checkPowerReserveBlocks()
{
    int distanceToPowerBlock = INT_MAX;
    BWAPI::TilePosition supplyReservePosition = BWAPI::TilePositions::Invalid;

    for (BWEB::Block block : ProtoBot_Blocks)
    {
        BlockData data = Block_Information[block.getTilePosition()];

        if (data.Blocksize == BlockData::SUPPLY && BWEB::Map::isUsed(block.getTilePosition()) == BWAPI::UnitTypes::None)
        {
            int distance = 0;
            theMap.GetPath(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), BWAPI::Position(block.getTilePosition()), &distance);

            if (distance == -1) continue;

            if (distance < distanceToPowerBlock)
            {
                distanceToPowerBlock = distance;
                supplyReservePosition = block.getTilePosition();
            }
        }
    }

    return supplyReservePosition;
}

BWAPI::TilePosition BuildingPlacer::findAvailableExpansion()
{
    const BWAPI::TilePosition ProtoBot_MainBase = BWAPI::Broodwar->self()->getStartLocation();
    const BWEM::Area* mainArea = theMap.GetArea(ProtoBot_MainBase);

    int distance = INT_MAX;
    BWAPI::TilePosition closestDistance = BWAPI::TilePositions::Invalid;
    const BWAPI::UnitType type = BWAPI::UnitTypes::Protoss_Nexus;

    //This does not cover the case of a building being place on a tile but it should be good enough for now.
    for (const BWEM::Area& area : theMap.Areas())
    {
        for (const BWEM::Base& base : area.Bases())
        {
            if (base.Location() == BWAPI::Broodwar->self()->getStartLocation() || alreadyUsingTiles(base.Location(), type.tileWidth(), type.tileHeight())) continue;

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

    return closestDistance;
}

BWAPI::TilePosition BuildingPlacer::findAvailableGyser()
{
    std::cout << "Checking for open gysers\n";
    BWAPI::TilePosition availableGyser = BWAPI::TilePositions::Invalid;

    for (const BWEM::Area& area : theMap.Areas())
    {
        for (const BWEM::Base& base : area.Bases())
        {
            if (base.Geysers().size() == 0) continue;

            BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRectangle(BWAPI::Position(base.Location()),
                BWAPI::Position(32 * (base.Location().x + (BWAPI::UnitTypes::Protoss_Nexus.tileWidth())), 32 * (base.Location().y + (BWAPI::UnitTypes::Protoss_Nexus.tileHeight()))));

            bool nexusOnLocation = false;

            for (const BWAPI::Unit unit : units)
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus && unit->getPlayer() == BWAPI::Broodwar->self())
                {
                    nexusOnLocation = true;
                    break;
                }
            }

            //We do not own the base.
            if (nexusOnLocation == false) continue;

            std::vector<BWEM::Geyser*> gysers = base.Geysers();

            for (const BWEM::Geyser* gyser : gysers)
            {
                const BWAPI::Unit unit = gyser->Unit();

                if (unit && unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser && 
                    !alreadyUsingTiles(gyser->TopLeft(), unit->getType().tileWidth(), unit->getType().tileHeight(), true))
                {
                    return gyser->TopLeft();
                }
            }
        }
    }

    return availableGyser;
}

//Need to possibly consider case when we have placements but we just dont have power there.
BWAPI::TilePosition BuildingPlacer::findAvaliblePlacement(BWAPI::UnitType type)
{
    int distance = INT_MAX;
    BWAPI::TilePosition closestDistance = BWAPI::TilePositions::Invalid;

    for (BWEB::Block block : ProtoBot_Blocks)
    {
        BlockData& data = Block_Information[block.getTilePosition()];

        if (data.Power_State != BlockData::FULLY_POWERED) continue;

        std::set<BWAPI::TilePosition> placements = block.getPlacements(type);

        for (const BWAPI::TilePosition placement : placements)
        {
            const int distanceToPlacement = BWAPI::Broodwar->self()->getStartLocation().getApproxDistance(placement);

            if (BWAPI::Broodwar->canBuildHere(placement, type)
                && distanceToPlacement < distance
                && !alreadyUsingTiles(placement, type.tileWidth(), type.tileHeight()))
            {
                distance = distanceToPlacement;
                closestDistance = placement;
            }
        }
    }

    return closestDistance;
}

void BuildingPlacer::drawPoweredTiles()
{
    /*for (int row = 0; row < poweredTiles.size(); row++)
    {
        for (int column = 0; column < poweredTiles[row].size(); column++)
        {
            if (poweredTiles[row][column] >= 1) BWAPI::Broodwar->drawCircleMap((column * 32) + 16, (row * 32) + 16, 5, BWAPI::Colors::Blue, true);
        }
    }*/

    for (BWEB::Block block : ProtoBot_Blocks)
    {
        block.draw();

        BlockData data = Block_Information[block.getTilePosition()];

        if (data.Blocksize != BlockData::SUPPLY)
        {
            for (const BWAPI::TilePosition PowerTilePosition : data.PowerTiles)
            {
                BWAPI::Broodwar->drawTextMap(BWAPI::Position((PowerTilePosition.x * 32) + 10, (PowerTilePosition.y * 32) + 16), "Power\nPlacement");
            }
        }
        else
        {
            BWAPI::Broodwar->drawTextMap(BWAPI::Position((block.getTilePosition().x * 32) + 16, (block.getTilePosition().y * 32) + 16), "Supply\nReserve");
        }

        if (data.Power_State == BlockData::FULLY_POWERED)
        {
            BWAPI::Broodwar->drawBoxMap(BWAPI::Position(block.getTilePosition()), BWAPI::Position((block.getTilePosition().x + block.width()) * 32 + 1, (block.getTilePosition().y + block.height()) * 32 + 1), BWAPI::Colors::Blue, false);
        }
    }

}

PlacementInfo BuildingPlacer::getPositionToBuild(BWAPI::UnitType type)
{
    PlacementInfo information;

    switch(type)
    {
        case BWAPI::UnitTypes::Protoss_Nexus:
        {
            const BWAPI::TilePosition pos = findAvailableExpansion();

            if (pos == BWAPI::TilePositions::Invalid)
            {
                information.flag = PlacementInfo::NO_EXPANSIONS;
            }
            else
            {
                information.flag = PlacementInfo::SUCCESS;
                information.position = BWAPI::Position(pos);
            }
        }
        break;
        case BWAPI::UnitTypes::Protoss_Assimilator:
        {
            const BWAPI::TilePosition pos = findAvailableGyser();

            if (pos == BWAPI::TilePositions::Invalid)
            {
                information.flag = PlacementInfo::NO_GYSERS;
            }
            else
            {
                information.flag = PlacementInfo::SUCCESS;
                information.position = BWAPI::Position(pos);
            }
        }
        break;
        case BWAPI::UnitTypes::Protoss_Pylon:
        {
            BWAPI::TilePosition powerTilePosition = checkBuildingBlocks();

            if (powerTilePosition == BWAPI::TilePositions::Invalid)
            {
                powerTilePosition = checkPowerReserveBlocks();

                if (powerTilePosition == BWAPI::TilePositions::Invalid)
                {
                    information.flag = PlacementInfo::NO_BLOCKS;
                }
                else
                {
                    information.position = BWAPI::Position(powerTilePosition);
                    information.flag = PlacementInfo::SUCCESS;
                }
            }
            else
            {
                information.position = BWAPI::Position(powerTilePosition);
                information.flag = PlacementInfo::SUCCESS;
            }
        }
        break;
        default:
        {
            const BWAPI::TilePosition pos = findAvaliblePlacement(type);

            if (pos == BWAPI::TilePositions::Invalid)
            {
                information.flag = PlacementInfo::NO_PLACEMENTS;

            }
            else
            {
                information.flag = PlacementInfo::SUCCESS;
                information.position = BWAPI::Position(pos);
            }
        }
        break;
    }

    return information;
}

//Update values at the start of a game
void BuildingPlacer::onStart()
{
    mapWidth = BWAPI::Broodwar->mapWidth();
    mapHeight = BWAPI::Broodwar->mapHeight();

    //poweredTiles.assign(mapHeight, std::vector<int>(mapWidth, 0));
    Block_Information.clear();
    AreasOccupied.clear();
    ProtoBot_Blocks.clear();
    Area_Blocks.clear();

    //Assign main area since we will always own this area at the start of the game
    const BWAPI::TilePosition ProtoBot_MainBase = BWAPI::Broodwar->self()->getStartLocation();
    const BWEM::Area* mainArea = theMap.GetArea(ProtoBot_MainBase);
    AreasOccupied.insert(mainArea);

    std::vector<BWEB::Block> blocks = BWEB::Blocks::getBlocks();
    int largePlacements = 0;
    int mediumPlacements = 0;
    int smallPlacements = 0;

    for (BWEB::Block block : blocks)
    {
        //Store this information for later use and save it.
        BlockData block_info;

        largePlacements = block.getLargeTiles().size();
        mediumPlacements = block.getMediumTiles().size();
        smallPlacements = block.getSmallTiles().size();

        //std::cout << "Large Placements = " << largePlacements << "\n";
        //std::cout << "Medium Placements = " << mediumPlacements << "\n";
        //std::cout << "Supply Placements = " << smallPlacements << "\n";

        if (largePlacements >= 2)
        {
            block_info.Blocksize = BlockData::LARGE;

            //std::cout << "Adding Large Block\n";
            std::set<BWAPI::TilePosition> powerTiles = block.getSmallTiles();
            block_info.PowerTiles = powerTiles;
            //std::cout << "Block power tiles = " << block_info.PowerTiles.size() << "\n";
        }
        else if (mediumPlacements != 0 || largePlacements == 1)
        {
            block_info.Blocksize = BlockData::MEDIUM;

            //std::cout << "Adding Medium Block\n";
            std::set<BWAPI::TilePosition> powerTiles = block.getSmallTiles();
            block_info.PowerTiles = powerTiles;
            //std::cout << "Block power tiles = " << block_info.PowerTiles.size() << "\n";
        }
        else if (mediumPlacements == 0 && largePlacements == 0 && smallPlacements == 1)
        {
            //std::cout << "Adding Supply block\n";
            block_info.Blocksize = BlockData::SUPPLY;
        }

        block_info.Large_Placements = largePlacements;
        block_info.Medium_Placements = mediumPlacements;
        block_info.Power_Placements = smallPlacements;
        block_info.Block_AreaLocation = theMap.GetArea(block.getTilePosition());

        //std::cout << "Large Placements " << block_info.Large_Placements << "\n";
        //std::cout << "Medium Placements " << block_info.Medium_Placements << "\n";
        //std::cout << "Small/Power Placements " << block_info.Power_Placements << "\n";

        const BWEM::Area* blockArea = theMap.GetNearestArea(block.getTilePosition());

        if (blockArea != nullptr)
        {
            Area_Blocks[blockArea].push_back(block);
        }
        else
        {
            //This should happen but just in case.
            std::cout << "Uh Oh couldnt find Area\n";
        }

        //Might have a bug with corners using this approach but hopefully it will be okay using reserving system
        Block_Information.emplace(block.getTilePosition(), block_info);
    }

    ProtoBot_Blocks.insert(ProtoBot_Blocks.end(), Area_Blocks[mainArea].begin(), Area_Blocks[mainArea].end());
}

void BuildingPlacer::onUnitCreate(BWAPI::Unit unit)
{
    if (!unit->getType().isBuilding()) return;

    BWEB::Map::addUsed(unit->getTilePosition(), unit->getType());
}

void BuildingPlacer::onUnitComplete(BWAPI::Unit unit)
{
    if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        const BWEM::Area* expandedArea = theMap.GetArea(unit->getTilePosition());

        //If the area is the same no new blocks should be added
        if (AreasOccupied.find(expandedArea) != AreasOccupied.end()) return;

        AreasOccupied.insert(expandedArea);

        ProtoBot_Blocks.insert(ProtoBot_Blocks.end(), Area_Blocks[expandedArea].begin(), Area_Blocks[expandedArea].end());
    }
    else if (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        /*const BWAPI::TilePosition location = unit->getTilePosition();
        const int mapX_location = location.x;
        const int mapY_location = location.y;

        //Make looping easier we will think of the area a pylon powers as a square. We will then factor in an offset to then get the proper "circle" shape.
        const int topLeft_x = mapX_location - 7;
        const int topLeft_y = mapY_location - 4;

        for (int poweredRow_index = topLeft_y; poweredRow_index < topLeft_y + PYLON_POWER_HEIGHT; poweredRow_index++)
        {
            for (int poweredColumn_index = topLeft_x; poweredColumn_index < (topLeft_x + PYLON_POWER_WIDTH); poweredColumn_index++)
            {
                if (poweredColumn_index >= mapWidth || poweredColumn_index < 0 || poweredRow_index < 0 || poweredRow_index >= mapHeight) continue;

                poweredTiles[poweredRow_index][poweredColumn_index] += 1;
            }
        }*/

        for (BWEB::Block block : ProtoBot_Blocks)
        {
            BlockData& data = Block_Information[block.getTilePosition()];

            for (const BWAPI::TilePosition powerTile : data.PowerTiles)
            {
                if (unit->getTilePosition() == powerTile)
                {
                    data.Used_Power_Placements++;

                    if (data.Used_Power_Placements == data.PowerTiles.size())
                    {
                        data.Power_State = BlockData::FULLY_POWERED;
                    }
                    else
                    {
                        data.Power_State = BlockData::HALF_POWERED;
                    }

                    break;
                }
            }
        }
    }
}

void BuildingPlacer::onUnitDestroy(BWAPI::Unit unit)
{
    BWEB::Map::onUnitDestroy(unit);

    if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        const BWEM::Area* destroyedNexusArea = theMap.GetArea(unit->getTilePosition());

        //Sanity Check
        if (AreasOccupied.find(destroyedNexusArea) == AreasOccupied.end()) return;

        AreasOccupied.erase(destroyedNexusArea);

        //Make sure this is working correctly.
        for (std::vector<BWEB::Block>::iterator it = ProtoBot_Blocks.begin(); it != ProtoBot_Blocks.end();)
        {
            const BlockData& data = Block_Information[it->getTilePosition()];

            if (data.Block_AreaLocation == destroyedNexusArea)
            {
                it = ProtoBot_Blocks.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
    else if (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        std::cout << "Pylon destroyed at location: " << unit->getTilePosition() << "\n";

        /*const BWAPI::TilePosition location = unit->getTilePosition();
        const int mapX_location = location.x;
        const int mapY_location = location.y;

        //Make looping easier we will think of the area a pylon powers as a square. We will then factor in an offset to then get the proper "circle" shape.
        const int topLeft_x = mapX_location - 7;
        const int topLeft_y = mapY_location - 4;

        for (int poweredRow_index = topLeft_y; poweredRow_index < topLeft_y + PYLON_POWER_HEIGHT; poweredRow_index++)
        {
            for (int poweredColumn_index = topLeft_x; poweredColumn_index < (topLeft_x + PYLON_POWER_WIDTH); poweredColumn_index++)
            {
                if (poweredColumn_index >= mapWidth || poweredColumn_index < 0 || poweredRow_index < 0 || poweredRow_index >= mapHeight) continue;

                poweredTiles[poweredRow_index][poweredColumn_index] -= 1;

                //Sanity check
                if (poweredTiles[poweredRow_index][poweredColumn_index] < 0)
                    poweredTiles[poweredRow_index][poweredColumn_index] = 0;
            }
        }*/

        for (BWEB::Block block : ProtoBot_Blocks)
        {
            BlockData& data = Block_Information[block.getTilePosition()];

            for (const BWAPI::TilePosition powerTile : data.PowerTiles)
            {
                if (unit->getTilePosition() == powerTile)
                {
                    data.Used_Power_Placements--;

                    if (data.Used_Power_Placements == data.PowerTiles.size())
                    {
                        data.Power_State = BlockData::FULLY_POWERED;
                    }
                    else if(data.Used_Power_Placements == data.PowerTiles.size() / 2)
                    {
                        data.Power_State = BlockData::HALF_POWERED;
                    }
                    else
                    {
                        data.Power_State = BlockData::NOT_POWERED;
                    }

                    break;
                }
            }
        }
    }
}

void BuildingPlacer::onUnitMorph(BWAPI::Unit unit)
{
    BWEB::Map::onUnitMorph(unit);
}

void BuildingPlacer::onUnitDiscover(BWAPI::Unit unit)
{
    BWEB::Map::onUnitDiscover(unit);
}