#include "ProtoBotCommander.h"

void ProtoBotCommander::onStart()
{
	ProtoBotCommander::strategyManager.onStart();
}

void ProtoBotCommander::onFrame()
{
	ProtoBotCommander::strategyManager.onFrame();
}

void ProtoBotCommander::onEnd(bool isWinner)
{
	std::cout << "We " << (isWinner ? "won!" : "lost!") << "\n";
}

void ProtoBotCommander::onUnitDestroy(BWAPI::Unit unit)
{
	ProtoBotCommander::strategyManager.onStart();
}

void ProtoBotCommander::onUnitCreate(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{

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