#pragma once
#include <BWAPI.h>
#include "SquadStateTypes.h"
#include "SquadState.h"
#include "A-StarPathfinding.h"
#include "math.h"
#include "VectorPos.h"

//#define DEBUG_SQUAD
//#define DEBUG_FLOCKING
#define DISPLAY_STATES
#define MAX_SQUAD_SIZE 4

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
	SquadState* currentState = nullptr;

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
	static map<SquadState*, BWAPI::Color> stateColorMap;

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
};

class SharedSquad{
public:
	vector<Squad*> Squads;
	map<Squad*, SquadInfo> savedSquadInfoMap;

	SharedSquad() {
		this->Squads = vector<Squad*>();
		this->savedSquadInfoMap = map<Squad*, SquadInfo>();
	}

	SharedSquad(Squad* initialSquad) {
		this->Squads.push_back(initialSquad);
		this->savedSquadInfoMap[initialSquad] = initialSquad->info;
	}

	void onFrame();
	BWAPI::Position getPosition();
private:
	void kitingMelee(BWAPI::Unit);
	void kitingRanged(BWAPI::Unit);
};