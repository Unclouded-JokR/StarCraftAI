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
	double minNDistance = 40;
	Path currentPath;
	int currentPathIdx = 0;

	Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor, int unitSize);

	bool operator==(const Squad& other) noexcept(true){
		return this->squadId == other.squadId;
	}
	bool operator!=(const Squad& other) noexcept(true) {
		return !(*this == other);
	}

	void onFrame();
	void simpleFlock();
	void flockingHandler();
	void pathHandler();
	void removeUnit(BWAPI::Unit unit);
	void move(BWAPI::Position position);
	void kitingMove(BWAPI::Unit unit, BWAPI::Position position);
	void attackUnit(BWAPI::Unit unit, BWAPI::Unit target);
	void kitingAttack(BWAPI::Unit unit, BWAPI::Unit target);
	void addUnit(BWAPI::Unit unit);

	// Utility functions
	void drawCurrentPath();
	double getMagnitude(BWAPI::Position vector);
	BWAPI::Position normalize(BWAPI::Position vector);
	void drawDebugInfo();
};