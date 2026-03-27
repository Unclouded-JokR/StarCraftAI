#pragma once
#include <BWAPI.h>
#include "VectorPos.h"
#include "Squad.h"
#include <algorithm>
#include "Hashes.h"

// DEBUG
//#define DEBUG_MAGNITUDES
#define DEBUG_DRAW

// Algorithm calibration
#define BOIDS_RANGE 300.0
#define MIN_SEPARATION_DISTANCE 37.0
#define MIN_FORCE 15.0
#define MAX_FORCE 30.0
#define TERRAIN_AVOIDANCE_FRAMES 5

#define SEPARATION_STRENGTH 60.0
#define LEADER_SEPARATION_STRENGTH 120.0
#define LEADER_STRENGTH 20.0
#define TERRAIN_STRENGTH 85.0

using namespace std;

struct SeparationCell {
	int x, y;

	SeparationCell() {
		this->x = 0;
		this->y = 0;
	}
	
	SeparationCell(int x, int y) {
		this->x = x;
		this->y = y;
	}

	SeparationCell operator+(const SeparationCell& other) {
		return SeparationCell(this->x + other.x, this->y + other.y);
	}
	
	bool operator==(const SeparationCell& other) const {
		return x == other.x && y == other.y;
	}
};

// Hash for struct SeparationCell used in unordered_map
struct SeparationCellHash {
	size_t operator()(const SeparationCell& c) const {
		return std::hash<int>()(c.x) ^ (std::hash<int>()(c.y) << 1);
	}
};

class BOIDS {
public:
	static void squadFlock(Squad* squad);
	static unordered_map<BWAPI::Unit, pair<int, int>, unitHash> leaderRadiusMap; // unit, squad count at time of calc, radius
private:
	static SeparationCell unitToCell(BWAPI::Position);
	static unordered_map<BWAPI::Unit, int, unitHash> terrainFrameMap;
	static unordered_map<BWAPI::Unit, VectorPos, unitHash> terrainDirMap;
	static unordered_map<BWAPI::Unit, unordered_map<BWAPI::Unit, double, unitHash>, unitHash> unitDistanceCache;
	static unordered_map<BWAPI::Unit, VectorPos, unitHash> previousBOIDSMap;
	static VectorPos getSeparationSteering(VectorPos unitPos, VectorPos neighborPos, VectorPos unitVelocity);
	static VectorPos getTerrainSteering(BWAPI::Unit unit, VectorPos unitPos, VectorPos leaderPos, VectorPos unitVelocity);
	static bool inLeaderRadius(VectorPos unitPos, VectorPos leaderPos, double leaderRadius);
	static VectorPos normalize(VectorPos vector);
};