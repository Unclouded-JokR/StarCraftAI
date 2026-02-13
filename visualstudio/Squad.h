#pragma once
#include <BWAPI.h>
#include "SquadStateTypes.h"
#include "SquadState.h"
#include "A-StarPathfinding.h"
#include "math.h"
#include "VectorPos.h"

#define MAX_SQUAD_SIZE 12
#define MIN_NEIGHBOUR_DISTANCE 80
#define MIN_SEPARATION_DISTANCE 30

class SquadState;

class Squad {
public:
	int squadId;
	BWAPI::Color squadColor;
	BWAPI::Unit leader;
	std::vector<BWAPI::Unit> units;
	BWAPI::Position commandPos;
	BWAPI::Position kitePos;
	Path currentPath = Path();
	int currentPathIdx = 0;

	Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor);

	void onFrame();
	void setState(SquadState& newState);
	void removeUnit(BWAPI::Unit unit);
	void addUnit(BWAPI::Unit unit);
	void drawDebugInfo();

	bool operator==(const Squad& other) noexcept(true) {
		return this->squadId == other.squadId;
	}
	bool operator!=(const Squad& other) noexcept(true) {
		return !(*this == other);
	}

private:
	SquadState* currentState = nullptr;

	void simpleFlock();
	void flockingHandler();
	void pathHandler();
	void kitingMove(BWAPI::Unit unit, BWAPI::Position position);
	void attackUnit(BWAPI::Unit unit, BWAPI::Unit target);
	void kitingAttack(BWAPI::Unit unit, BWAPI::Unit target);
	double getMagnitude(BWAPI::Position vector);
	VectorPos normalize(VectorPos vector);
};