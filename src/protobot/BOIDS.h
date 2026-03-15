#pragma once
#include <BWAPI.h>
#include "VectorPos.h"
#include "Squad.h"
#include <algorithm>

// DEBUG
//#define DEBUG_FLOCKING

// Frame frequency
#define FRAMES_BETWEEN_BOIDS 10

// Algorithm calibration
#define MIN_SEPARATION_DISTANCE 45.0
#define MIN_LEADER_SURROUND_RADIUS 60.0
#define SEPARATION_STRENGTH 5.0
#define LEADER_STRENGTH 2.0	
#define MIN_BOIDS_VECTOR_LENGTH 3.0


using namespace std;

class BOIDS {
public:
	static void squadFlock(Squad* squad);
private:
	static VectorPos normalize(VectorPos vector);
};