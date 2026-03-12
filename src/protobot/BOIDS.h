#pragma once
#include <BWAPI.h>
#include "VectorPos.h"
#include "Squad.h"

#define DEBUG_FLOCKING

#define MIN_NEIGHBOUR_DISTANCE 0
#define MIN_SEPARATION_DISTANCE 0
#define SEPARATION_STRENGTH 1
#define COHESION_STRENGTH 1
#define ALIGNMENT_STRENGTH 1
#define LEADER_STRENGTH 1


class BOIDS {
public:
	static void squadFlock(Squad* squad);
private:
	static double getMagnitude(BWAPI::Position vector);
	static VectorPos normalize(VectorPos vector);
};