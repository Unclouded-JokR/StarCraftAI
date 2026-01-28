#pragma once

#include <BWAPI.h>
#include <set>
#include <map>
#include <iostream>
#include "InfluenceMap.h"
#include <vector>

namespace BWEM { class Base; } // forward-declare BWEM::Base

class ProtoBotCommander;

struct FriendlyUnitCounter
{
    int arbiter = 0;
    int archon = 0;
    int carrier = 0;
    int corsair = 0;
    int darkArchon = 0;
    int darkTemplar = 0;
    int dragoon = 0;
    int highTemplar = 0;
    int interceptor = 0;
    int observer = 0;
    int probe = 0;
    int reaver = 0;
    int scout = 0;
    int shuttle = 0;
    int zealot = 0;
};

struct FriendlyBuildingCounter
{
	int arbiterTribunal = 0;
	int assimilator = 0;
    int citadelOfAdun = 0;
    int cyberneticsCore = 0;
    int fleetBeacon = 0;
    int forge = 0;
    int gateway = 0;
    int nexus = 0;
    int observatory = 0;
    int photonCannon = 0;
    int pylon = 0;
    int roboticsFacility = 0;
    int roboticsSupportBay = 0;
	int shieldBattery = 0;
    int stargate = 0;
    int templarArchives = 0;
};

struct FriendlyUpgradeCounter
{
    int airArmor = 0;
    int airWeapons = 0;
    int groundArmor = 0;
    int groundWeapons = 0;
    int plasmaShields = 0;
	bool singularityCharge = false;
	bool legEnhancements = false;
	bool scarabDamage = false;
	bool reaverCapacity = false;
	bool graviticDrive = false;
    bool sensorArray = false;
	bool graviticBoosters = false;
	bool khaydarinAmulet = false;
	bool apialSensors = false;
	bool graviticThrusters = false;
	bool carrierCapacity = false;
	bool khaydarinCore = false;
	bool argusJewel = false;
	bool argusTalisman = false;
};

struct FriendlyTechCounter
{
	bool disruptionWeb = false;
	bool psionicStorm = false;
	bool hallucination = false;
	bool maelstrom = false;
	bool mindControl = false;
	bool stasisField = false;
	bool recall = false;
	bool archonWarp = false;
	bool darkArchonMeld = false;
};

struct EnemyBuildingInfo
{
    BWAPI::UnitType type;
    BWAPI::Position lastKnownPosition;
    bool destroyed = false;
};

struct TrackedEnemy {
    BWAPI::UnitType type;
    BWAPI::Position lastSeenPos;
    int id;
    bool isBuilding;
    bool destroyed = false;
};

class InformationManager
{
private:
    std::set<BWAPI::Unit> _knownEnemies;
    std::map<BWAPI::Unit, EnemyBuildingInfo> _knownEnemyBuildings;
    std::map<int, TrackedEnemy> trackedEnemies;  // track by BWAPI unit ID
    InfluenceMap influenceMap;
    double gameState;
	FriendlyUnitCounter friendlyUnitCounter;
	FriendlyBuildingCounter friendlyBuildingCounter;
	FriendlyTechCounter friendlyTechCounter;
    FriendlyUpgradeCounter friendlyUpgradeCounter;
    void incrementFriendlyUnit(FriendlyUnitCounter& counter, BWAPI::UnitType type);
	void incrementFriendlyBuilding(FriendlyBuildingCounter& counter, BWAPI::UnitType type);
	void decrementFriendlyUnit(FriendlyUnitCounter& counter, BWAPI::UnitType type);
	void decrementFriendlyBuilding(FriendlyBuildingCounter& counter, BWAPI::UnitType type);
	void checkResearch();
	void checkUpgrades();
    void printFriendlyUnit();
	void printFriendlyBuilding();
	void printFriendlyResearch();
	void printFriendlyUpgrades();

public:
    ProtoBotCommander* commanderReference;

    InformationManager(ProtoBotCommander* commanderReference);
    void onStart();
    void onFrame();
	void onUnitCreate(BWAPI::Unit unit);
    void onUnitComplete(BWAPI::Unit unit);
	void onUnitMorph(BWAPI::Unit unit);
    void onUnitDestroy(BWAPI::Unit unit);
    double evaluateGameState() const;

    // Utility / debug
    const std::set<BWAPI::Unit>& getKnownEnemies() const { return _knownEnemies; }
    const std::map<BWAPI::Unit, EnemyBuildingInfo>& getKnownEnemyBuildings() const { return _knownEnemyBuildings; }

    void printKnownEnemies() const;
    void printKnownEnemyBuildings() const;
    void printTrackedEnemies() const;

	FriendlyUnitCounter getFriendlyUnitCounter() const { return friendlyUnitCounter; }
	FriendlyBuildingCounter getFriendlyBuildingCounter() const { return friendlyBuildingCounter; }
	FriendlyTechCounter getFriendlyTechCounter() const { return friendlyTechCounter; }
	FriendlyUpgradeCounter getFriendlyUpgradeCounter() const { return friendlyUpgradeCounter; }

    // Returns a list of BWEM base locations that currently have no nearby resource depot
    std::vector<const BWEM::Base*> FindUnownedBases() const;
    // Returns a list of BWEM bases that have one of our resource depots nearby
    std::vector<const BWEM::Base*> FindPlayerOwnedBases() const;
    // Returns a list of BWEM bases that have an enemy resource depot nearby
    std::vector<const BWEM::Base*> FindEnemyOwnedBases() const;

    // Convenience accessors / helpers
    std::vector<const BWEM::Base*> GetPlayerBases() const;
    std::vector<const BWEM::Base*> GetEnemyBases() const;
    // Return up to 'count' nearest unowned bases from position 'from' (defaults to 3)
    std::vector<const BWEM::Base*> GetNearestUnownedBases(const BWAPI::Position& from, int count = 3) const;

    // New helper: return up to 'count' nearest unowned bases from the map center (default count = 3)
    std::vector<const BWEM::Base*> FindNewBases(int count = 3) const;

    // Test / debug helpers
    void TestPrintBaseOwnership() const;
    void TestDrawBaseOwnership() const;
};