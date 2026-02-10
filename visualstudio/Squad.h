#pragma once
#include <BWAPI.h>
#include "SquadState.h"
#include "A-StarPathfinding.h"
#include "math.h"
#include "VectorPos.h"

class Squad {
public:
	int squadId;
	BWAPI::Color squadColor;
	int unitSize;
	BWAPI::Unit leader;
	std::vector<BWAPI::Unit> units;
	Path currentPath;
	int currentPathIdx = 0;
	double minNDistance = 80;
	double minSepDistance = 30;

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
	double getMagnitude(BWAPI::Position vector);
	VectorPos normalize(VectorPos vector);
	void drawDebugInfo();
};