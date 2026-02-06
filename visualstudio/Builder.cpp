#include "Builder.h"
#include "BuildManager.h"

Builder::Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToConstruct, BWAPI::Position positionToBuild) : 
	unitReference(unitReference), 
	buildingToConstruct(buildingToConstruct),
	positionToBuild(positionToBuild)
{
	std::cout << unitReference->getID() << "\n";
	std::cout << buildingToConstruct.getName() << "\n";
	std::cout << positionToBuild.x << ", " << positionToBuild.y << "\n";

	auto temp = AStar::GeneratePath(unitReference->getPosition(), unitReference->getType(), positionToBuild, true);
	std::cout << temp.positions.size() << "\n";

	//Currently AStar is not providing path.
	for (auto position : temp.positions)
	{
		std::cout << position.x << "," << position.y << "\n";
	}
}

Builder::~Builder() 
{
	path.clear();
}

void Builder::onFrame()
{

}

BWAPI::Unit Builder::getUnitReference()
{
	return unitReference;
}

void Builder::setUnitReference(BWAPI::Unit unit)
{
	unitReference = unit;
}
