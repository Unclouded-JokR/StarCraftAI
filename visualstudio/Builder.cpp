#include "Builder.h"
#include "BuildManager.h"
#include <cmath>

Builder::Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToConstruct, BWAPI::Position positionToBuild, Path path) :
	unitReference(unitReference), 
	buildingToConstruct(buildingToConstruct),
	requestedPositionToBuild(positionToBuild),
	requestedTileToBuild(BWAPI::TilePosition(positionToBuild)),
	referencePath(path)
{
	/*std::cout << unitReference->getID() << "\n";
	std::cout << buildingToConstruct.getName() << "\n";
	std::cout << positionToBuild.x << ", " << positionToBuild.y << "\n";*/
	/*if (buildingToConstruct == BWAPI::UnitTypes::Protoss_Assimilator)
	{
		unitReference->build(buildingToConstruct, BWAPI::TilePosition(requestedPositionToBuild));
	}
	else
	{
		referencePath = AStar::GeneratePath(unitReference->getPosition(), unitReference->getType(), positionToBuild);
		path = referencePath.positions;
	}*/

	std::cout << path.positions.size() << "\n";
}

Builder::~Builder() 
{
	//path.clear();
}

void Builder::onFrame()
{
	if(referencePath.positions.empty() == false)
		AStar::drawPath(referencePath);

    // --- Timeout / progress tracking ---
    framesSinceAssigned++;

    const double dist = unitReference->getDistance(requestedPositionToBuild);

    if (framesSinceAssigned == 1)
    {
        lastDistance = dist;
        lastProgressFrame = 0;
    }
    else
    {
        if (framesSinceAssigned - lastProgressFrame >= progressCheckWindow)
        {
            const double progress = lastDistance - dist; // positive means we got closer
            if (progress < minProgressPixels)
            {
                gaveUp = true;
                return;
            }
            lastDistance = dist;
            lastProgressFrame = framesSinceAssigned;
        }
    }

    if (framesSinceAssigned >= maxFramesToRevealOrReach)
    {
        gaveUp = true;
        return;
    }

    // --- Reveal fog-of-war if needed (unexplored tiles prevent build commands) ---
    auto footprintExplored = [&]() -> bool {
        const int w = buildingToConstruct.tileWidth();
        const int h = buildingToConstruct.tileHeight();
        for (int x = 0; x < w; x++)
        {
            for (int y = 0; y < h; y++)
            {
                const BWAPI::TilePosition t = requestedTileToBuild + BWAPI::TilePosition(x, y);
                if (!BWAPI::Broodwar->isExplored(t)) return false;
            }
        }
        return true;
    };

    if (!footprintExplored())
    {
        // Move to the center of the build tile to reveal terrain, then try building later.
        unitReference->move(BWAPI::Position(requestedTileToBuild) + BWAPI::Position(16, 16));
        return;
    }


	if (buildingToConstruct.isResourceDepot())
	{
		if (unitReference->isIdle())
		{
			unitReference->move(requestedPositionToBuild);
		}
		else if (unitReference->getDistance(requestedPositionToBuild) < 200)
		{
			unitReference->build(buildingToConstruct, BWAPI::TilePosition(requestedPositionToBuild));
		}
	}
	else
	{
		if (pathIndex == referencePath.positions.size() || unitReference->getDistance(requestedPositionToBuild) < DISTANCE_THRESHOLD)
		{
			unitReference->build(buildingToConstruct, BWAPI::TilePosition(requestedPositionToBuild));
		}
		else
		{
			if (unitReference->getDistance(referencePath.positions.at(pathIndex)) < DISTANCE_THRESHOLD)
			{
				if ((pathIndex + 1) != referencePath.positions.size())
				{
					pathIndex++;
					unitReference->move(referencePath.positions.at(pathIndex));
				}
			}
		}
	}
}

BWAPI::Unit Builder::getUnitReference()
{
	return unitReference;
}

void Builder::setUnitReference(BWAPI::Unit unit)
{
	unitReference = unit;
}
