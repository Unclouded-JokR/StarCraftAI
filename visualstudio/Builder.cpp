#include "Builder.h"
#include "BuildManager.h"

Builder::Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToConstruct) : 
	unitReference(unitReference), 
	buildingToConstruct(buildingToConstruct)
{

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
