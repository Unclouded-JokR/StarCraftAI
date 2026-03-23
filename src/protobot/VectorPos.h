#pragma once
#include <BWAPI.h>

class VectorPos : public BWAPI::Point<double, 1> {
public:
	VectorPos(int _x, int _y) {
		this->x = _x;
		this->y = _y;
	};
	VectorPos(double _x, double _y){
		this->x = _x;
		this->y = _y;
	};

	VectorPos(BWAPI::Position pos){
		this->x = pos.x;
		this->y = pos.y;
	};

	VectorPos() {
		this->x = 0.0;
		this->y = 0.0;
	};

	VectorPos normalized() {
		// Avoid division by 0
		const double magnitude = this->getDistance(BWAPI::Position(0, 0));
		if (magnitude < 1 && magnitude > 0) {
			return VectorPos(this->x, this->y);
		}

		if (magnitude == 0) {
			return VectorPos(0, 0);
		}

		return VectorPos(this->x, this->y) / magnitude;
	}

	VectorPos& operator=(const VectorPos& other) noexcept(true) {
		this->x = other.x;
		this->y = other.y;
		return *this;
	}
	VectorPos& operator=(const BWAPI::Point<int, 1> other) noexcept(true) {
		this->x = other.x;
		this->y = other.y;
		return *this;
	}
	VectorPos& operator=(const BWAPI::Point<double, 1> other) noexcept(true) {
		this->x = other.x;
		this->y = other.y;
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
	VectorPos operator/(const double& scalar) const {
		return VectorPos(this->x / scalar, this->y / scalar);
	}
	bool operator==(const VectorPos& rhs) const {
		return this->x == rhs.x && this->y == rhs.y;
	}
	bool operator!=(const VectorPos& rhs) const {
		return !(this->x == rhs.x && this->y == rhs.y);
	}

	std::ostream& operator<<(std::ostream& out) {
		out << "VectorPos(" << this->x << ", " << this->y << ")";
		return out; // Enable chaining
	}
};