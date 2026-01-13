#pragma once
#include <BWAPI.h>
#include "A-StarPathfinding.h"
#include "../src/starterbot/Tools.h"
#include "math.h"

enum State {
	ATTACK, DEFEND, RETREAT, IDLE, POSITIONING
};

class Squad {
public:
	int squadId;
	BWAPI::Color squadColor;
	int unitSize;
	BWAPI::Unit leader;
	std::vector<BWAPI::Unit> units;
	State state;
	double minNDistance = 30;

	Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor, int unitSize);

	bool operator==(const Squad& other) noexcept(true){
		return this->squadId == other.squadId;
	}
	bool operator!=(const Squad& other) noexcept(true) {
		return !(*this == other);
	}

	void onFrame();
	void flockingHandler(BWAPI::Position leaderDest);
	void removeUnit(BWAPI::Unit unit);
	void move(BWAPI::Position position);
	void kitingMove(BWAPI::Unit unit, BWAPI::Position position);
	void attackUnit(BWAPI::Unit unit, BWAPI::Unit target);
	void kitingAttack(BWAPI::Unit unit, BWAPI::Unit target);
	void addUnit(BWAPI::Unit unit);

	// Utility functions
	double getMagnitude(BWAPI::Position vector);
	void drawDebugInfo();
};