#pragma once
#include <BWAPI.h>
#include "VectorPos.h"
#include "Squad.h"
#include <algorithm>

#define DEBUG_FLOCKING

#define MIN_NEIGHBOUR_DISTANCE 150
#define MIN_SEPARATION_DISTANCE 50
#define SEPARATION_STRENGTH 10
#define COHESION_STRENGTH 1
#define ALIGNMENT_STRENGTH 1
#define LEADER_STRENGTH 1

using namespace std;

class BOIDS {
public:
	static void squadFlock(Squad* squad);
private:
	static VectorPos normalize(VectorPos vector);
};