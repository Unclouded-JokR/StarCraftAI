#include "Builder.h"
#include "BuildManager.h"

Builder::Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToConstruct, BWAPI::Position positionToBuild) : 
	unitReference(unitReference), 
	buildingToConstruct(buildingToConstruct),
	requestedPositionToBuild(positionToBuild),
	isConstructing(true),
	initialPosition(unitReference->getPosition())
{
	/*std::cout << unitReference->getID() << "\n";
	std::cout << buildingToConstruct.getName() << "\n";
	std::cout << positionToBuild.x << ", " << positionToBuild.y << "\n";*/
	if (buildingToConstruct == BWAPI::UnitTypes::Protoss_Assimilator)
	{
		unitReference->build(buildingToConstruct, BWAPI::TilePosition(requestedPositionToBuild));
	}
	else
	{
		referencePath = AStar::GeneratePath(unitReference->getPosition(), unitReference->getType(), positionToBuild);
		path = referencePath.positions;
	}

	unitReference->stop();
}

Builder::~Builder() 
{
	path.clear();
}

void Builder::onFrame()
{
	if(referencePath.positions.empty() == false)
		AStar::drawPath(referencePath);

	if (pathIndex == path.size() || unitReference->getDistance(requestedPositionToBuild) < DISTANCE_THRESHOLD)
	{
		unitReference->build(buildingToConstruct, BWAPI::TilePosition(requestedPositionToBuild));
	}
	else
	{
		//First initial move
		if (unitReference->isIdle())
		{
			//std::cout << "First move\n";
			unitReference->move(path.at(pathIndex));
		}

		if (unitReference->getDistance(path.at(pathIndex)) < DISTANCE_THRESHOLD)
		{
			//std::cout << "Moving\n";
			pathIndex++;

			if(pathIndex != path.size())
				unitReference->move(path.at(pathIndex));
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
