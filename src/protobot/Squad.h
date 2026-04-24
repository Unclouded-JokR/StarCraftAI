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
#define MAX_SQUAD_SIZE 12

class SquadState;

/// \brief Contains squad information such as its units, positions, and current state.
/// <summary> 
/// Used to get a squad's information.
/// </summary>
class SquadInfo {
public:
	int squadId;
	BWAPI::Color squadColor;
	std::vector<BWAPI::Unit> units;
	BWAPI::Position commandPos; ///< Initial position sent by StrategyManager. Used to compared against the current attacking, defending , and reinforcing positions to determine if the squad needs to update its position. ///<
	BWAPI::Position currentDefensivePosition;
	BWAPI::Position currentReinforcePosition;
	BWAPI::Position currentAttackPosition;
	BWAPI::Position kitePos;
	Path currentPath;
	int currentPathIdx;
	SquadState* currentState = nullptr; ///< Contains current state of the squad (See SquadState)

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

/// \brief The Squad class represents a group of combat units owned by ProtoBot.
/// <summary>
/// Base squad structure that holds its leader, observer, as well as its methods.
/// \n Information is stored inside of a SquadInfo class.
/// 
/// \n\n Every squad has a leader and methods for adding and removing units.
/// \n Squad behavior is defined by its state and onFrame(), which calls the current state's Update() method.
/// </summary>
class Squad {
public:
	BWAPI::Unit leader = nullptr;
	BWAPI::Unit observer = nullptr;
	SquadInfo info;

	Squad(BWAPI::Unit leader, int squadId, BWAPI::Color squadColor);

	void onFrame();
	void setState(SquadState& newState);
	void removeUnit(BWAPI::Unit unit);
	void addUnit(BWAPI::Unit unit);
	void addObserver(BWAPI::Unit observer);

	/// \brief Defines the behavior of the observer unit in the squad.
	/// <summary>
	/// \n If the observer is not under attack, it will stay above the leader. 
	/// \n If the observer is under attack, it will run towards the base (typically away from enemies).
	/// </summary>
	void observerOnFrame();
	void drawSquadBox();
	void drawDebugInfo();

	bool operator==(const Squad& other) noexcept(true) {
		return this->info.squadId == other.info.squadId;
	}
	bool operator!=(const Squad& other) noexcept(true) {
		return !(*this == other);
	}
};