#pragma once
#include <BWAPI.h>
#include "SquadStateTypes.h"
#include "SquadState.h"
#include "A-StarPathfinding.h"
#include "math.h"
#include "VectorPos.h"

//#define DEBUG_SQUAD
//#define DEBUG_FLOCKING
#define MAX_SQUAD_SIZE 12

class SquadState;

class SquadInfo {
public:
	int squadId;
	BWAPI::Color squadColor;
	std::vector<BWAPI::Unit> units;
	BWAPI::Position commandPos;
	BWAPI::Position currentDefensivePosition;
	BWAPI::Position currentReinforcePosition;
	BWAPI::Position currentAttackPosition;
	BWAPI::Position kitePos;
	Path currentPath;
	int currentPathIdx;
	SquadState* currentState;

	SquadInfo() {
		squadId = 0;
		BWAPI::Position commandPos = BWAPI::Positions::Invalid;
		BWAPI::Position currentDefensivePosition = BWAPI::Positions::Invalid;
		BWAPI::Position currentReinforcePosition = BWAPI::Positions::Invalid;
		BWAPI::Position currentAttackPosition = BWAPI::Positions::Invalid;
		BWAPI::Position kitePos = BWAPI::Positions::Invalid;

		Path currentPath = Path();
		int currentPathIdx = 0;
		SquadState* currentState = nullptr;
	}
};

class Squad {
public:
	BWAPI::Unit leader;
	SquadInfo info;

	Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor);

	void onFrame();
	void setState(SquadState& newState);
	void removeUnit(BWAPI::Unit unit);
	void addUnit(BWAPI::Unit unit);
	void drawDebugInfo();

	bool operator==(const Squad& other) noexcept(true) {
		return this->info.squadId == other.info.squadId;
	}
	bool operator!=(const Squad& other) noexcept(true) {
		return !(*this == other);
	}

private:
	void pathHandler();
	void kitingMove(BWAPI::Unit unit, BWAPI::Position position);
	void attackUnit(BWAPI::Unit unit, BWAPI::Unit target);
	void kitingAttack(BWAPI::Unit unit, BWAPI::Unit target);
};

class SharedSquad {
	vector<Squad*> Squads;
	map<Squad*, SquadInfo> savedSquadInfoMap;
};