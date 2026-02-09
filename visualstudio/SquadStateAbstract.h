#pragma once
#include "Squad.h"

class Squad;

class SquadStateAbstract {
public:
	virtual void Enter(Squad* squad) = 0;
	virtual void Update(Squad* squad) = 0;
	virtual void Exit(Squad* squad) = 0;
	virtual ~SquadStateAbstract() {};
};