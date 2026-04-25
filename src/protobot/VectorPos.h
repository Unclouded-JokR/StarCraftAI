#pragma once
#include <BWAPI.h>

/// \brief Custom Vector class that extends from BWAPI::Point, with added vector operations and functions
/// <summary>
/// Since standard position classes such as BWAPI::Position and BWAPI::TilePosition automatically round values to integers, there is a loss of precision for vector calculations.
/// \n Extends from BWAPI::Point using a double template to allow for more precise calculations
/// 
/// </summary>
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


	/// \brief Uses L^2 normalization
	/// <returns>Normalized VectorPos object</returns>
	VectorPos normalized() {
		// Using squared distance to save performance instead of using sqrt
		const double mag = this->getLength();

		if (mag == 0) {
			return VectorPos(0, 0);
		}

		return VectorPos(this->x, this->y) / mag;
	}

	/// <summary>
	/// Returns dot product of this vector and another vector
	/// </summary>
	/// <param name="other">Other VectorPos object</param>
	/// <returns>Dot product</returns>
	double dot(VectorPos other) {
		return this->x * other.x + this->y * other.y;
	}

	/// <summary>
	/// Returns squared distance of vector from origin (0, 0). Used for performance optimization instead of getLength() since sqrt is expensive and we only need to compare distances, not get the actual distance.
	/// </summary>
	/// <returns>Squared distance between vector and origin (0, 0).</returns>
	double getSqDistance() {
		return pow(this->x, 2) + pow(this->y, 2);
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