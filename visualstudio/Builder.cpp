#include "Builder.h"
#include "BuildManager.h"

Builder::Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToConstruct, BWAPI::Position positionToBuild, Path path) :
	unitReference(unitReference), 
	buildingToConstruct(buildingToConstruct),
	requestedPositionToBuild(positionToBuild),
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
