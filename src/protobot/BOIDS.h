#pragma once
#include <BWAPI.h>
#include "VectorPos.h"
#include "Squad.h"
#include <algorithm>

// DEBUG
//#define DEBUG_FLOCKING

// Frame frequency
#define FRAMES_BETWEEN_BOIDS 8

// Algorithm calibration
#define MIN_SEPARATION_DISTANCE 40.0
#define INNER_LEADER_RADIUS 40.0

#define SEPARATION_STRENGTH 50.0
#define LEADER_STRENGTH 30.0
#define TERRAIN_STRENGTH 80.0
#define VELOCITY_DAMPENING 0.85

#define TERRAIN_LOOKAHEAD_LENGTH 80.0
#define LOOKAHEAD_LENGTH 30.0


using namespace std;

class BOIDS {
public:
	static void squadFlock(Squad* squad);
private:
	static VectorPos getSeparationVector(BWAPI::Unit unit);
	static VectorPos getTerrainVector(BWAPI::Unit unit, BWAPI::Unit leader);
	static VectorPos normalize(VectorPos vector);
};