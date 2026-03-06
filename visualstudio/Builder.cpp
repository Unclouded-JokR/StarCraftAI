#include "Builder.h"
#include "BuildManager.h"

Builder::Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToConstruct, BWAPI::Position positionToBuild, Path path) :
	unitReference(unitReference), 
	buildingToConstruct(buildingToConstruct),
	requestedPositionToBuild(positionToBuild),
	referencePath(path)
{
	if (referencePath.positions.empty() && debug) std::cout << "Path is empty to place " << buildingToConstruct << " at " << positionToBuild << "\n";

	unitReference->stop();
}

Builder::~Builder() 
{
	
}

void Builder::onFrame()
{
	if(referencePath.positions.empty() == false)
		AStar::drawPath(referencePath);

	if (referencePath.positions.empty())
	{
		if (unitReference->getDistance(requestedPositionToBuild) < CONSTRUCT_DISTANCE_THRESHOLD)
		{
			unitReference->build(buildingToConstruct, BWAPI::TilePosition(requestedPositionToBuild));
		}

		if (unitReference->isIdle())
		{
			if (buildingToConstruct.isRefinery())
			{
				unitReference->rightClick(BWAPI::Position(requestedPositionToBuild.x - 32, requestedPositionToBuild.y));
			}
			else
			{
				unitReference->rightClick(requestedPositionToBuild);
			}
		}

	}
	else
	{
		if (pathIndex == referencePath.positions.size() || unitReference->getDistance(requestedPositionToBuild) < CONSTRUCT_DISTANCE_THRESHOLD)
		{
			unitReference->build(buildingToConstruct, BWAPI::TilePosition(requestedPositionToBuild));
			if(debug) BWAPI::Broodwar->drawTextMap(unitReference->getPosition(), "BUILDING");
		}
		else
		{
			if (unitReference->getDistance(referencePath.positions.at(pathIndex)) < PATH_DISTANCE_THRESHOLD)
			{
				if ((pathIndex + 1) != referencePath.positions.size())
				{
					pathIndex++;
					unitReference->rightClick(referencePath.positions.at(pathIndex));
				}
			}

			if (debug) BWAPI::Broodwar->drawTextMap(unitReference->getPosition(), "MOVING");
		}

		//Incase unit gets stuck
		if (unitReference->isIdle()) unitReference->rightClick(referencePath.positions.at(pathIndex));
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

void Builder::updatePath(Path& path)
{
	pathIndex = 0;
	referencePath = path;
}
