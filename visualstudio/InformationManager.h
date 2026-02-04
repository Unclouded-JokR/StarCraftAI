#pragma once

#include <BWAPI.h>
#include <set>
#include <map>
#include <iostream>
#include "InfluenceMap.h"
#include <vector>
#include "ThreatGrid.h"

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
    bool feedback = false;
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

struct ThreatQueryResult {
    int airThreat = 0;          // higher = more dangerous
    int detectorThreat = 0;     // 0
};

struct EnemyBuildingCounter
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
    int terranAcademy = 0;
    int terranArmory = 0;
    int terranBarracks = 0;
    int terranBunker = 0;
    int terranCommandCenter = 0;
    int terranEngineeringBay = 0;
    int terranFactory = 0;
    int terranMissileTurret = 0;
    int terranRefinery = 0;
    int terranScienceFacility = 0;
    int terranStarport = 0;
    int terranSupplyDepot = 0;
    int zergCreepColony = 0;
    int zergDefilerMound = 0;
    int zergEvolutionChamber = 0;
    int zergExtractor = 0;
    int zergGreaterSpire = 0;
    int zergHatchery = 0;
    int zergHive = 0;
    int zergHydraliskDen = 0;
    int zergInfestedCommandCenter = 0;
    int zergLair = 0;
    int zergNydusCanal = 0;
    int zergQueensNest = 0;
    int zergSpawningPool = 0;
    int zergSpire = 0;
    int zergSporeColony = 0;
    int zergSunkenColony = 0;
    int zergUltraliskCavern = 0;
};

struct enemyTechCounter
{
	bool stimPacks = false;
	bool lockdown = false;
	bool EMPShockwave = false;
	bool spiderMines = false;
	bool scannerSweep = false;
	bool tankSiegeMode = false;
	bool defensiveMatrix = false;
	bool irradiate = false;
	bool yamatoGun = false;
	bool cloakingField = false;
	bool personnelCloaking = false;
	bool restoration = false;
	bool opticalFlare = false;
	bool healing = false;
	bool nuclearStrike = false;
	bool burrowing = false;
	bool infestation = false;
	bool spawnBroodlings = false;
	bool darkSwarm = false;
	bool plague = false;
	bool consume = false;
	bool ensnare = false;
	bool parasite = false;
	bool lurkerAspect = false;
    bool disruptionWeb = false;
    bool psionicStorm = false;
    bool hallucination = false;
    bool maelstrom = false;
    bool mindControl = false;
    bool stasisField = false;
    bool recall = false;
    bool feedback = false;
    bool archonWarp = false;
    bool darkArchonMeld = false;
};

struct enemyUpgradeCounter
{
    int infantryArmor = 0;
    int infantryWeapons = 0;
    int vehicleArmor = 0;
    int vehicleWeapons = 0;
    int shipArmor = 0;
    int shipWeapons = 0;
	bool U238Shells = false;
	bool ionThrusters = false;
	bool titanReactor = false;
	bool ocularImplants = false;
	bool MoebiusReactor = false;
	bool apolloReactor = false;
	bool colossusReactor = false;
	bool caduceusReactor = false;
	bool charonBoosters = false;
	int zergCarapace = 0;
	int zergMeleeAttacks = 0;
	int zergMissileAttacks = 0;
	int zergFlyerAttacks = 0;
	int zergFlyerCarapace = 0;
	bool ventralSacs = false;
	bool antennae = false;
	bool pneumatizedCarapace = false;
	bool metabolicBoost = false;
	bool adrenalGlands = false;
	bool muscularAugments = false;
	bool grooveSpines = false;
	bool gameteMeiosis = false;
	bool metasynapticNodes = false;
	bool chitinousPlating = false;
	bool anabolicSynthesis = false;
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

class InformationManager
{
private:
    std::set<BWAPI::Unit> _knownEnemies;
    std::map<BWAPI::Unit, EnemyBuildingInfo> _knownEnemyBuildings;
    std::map<int, TrackedEnemy> trackedEnemies;  // track by BWAPI unit ID
    InfluenceMap influenceMap;
    ThreatGrid threatGrid;
    double gameState;
	FriendlyUnitCounter friendlyUnitCounter;
	FriendlyBuildingCounter friendlyBuildingCounter;
	FriendlyTechCounter friendlyTechCounter;
    FriendlyUpgradeCounter friendlyUpgradeCounter;
	EnemyBuildingCounter enemyBuildingCounter;
    enemyTechCounter enemyTechCounter;
    enemyUpgradeCounter enemyUpgradeCounter;
    void incrementFriendlyUnit(FriendlyUnitCounter& counter, BWAPI::UnitType type);
	void incrementFriendlyBuilding(FriendlyBuildingCounter& counter, BWAPI::UnitType type);
	void decrementFriendlyUnit(FriendlyUnitCounter& counter, BWAPI::UnitType type);
	void decrementFriendlyBuilding(FriendlyBuildingCounter& counter, BWAPI::UnitType type);
	void checkResearch();
	void checkUpgrades();
	void checkEnemyResearch();
	void checkEnemyUpgrades();
    void printFriendlyUnit();
	void printFriendlyBuilding();
	void printFriendlyResearch();
	void printFriendlyUpgrades();

public:
    ProtoBotCommander* commanderReference;

    std::vector<BWAPI::Position> EnemyBaseLocations;

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
    void printEnemyBuildingCounter() const;
    void updateEnemyBuildingCounter();
    void printEnemyResearch() const;
    void printEnemyUpgrades() const;
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

    // Enemy capability queries
    bool enemyHasAirTech() const;
    bool enemyHasCloakTech() const;

    // Communication to ThreatGrid
    int getEnemyGroundThreatAt(BWAPI::Position p) const;
    int getEnemyDetectionAt(BWAPI::Position p) const;
    ThreatQueryResult queryThreatAt(const BWAPI::Position& pos) const;
};