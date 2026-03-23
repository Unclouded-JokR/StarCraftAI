#pragma once
#include <BWAPI.h>
#include "VectorPos.h"
#include "Squad.h"
#include <algorithm>

// DEBUG
//#define DEBUG_FLOCKING

// Algorithm calibration
#define BOIDS_RANGE 300.0
#define MIN_SEPARATION_DISTANCE 37.0
#define MIN_FORCE 15.0
#define MAX_FORCE 25.0

#define SEPARATION_STRENGTH 50.0
#define LEADER_SEPARATION_STRENGTH 80.0
#define LEADER_STRENGTH 30.0
#define TERRAIN_STRENGTH 80.0

#define TERRAIN_LOOKAHEAD_LENGTH 80.0


using namespace std;

struct unitHash {
	std::size_t operator()(const BWAPI::Unit& unit) const {
		return std::hash<int>{}(unit->getID());
	}
};

class BOIDS {
public:
	static void squadFlock(Squad* squad);
	static unordered_map<BWAPI::Unit, double, unitHash> leaderRadiusMap;
	static unordered_map<BWAPI::Unit, unordered_map<BWAPI::Unit, double, unitHash>, unitHash> unitDistanceCache;
	static unordered_map<BWAPI::Unit, VectorPos, unitHash> previousBOIDSMap;
private:
	static VectorPos getSeparationSteering(VectorPos unitPos, VectorPos neighborPos, VectorPos unitVelocity);
	static VectorPos getTerrainVector(BWAPI::Unit unit, BWAPI::Unit leader);
	static bool inLeaderRadius(VectorPos unitPos, VectorPos leaderPos, double leaderRadius);
	static VectorPos normalize(VectorPos vector);
};