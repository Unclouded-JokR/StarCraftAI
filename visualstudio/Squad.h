#pragma once
#include <BWAPI.h>

class Squad {
public:
	int squadType;
	int squadId;
	BWAPI::Color squadColor;
	int unitSize;
	BWAPI::Unitset units;
	bool isAttacking = false;

	Squad(int squadType, int squadId, BWAPI::Color squadColor, int unitSize);

	void removeUnit(BWAPI::Unit unit);
	void move(BWAPI::Position position);
	void attack(BWAPI::Position initialAttackPos);
	void attackMove(BWAPI::Unit unit, BWAPI::Position position);
	void attackUnit(BWAPI::Unit unit, BWAPI::Unit target);
	void attackKite(BWAPI::Unit unit, BWAPI::Unit target);
	void addUnit(BWAPI::Unit unit);
	void drawDebugInfo();
};