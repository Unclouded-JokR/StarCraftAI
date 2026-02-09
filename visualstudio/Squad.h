#pragma once
#include <BWAPI.h>
#include "A-StarPathfinding.h"
#include "../src/starterbot/Tools.h"
#include "math.h"

class Squad {
public:
	int squadId;
	BWAPI::Color squadColor;
	int unitSize;
	BWAPI::Unit leader;
	std::vector<BWAPI::Unit> units;
	SquadState state;
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

class VectorPos : public BWAPI::Point<double, 1> {
public:
	VectorPos(double _x, double _y) : BWAPI::Point<double, 1>(_x, _y) {}

	VectorPos& operator=(const VectorPos& other) noexcept(true) {
		this->x = other.x;
		this->y = other.y;
		return *this;
	}
	VectorPos& operator=(const BWAPI::Point<int, 1> other) noexcept(true) {
		this->x = (double)other.x;
		this->y = (double)other.y;
		return *this;
	}
	VectorPos& operator=(const BWAPI::Point<double, 1> other) noexcept(true) {
		this->x = (double)other.x;
		this->y = (double)other.y;
		return *this;
	}

	VectorPos operator-(const VectorPos& rhs) const {
		return VectorPos(this->x - rhs.x, this->y - rhs.y);
	}
	VectorPos operator+(const VectorPos& rhs) const {
		return VectorPos(this->x + rhs.x, this->y + rhs.y);
	}
	VectorPos operator*(const double& scalar) const {
		return VectorPos(this->x * scalar, this->y * scalar);
	}
	VectorPos operator*(const int& scalar) const {
		return VectorPos(this->x * scalar, this->y * scalar);
	}
};