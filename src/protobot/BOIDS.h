#pragma once
#include <BWAPI.h>
#include "VectorPos.h"
#include "Squad.h"
#include <algorithm>
#include "Hashes.h"

// DEBUG
//#define DEBUG_MAGNITUDES
//#define DEBUG_DRAW

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

/// <summary>
/// Structure for cell within the spatial grid used for BOIDS separation optimization. 
/// \n Each cell has an x and y coordinate, as well as an operator+ for adding two SeparationCells together and an operator== for comparing two SeparationCells.
/// </summary>
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

/// \brief (DISCONTINUED) \n Unfortunately, BOIDS could not be optimized enough to be run within tournament computation time rules. However, the algorithm is still a cool implementation of a flocking algorithm for unit movement, so it is being kept in the codebase for potential future use.
/// 
/// <summary>
/// Uses flocking algorithm to maintain unit separation and formation while moving towards a position.
/// \n Units will adjust their position based on a leader vector, separation vector, and terrain avoidance vector.
/// \n Leader vector keeps units surrounding the squad leader, separation vector keeps units from crowding too tightly, and terrain avoidance vector keeps units away from terrain.
/// \n\n Normalization is done through VectorPos class
/// </summary>
class BOIDS {
public:
	/// \brief
	/// 
	/// <summary>
	/// Main function of BOIDS flocking. Calculates leader, separation, and terrain avoidance vectors for steering each unit in the squad.
	/// \n Uses <b>spatial partitioning optimization</b> to only calculate separation vector with nearby units.
	/// \n\n BOIDS vectors are applied to units in a gradually, being applied to previous BOIDS vectors with a reduced magnitude to cause steering rather than flickering.
	/// 
	/// </summary>
	/// <param name="squad"></param>
	static void squadFlock(Squad* squad);
	static unordered_map<BWAPI::Unit, pair<int, int>, unitHash> leaderRadiusMap;
private:
		
	/// \brief Maps unit to a SeparationCell within the grid used for spatial partitioning
	/// 
	/// <summary>
	/// Returns unit's SeparationCell on the spatial grid map (unitGrid) that stores unit counts on the game map's TilePositions
	/// </summary>
	/// <param name="pos">BWAPI::Position of the unit</param>
	/// <returns>SeparationCell with the x and y coordinates of the cell</returns>
	static SeparationCell unitToCell(BWAPI::Position pos);

	/// \brief Maps unit to a frame counter, being reduced every frame
	/// 
	/// <summary>
	/// To avoid constant checks for when units are near frames, the terrainFrameMap tracks the frames alotted to units for avoiding terrain.
	/// \n If a unit's terrain frames are still greater than 0, the unit's terrain vector will not be calculated.
	/// </summary>
	static unordered_map<BWAPI::Unit, int, unitHash> terrainFrameMap;

	/// \brief Maps unit to a previously calculated terrain vector
	/// 
	/// <summary>
	/// Tracks the vector the unit is currently travelling in to avoid terrain (if the unit had recently avoided any terrain)
	/// \n If a unit is still avoiding terrain (terrainFrameMap[unit] > 0), then applies this direction to the boidsVector for the current frame.
	/// </summary>
	static unordered_map<BWAPI::Unit, VectorPos, unitHash> terrainDirMap;

	/// \brief Maps unit to an unordered_map of previous distance calculations for its surrounding units
	/// 
	/// <summary>
	/// Since we're looping through units, the distance between two units will normally be checked twice. This distance measurement is expensive.
	/// \n To optimize the distance calculation, this map tracks the distance between a unit and its surrounding units.
	/// \n Once one unit calculates the distance between itself and other units, when the latter arrives in the loop the unit will check the map and find previously calculated distances.
	/// </summary>
	static unordered_map<BWAPI::Unit, unordered_map<BWAPI::Unit, double, unitHash>, unitHash> unitDistanceCache;

	/// \brief Maps unit to its previous BOIDS calculations to apply the current BOIDS vector gradually
	/// 
	/// <summary>
	/// BOIDS forces/vectors are not applied in its entirety if a previous BOIDS vector was found.
	/// \n This is to avoid doing sudden flickering movements due to new forces.
	/// \n The current BOIDS direction is applied, with a reduced magnitude, to the previous BOIDS direction to "steer" the unit instead of drastically changing its direction.
	/// </summary>
	static unordered_map<BWAPI::Unit, VectorPos, unitHash> previousBOIDSMap;

	/// \brief Calculates separation steering based on cross product between unit-neighbor vector and the unit's velocity vector
	/// 
	/// <summary>
	/// Uses cross product to determine whether to move left or right relative to the neighbouring unit.
	/// \n This vector's magnitude is modified later in squadFlock() using SEPARATION_STRENGTH which is defined in BOIDS.h
	/// </summary>
	/// <param name="unitPos">Unit position in BWAPI::Position</param>
	/// <param name="neighborPos">Neighbor's position in BWAPI::Position</param>
	/// <param name="unitVelocity">Current velocity of unit in VectorPos</param>
	/// <returns></returns>
	static VectorPos getSeparationSteering(VectorPos unitPos, VectorPos neighborPos, VectorPos unitVelocity);

	/// \brief Checks ahead of unit and, if it finds a close enough terrain, returns vector pointing away from terrain
	/// 
	/// <summary>
	/// Uses unitVelocity to check all four corners of the unit. If, from these corners, the lookahead vectors find a terrain, the unit is going to collide with something soon.
	/// \n Accumulates vectors pointing away from the terrain from each of the checked corners that found a terrain.
	/// \n Final normalized vector used to steer unit away.
	/// </summary>
	/// <param name="unit"></param>
	/// <param name="unitPos"></param>
	/// <param name="leaderPos"></param>
	/// <param name="unitVelocity"></param>
	/// <returns></returns>
	static VectorPos getTerrainSteering(BWAPI::Unit unit, VectorPos unitPos, VectorPos leaderPos, VectorPos unitVelocity);
	static bool inLeaderRadius(VectorPos unitPos, VectorPos leaderPos, double leaderRadius);
};