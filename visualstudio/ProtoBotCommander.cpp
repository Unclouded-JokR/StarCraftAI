#include "ProtoBotCommander.h"

void ProtoBotCommander::onStart()
{
	ProtoBotCommander::strategyManager.onStart();
}

void ProtoBotCommander::onFrame()
{
	//Might want to pass unit set to all managers in order to not have to loop through units again and again.
	ProtoBotCommander::strategyManager.onFrame();
}

void ProtoBotCommander::onEnd(bool isWinner)
{
	std::cout << "We " << (isWinner ? "won!" : "lost!") << "\n";
}

void ProtoBotCommander::onUnitDestroy(BWAPI::Unit unit)
{
	ProtoBotCommander::strategyManager.onUnitDestroy(unit);
}

void ProtoBotCommander::onUnitCreate(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{
	ProtoBotCommander::unitManager.addUnit(unit);
}

void ProtoBotCommander::onUnitShow(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitHide(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitRenegade(BWAPI::Unit unit)
{

}