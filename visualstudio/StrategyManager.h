#pragma once

#include <BWAPI.h>

class StrategyManager
{
public:
	StrategyManager();

	float boredomPerSecond = 0.0001f;
	float angerFromUnitDeath = .005f;
	float egoFromEnemyUnitDeath = .01f;
	void onStart();
	void onFrame();
	void onUnitDestroy(BWAPI::Unit unit);
	void onEnd(bool isWinner);
	//add evaluate position function to add to ego for when we are in a stronger position.

private:
	//State currentState; //set to idle state
};

//class State
//{
//public:
//	virtual void enter();
//	virtual void exit();
//	virtual void evaluate();
//};

//Rage
//Content
//Denial
//Ego
//Angry
