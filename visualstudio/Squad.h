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
	void attack();
	void addUnit(BWAPI::Unit unit);
	void drawDebugInfo();
};