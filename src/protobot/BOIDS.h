#pragma once
#include <BWAPI.h>
#include "VectorPos.h"
#include "Squad.h"

#define DEBUG_FLOCKING

#define SEPARATION_STRENGTH 1
#define COHESION_STRENGTH 1
#define ALIGNMENT_STRENGTH 1
#define LEADER_STRENGTH 1

class BOIDS {
public:
	void squadFlock(Squad* squad);

	double getMagnitude(BWAPI::Position vector);
	VectorPos normalize(VectorPos vector);
};