#pragma once
#include <BWAPI.h>

class VectorPos : public BWAPI::Point<double, 1> {
public:
	VectorPos(double _x, double _y) : BWAPI::Point<double, 1>(_x, _y) {
		this->x = _x;
		this->y = _y;
	};

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