#pragma once
#include <BWAPI.h>
#include <vector>
#include "A-StarPathfinding.h"

#define PATH_DISTANCE_THRESHOLD 32
#define CONSTRUCT_DISTANCE_THRESHOLD 64
#define MAXIMUM_PIXEL_DIFFERENCE 5
#define IDLE_FRAMES_BEFORE_FORCE_NEXT_POSITION 240

/// <summary>
/// An instance of a Builder is created when ProtoBot's Build Manager has done the following for an approved ResourceRequest.
/// \n - Found an ideal location to place
/// \n - Found A worker that is avalible to construct the building
/// \n - Attempted to generate an AStar Path to the located
/// 
/// A Builder's sole repsonbsiblity is to take an approved building ResourceRequest and finally construct the approved building on the map. \n
/// Due to Brood War having outdated path finding algorithm, an AStar class is defined to be able to generate the clicks a Builder need's to 'optimally' travel to a location.
/// \n\n
/// NOTE:\n
/// There are cases where Builder's are not erased from the Build Managers' list of Builder's and is not the correct behaviour a more advance bot should peform. \n
/// Future implementations of ProtoBot should be able to black list locations to build if we dont 'control' a certain area and adapt to find other suitable locations to build. \n
/// This prevents the reassignment of workers to suicide into the enemy just to construct one building at a location we cant build or dont control currently.
/// \n
/// </summary>
/// <param name="unitReference (ally worker unit)"></param>
/// <param name="buildingToPlace (type of building to construct)"></param>
/// <param name="requestedLocation (ideal location to build)"></param>
/// <param name="Path (A* generated path)"></param>
class Builder
{
private:
    BWAPI::Unit unitReference;
    BWAPI::Position lastPosition;

    Path referencePath;
    size_t pathIndex = 0;
    int idleFrames = 0;
    bool debug = false;

public:
    BWAPI::Position requestedPositionToBuild;
    BWAPI::UnitType buildingToConstruct;

    Builder(BWAPI::Unit unitReference, BWAPI::UnitType buildingToPlace, BWAPI::Position requestedLocation, Path path);
    ~Builder();

    /// <summary>
    /// onFrame is where the logic happens for a Builder to perform the clicks to travel to a location.\n\n
    /// 
    /// There are two cases a builder needs to consider to reach a location.
    /// \n - An AStar Path was not generated
    /// \n - An AStar Path was succesfully generated 
    /// 
    /// [Failure]\n
    /// If AStar wasnt able to generate a Path to the requestedLocation, a Builder will just use the native Brood War pathfinding to a location as a fail safe.\n
    /// This will sometimes result in weird behaviour due to how bad the path finding can be but, it is better than not building something. \n
    /// As a worker is traveling to a location, when a Builder is withing a CONSTRUCT_DISTANCE_THRESHOLD amount of pixels away from a building location, a call to build the buildingToConstruct is sent the StarCraft.
    /// 
    /// Besides just checking if a Builder is within a certain range of a buildingLocation. There is also a small check to prevent a Builder from idling due to Brood War. \n To make sure that a worker is infact not standing still, if a Builder's unit reference is flag as idle, a Builder will 'spam' the click until a Builder is moving again.
    /// 
    /// [Success]\n
    /// In the case an AStar Path was succesfully generated, we need to perform a couple different steps compared to the AStar failure case.
    /// An AStar Path is just a vector of positions a Builder needs to travel to 'optimally' arrive at a location. \n
    /// To iterate through the list, we tell a Builder to move to the index of the path using the pathIndex. \n
    /// The path index is way we keep track of what position we should move to until we reach the end of the Path. \n
    /// When we get close to a position in the AStar generated Path change the pathIndex to the next position in the vector.
    /// Similarly, when we get close to the requestedLocation to build or we reach the end of a list, instead of commanding a worker to move, we instead tell the Builder to build.  
    ///
    /// There is also a small check to prevent a Builder from idling due to Brood War. Even in the case of a Builder using an AStar generated Path, a unit is still able to get stuck on the hitboxes of other units.\n
    /// \n To make sure that a worker is infact not standing still due to this, if a Builder's unit reference is flag as idle, a Builder will change the pathIndex to the next position in the Path to hopefully fix this unit hitbox collision.
    /// </summary>
    void onFrame();

    BWAPI::Unit getUnitReference();
    void setUnitReference(BWAPI::Unit);

    /// <summary>
    /// In the case a Builder is destroyed on the way to a location, ProtoBot still attempts to try and build the requested building. \n
    /// This requires ProtoBot to find another suitable worker that is able to constructed the requested building. \n
    /// If another worker unit is found, update the path to reflect the current position of the worker to the requestedLocation to build. \n
    /// Otherwise we should not attempt the try and build this unit.
    /// </summary>
    /// <param name="path"></param>
    void updatePath(Path& path);
};

