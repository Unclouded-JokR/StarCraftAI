#include "InformationManager.h"
#include "ProtoBotCommander.h"
#include <algorithm>

InformationManager::InformationManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
	
}

void InformationManager::onStart()
{
    _knownEnemies.clear();
    _knownEnemyBuildings.clear();
    trackedEnemies.clear();

    //influenceMap.onStart();
    threatGrid.onStart();
    
    std::cout << "Information Manager Initialized\n";
}

void InformationManager::onFrame()
{
    checkResearch();
    checkUpgrades();
    checkEnemyResearch();
    checkEnemyUpgrades();

    threatGrid.onFrameStart(BWAPI::Broodwar->getFrameCount());

    // Track visible enemies
    for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (!enemy || !enemy->exists()) continue;

        const int id = enemy->getID();

        TrackedEnemy& entry = trackedEnemies[id];
        entry.id = id;
        entry.type = enemy->getType();
        entry.lastSeenPos = enemy->getPosition();
        entry.isBuilding = enemy->getType().isBuilding();
        entry.destroyed = false;

        _knownEnemies.insert(enemy);

        if (enemy->getType().isBuilding())
        {
            EnemyBuildingInfo& info = _knownEnemyBuildings[enemy];
            info.type = enemy->getType();
            info.lastKnownPosition = enemy->getPosition();
            info.destroyed = false;
        }

        const bool completed = enemy->isCompleted();
        const bool burrowed = enemy->isBurrowed();
        const bool immobile = enemy->getType().isBuilding();

        threatGrid.addOrUpdateEnemy(id, enemy->getType(), enemy->getPosition(), completed, burrowed, immobile);
    }

    // Remove destroyed units
    std::set<BWAPI::Unit> toRemove;
    for (auto& unit : _knownEnemies)
    {
        if (!unit->exists())
            toRemove.insert(unit);
    }
    for (auto& unit : toRemove)
        _knownEnemies.erase(unit);

    // Update trackedEnemies for non-visible units
    for (auto& pair : trackedEnemies)
    {
        int id = pair.first;
        TrackedEnemy& entry = pair.second;

        BWAPI::Unit u = BWAPI::Broodwar->getUnit(id);

        bool visible = (u && u->exists() && u->isVisible());

        if (visible)
        {
            // Update last seen position
            entry.lastSeenPos = u->getPosition();
        }
    }


    // Update building info in fog of war
    for (auto it = _knownEnemyBuildings.begin(); it != _knownEnemyBuildings.end();)
    {
        BWAPI::Unit unit = it->first;
        EnemyBuildingInfo& info = it->second;

        if (info.destroyed)
        {
            it = _knownEnemyBuildings.erase(it);
            continue;
        }

        // If still exists and visible, update position
        if (unit && unit->exists())
            info.lastKnownPosition = unit->getPosition();

        ++it;
    }

    updateEnemyBuildingCounter();

    //TestDrawBaseOwnership();
    if (BWAPI::Broodwar->getFrameCount() % 120 == 0)
    {
        //printFriendlyUnit();
        //printFriendlyBuilding();
        //printFriendlyResearch();
        //printFriendlyUpgrades();
        //TestPrintBaseOwnership();
		//printEnemyBuildingCounter();
		//printEnemyResearch();
		//printEnemyUpgrades();
    }
}

//void InformationManager::onFrame()
//
//    // Track visible enemies
//    for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
//    {
//        if (!enemy || !enemy->exists()) continue;
//
//        int id = enemy->getID();
//
//        TrackedEnemy& entry = trackedEnemies[id];
//        entry.id = id;
//        entry.type = enemy->getType();
//        entry.lastSeenPos = enemy->getPosition();
//        entry.isBuilding = enemy->getType().isBuilding();
//        entry.destroyed = false;  // ensure it's active
//
//        // Also keep your existing containers updated
//        _knownEnemies.insert(enemy);
//
//        if (enemy->getType().isBuilding())
//        {
//            EnemyBuildingInfo& info = _knownEnemyBuildings[enemy];
//            info.type = enemy->getType();
//            info.lastKnownPosition = enemy->getPosition();
//            info.destroyed = false;
//        }
//    }
//
//
//    // Remove destroyed units
//    std::set<BWAPI::Unit> toRemove;
//    for (auto& unit : _knownEnemies)
//    {
//        if (!unit->exists())
//            toRemove.insert(unit);
//    }
//    for (auto& unit : toRemove)
//        _knownEnemies.erase(unit);
//
//    // Update trackedEnemies for non-visible units
//    for (auto& pair : trackedEnemies)
//    {
//        int id = pair.first;
//        TrackedEnemy& entry = pair.second;
//
//        BWAPI::Unit u = BWAPI::Broodwar->getUnit(id);
//
//        bool visible = (u && u->exists() && u->isVisible());
//
//        if (visible)
//        {
//            // Update last seen position
//            entry.lastSeenPos = u->getPosition();
//        }
//    }
//
//
//    // Update building info in fog of war
//    for (auto it = _knownEnemyBuildings.begin(); it != _knownEnemyBuildings.end();)
//    {
//        BWAPI::Unit unit = it->first;
//        EnemyBuildingInfo& info = it->second;
//
//        if (info.destroyed)
//        {
//            it = _knownEnemyBuildings.erase(it);
//            continue;
//        }
//
//        // If still exists and visible, update position
//        if (unit && unit->exists())
//            info.lastKnownPosition = unit->getPosition();
//
//        ++it;
//    }
//
//    for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
//    {
//        int id = enemy->getID();
//
//        TrackedEnemy& e = trackedEnemies[id];
//        e.id = id;
//        e.type = enemy->getType();
//        e.lastSeenPos = enemy->getPosition();
//        e.isBuilding = enemy->getType().isBuilding();
//        e.destroyed = false;
//    }
//
//    //// Update influence map with known enemy units
//    //for (auto& enemy : _knownEnemies)
//    //{
//    //    if (enemy && enemy->exists())
//    //    {
//    //        influenceMap.addEnemyInfluence(enemy->getPosition(), enemy->getType().supplyRequired());
//    //    }
//    //}
//
//    //// Update influence with known enemy buildings
//    //for (auto& [unit, info] : _knownEnemyBuildings)
//    //{
//    //    influenceMap.addEnemyBuildingInfluence(info.lastKnownPosition, info.type);
//    //}
//
//    //// Add friendly influence
//    //for (auto& myUnit : BWAPI::Broodwar->self()->getUnits())
//    //{
//    //    if (myUnit->getType().isBuilding())
//    //        influenceMap.addAllyBuildingInfluence(myUnit->getPosition(), myUnit->getType());
//    //    else
//    //        influenceMap.addAllyInfluence(myUnit->getPosition(), myUnit->getType().supplyRequired());
//    //}
//
//
//    //influenceMap.onFrame();
//    //influenceMap.draw();
//
//    //COMMENT THIS OUT IF GAME FREEZES, TERMINAL CRASHES AND NOTHING IS DRAWING ANYMORE
//    /*if (BWAPI::Broodwar->getFrameCount() % 12 == 0)
//    {
//        gameState = evaluateGameState();
//    }*/
//
//    // Comment this out if you don't want the terminal output to be flooded
//    //if (BWAPI::Broodwar->getFrameCount() % 120 == 0)
//    //{
//    //    std::cout << "Tracking " << _knownEnemies.size() << " enemy units, "
//    //        << _knownEnemyBuildings.size() << " enemy buildings.\n";
//    //    printKnownEnemies();
//    //    printKnownEnemyBuildings();
//    //    printTrackedEnemies();
//    //}
//
//    //if (BWAPI::Broodwar->getFrameCount() % 120 == 0)
//    //{
//    //    std::cout << influenceMap.toStringSummary();
//    //}
//
//    //if (BWAPI::Broodwar->getFrameCount() % 120 == 0)
//    //{
//    //    std::cout << "Game state is: " << gameState << std::endl;
//    //}
//}

std::vector<const BWEM::Base*> InformationManager::FindUnownedBases() const
{
    std::vector<const BWEM::Base*> res;

    // Safety: require theMap to be initialized (your code already calls Initialize() in onStart).
    // We consider a base "owned" if any resource-depot (CC / Nexus / Hatchery) or a depot-under-construction
    // is close to the base center. Otherwise we treat it as unowned.
    const int checkRadius = 96; // pixels ó safe for detecting a town-hall inside/near the base

    for (const BWEM::Area& area : theMap.Areas())
    {
        for (const BWEM::Base& base : area.Bases())
        {
            bool owned = false;
            BWAPI::Position baseCenter = base.Center();

            // iterate units in radius around the base center
            for (BWAPI::Unit u : BWAPI::Broodwar->getUnitsInRadius(baseCenter, checkRadius))
            {
                if (!u) continue;
                // treat any resource depot (completed or under construction) as ownership
                if (u->getType().isResourceDepot())
                {
                    // use Position::getApproxDistance for cheaper distance check
                    if (baseCenter.getApproxDistance(u->getPosition()) <= checkRadius)
                    {
                        owned = true;
                        break;
                    }
                }
            }

            if (!owned)
            {
                res.push_back(&base);
            }
        }
    }

    return res;
}

std::vector<const BWEM::Base*> InformationManager::FindPlayerOwnedBases() const
{
    std::vector<const BWEM::Base*> res;
    const int checkRadius = 96;

    for (const BWEM::Area& area : theMap.Areas())
    {
        for (const BWEM::Base& base : area.Bases())
        {
            BWAPI::Position baseCenter = base.Center();
            bool found = false;
            for (BWAPI::Unit u : BWAPI::Broodwar->getUnitsInRadius(baseCenter, checkRadius))
            {
                if (!u) continue;
                if (u->getType().isResourceDepot() && u->getPlayer() == BWAPI::Broodwar->self())
                {
                    if (baseCenter.getApproxDistance(u->getPosition()) <= checkRadius)
                    {
                        found = true;
                        break;
                    }
                }
            }
            if (found) res.push_back(&base);
        }
    }
    return res;
}

std::vector<const BWEM::Base*> InformationManager::FindEnemyOwnedBases() const
{
    std::vector<const BWEM::Base*> res;
    const int checkRadius = 96;

    for (const BWEM::Area& area : theMap.Areas())
    {
        for (const BWEM::Base& base : area.Bases())
        {
            BWAPI::Position baseCenter = base.Center();
            bool found = false;
            for (BWAPI::Unit u : BWAPI::Broodwar->getUnitsInRadius(baseCenter, checkRadius))
            {
                if (!u) continue;
                if (u->getType().isResourceDepot() && u->getPlayer() == BWAPI::Broodwar->enemy())
                {
                    if (baseCenter.getApproxDistance(u->getPosition()) <= checkRadius)
                    {
                        found = true;
                        break;
                    }
                }
            }
            if (found) res.push_back(&base);
        }
    }
    return res;
}

std::vector<const BWEM::Base*> InformationManager::FindNewBases(int count) const
{
    std::vector<const BWEM::Base*> res;

    if (count <= 0) return res;

    // get all unowned bases first
    std::vector<const BWEM::Base*> unowned = FindUnownedBases();
    if (unowned.empty()) return res;

    // clamp count to available bases
    int want = std::min<int>(count, (int)unowned.size());

    // use map center as reference point
    BWAPI::Position ref = theMap.Center();

    // pair distance -> base
    std::vector<std::pair<int, const BWEM::Base*>> distList;
    distList.reserve(unowned.size());
    for (const BWEM::Base* b : unowned)
    {
        if (!b) continue;
        int d = ref.getApproxDistance(b->Center());
        distList.emplace_back(d, b);
    }

    if ((int)distList.size() <= want)
    {
        // sort all and return
        std::sort(distList.begin(), distList.end(), [](auto &a, auto &b){ return a.first < b.first; });
    }
    else
    {
        // partial selection then sort the selected prefix
        std::nth_element(distList.begin(), distList.begin() + want, distList.end(),
                         [](auto &a, auto &b){ return a.first < b.first; });
        distList.resize(want);
        std::sort(distList.begin(), distList.end(), [](auto &a, auto &b){ return a.first < b.first; });
    }

    res.reserve(distList.size());
    for (const auto &p : distList) res.push_back(p.second);
    return res;
}

void InformationManager::checkResearch()
{
	if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Disruption_Web))
    {
        friendlyTechCounter.disruptionWeb = true;
    }
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm))
    {
        friendlyTechCounter.psionicStorm = true;
	}
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Hallucination))
    {
        friendlyTechCounter.hallucination = true;
    }
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Maelstrom))
    {
        friendlyTechCounter.maelstrom = true;
    }
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Mind_Control))
    {
        friendlyTechCounter.mindControl = true;
    }
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stasis_Field))
    {
        friendlyTechCounter.stasisField = true;
    }
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Recall))
    {
        friendlyTechCounter.recall = true;
    }
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Archon_Warp))
    {
        friendlyTechCounter.archonWarp = true;
    }
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Dark_Archon_Meld))
    {
        friendlyTechCounter.darkArchonMeld = true;
	}
    if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Feedback))
    {
        friendlyTechCounter.feedback = true;
	}
}

void InformationManager::checkUpgrades()
{
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) > friendlyUpgradeCounter.groundWeapons)
    {
        friendlyUpgradeCounter.groundWeapons = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor) > friendlyUpgradeCounter.groundArmor)
    {
        friendlyUpgradeCounter.groundArmor = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor);
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons) > friendlyUpgradeCounter.airWeapons)
    {
        friendlyUpgradeCounter.airWeapons = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons);
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Armor) > friendlyUpgradeCounter.airArmor)
    {
        friendlyUpgradeCounter.airArmor = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Armor);
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Plasma_Shields) > friendlyUpgradeCounter.plasmaShields)
    {
        friendlyUpgradeCounter.plasmaShields = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) > 0)
    {
        friendlyUpgradeCounter.singularityCharge = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Leg_Enhancements) > 0)
    {
        friendlyUpgradeCounter.legEnhancements = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Scarab_Damage) > 0)
    {
        friendlyUpgradeCounter.scarabDamage = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Reaver_Capacity) > 0)
    {
        friendlyUpgradeCounter.reaverCapacity = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Drive) > 0)
    {
        friendlyUpgradeCounter.graviticDrive = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Sensor_Array) > 0)
    {
        friendlyUpgradeCounter.sensorArray = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Boosters) > 0)
    {
        friendlyUpgradeCounter.graviticBoosters = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Khaydarin_Amulet) > 0)
    {
        friendlyUpgradeCounter.khaydarinAmulet = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Apial_Sensors) > 0)
    {
        friendlyUpgradeCounter.apialSensors = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Thrusters) > 0)
    {
        friendlyUpgradeCounter.graviticThrusters = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Carrier_Capacity) > 0)
    {
        friendlyUpgradeCounter.carrierCapacity = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Khaydarin_Core) > 0)
    {
        friendlyUpgradeCounter.khaydarinCore = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Argus_Jewel) > 0)
    {
        friendlyUpgradeCounter.argusJewel = true;
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Argus_Talisman) > 0)
    {
        friendlyUpgradeCounter.argusTalisman = true;
    }
}

void InformationManager::incrementFriendlyUnit(FriendlyUnitCounter& counter, BWAPI::UnitType type)
{
         if (type == BWAPI::UnitTypes::Enum::Protoss_Arbiter)                counter.arbiter++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Archon)                 counter.archon++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Carrier)                counter.carrier++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Corsair)                counter.corsair++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Dark_Archon)            counter.darkArchon++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Dark_Templar)           counter.darkTemplar++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Dragoon)                counter.dragoon++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_High_Templar)           counter.highTemplar++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Interceptor)            counter.interceptor++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Observer)               counter.observer++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Probe)                  counter.probe++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Reaver)                 counter.reaver++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Scout)                  counter.scout++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Shuttle)                counter.shuttle++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Zealot)                 counter.zealot++;
}

void InformationManager::incrementFriendlyBuilding(FriendlyBuildingCounter& counter, BWAPI::UnitType type)
{
         if (type == BWAPI::UnitTypes::Enum::Protoss_Arbiter_Tribunal)       counter.arbiterTribunal++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Citadel_of_Adun)        counter.citadelOfAdun++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Cybernetics_Core)       counter.cyberneticsCore++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Fleet_Beacon)           counter.fleetBeacon++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Forge)                  counter.forge++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Gateway)                counter.gateway++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Nexus)                  counter.nexus++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Observatory)            counter.observatory++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Photon_Cannon)          counter.photonCannon++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Pylon)                  counter.pylon++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Robotics_Facility)      counter.roboticsFacility++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Robotics_Support_Bay)   counter.roboticsSupportBay++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Shield_Battery)         counter.shieldBattery++;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Stargate)               counter.stargate++;
	else if (type == BWAPI::UnitTypes::Enum::Protoss_Templar_Archives)       counter.templarArchives++;
}

void InformationManager::decrementFriendlyUnit(FriendlyUnitCounter& counter, BWAPI::UnitType type)
{
         if (type == BWAPI::UnitTypes::Enum::Protoss_Arbiter)                counter.arbiter--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Archon)                 counter.archon--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Carrier)                counter.carrier--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Corsair)                counter.corsair--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Dark_Archon)            counter.darkArchon--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Dark_Templar)           counter.darkTemplar--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Dragoon)                counter.dragoon--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_High_Templar)           counter.highTemplar--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Interceptor)            counter.interceptor--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Observer)               counter.observer--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Probe)                  counter.probe--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Reaver)                 counter.reaver--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Scout)                  counter.scout--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Shuttle)                counter.shuttle--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Zealot)                 counter.zealot--;
}

void InformationManager::decrementFriendlyBuilding(FriendlyBuildingCounter& counter, BWAPI::UnitType type)
{
         if (type == BWAPI::UnitTypes::Enum::Protoss_Arbiter_Tribunal)       counter.arbiterTribunal--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Citadel_of_Adun)        counter.citadelOfAdun--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Cybernetics_Core)       counter.cyberneticsCore--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Fleet_Beacon)           counter.fleetBeacon--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Forge)                  counter.forge--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Gateway)                counter.gateway--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Nexus)                  counter.nexus--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Observatory)            counter.observatory--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Photon_Cannon)          counter.photonCannon--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Pylon)                  counter.pylon--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Robotics_Facility)      counter.roboticsFacility--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Robotics_Support_Bay)   counter.roboticsSupportBay--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Shield_Battery)         counter.shieldBattery--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Stargate)               counter.stargate--;
    else if (type == BWAPI::UnitTypes::Enum::Protoss_Templar_Archives)       counter.templarArchives--;
}

void InformationManager::updateEnemyBuildingCounter()
{
    // Reset counts
    enemyBuildingCounter = EnemyBuildingCounter();

    for (const auto& kv : _knownEnemyBuildings)
    {
        const EnemyBuildingInfo& info = kv.second;
        if (info.destroyed) continue;
        const BWAPI::UnitType& type = info.type;
        if (!type.isValid()) continue;

        // Protoss
        if (type == BWAPI::UnitTypes::Protoss_Arbiter_Tribunal) ++enemyBuildingCounter.arbiterTribunal;
        else if (type == BWAPI::UnitTypes::Protoss_Assimilator) ++enemyBuildingCounter.assimilator;
        else if (type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun) ++enemyBuildingCounter.citadelOfAdun;
        else if (type == BWAPI::UnitTypes::Protoss_Cybernetics_Core) ++enemyBuildingCounter.cyberneticsCore;
        else if (type == BWAPI::UnitTypes::Protoss_Fleet_Beacon) ++enemyBuildingCounter.fleetBeacon;
        else if (type == BWAPI::UnitTypes::Protoss_Forge) ++enemyBuildingCounter.forge;
        else if (type == BWAPI::UnitTypes::Protoss_Gateway) ++enemyBuildingCounter.gateway;
        else if (type == BWAPI::UnitTypes::Protoss_Nexus) ++enemyBuildingCounter.nexus;
        else if (type == BWAPI::UnitTypes::Protoss_Observatory) ++enemyBuildingCounter.observatory;
        else if (type == BWAPI::UnitTypes::Protoss_Photon_Cannon) ++enemyBuildingCounter.photonCannon;
        else if (type == BWAPI::UnitTypes::Protoss_Pylon) ++enemyBuildingCounter.pylon;
        else if (type == BWAPI::UnitTypes::Protoss_Robotics_Facility) ++enemyBuildingCounter.roboticsFacility;
        else if (type == BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) ++enemyBuildingCounter.roboticsSupportBay;
        else if (type == BWAPI::UnitTypes::Protoss_Shield_Battery) ++enemyBuildingCounter.shieldBattery;
        else if (type == BWAPI::UnitTypes::Protoss_Stargate) ++enemyBuildingCounter.stargate;
        else if (type == BWAPI::UnitTypes::Protoss_Templar_Archives) ++enemyBuildingCounter.templarArchives;

        // Terran
        else if (type == BWAPI::UnitTypes::Terran_Academy) ++enemyBuildingCounter.terranAcademy;
        else if (type == BWAPI::UnitTypes::Terran_Armory) ++enemyBuildingCounter.terranArmory;
        else if (type == BWAPI::UnitTypes::Terran_Barracks) ++enemyBuildingCounter.terranBarracks;
        else if (type == BWAPI::UnitTypes::Terran_Bunker) ++enemyBuildingCounter.terranBunker;
        else if (type == BWAPI::UnitTypes::Terran_Command_Center) ++enemyBuildingCounter.terranCommandCenter;
        else if (type == BWAPI::UnitTypes::Terran_Engineering_Bay) ++enemyBuildingCounter.terranEngineeringBay;
        else if (type == BWAPI::UnitTypes::Terran_Factory) ++enemyBuildingCounter.terranFactory;
        else if (type == BWAPI::UnitTypes::Terran_Missile_Turret) ++enemyBuildingCounter.terranMissileTurret;
        else if (type == BWAPI::UnitTypes::Terran_Refinery) ++enemyBuildingCounter.terranRefinery;
        else if (type == BWAPI::UnitTypes::Terran_Science_Facility) ++enemyBuildingCounter.terranScienceFacility;
        else if (type == BWAPI::UnitTypes::Terran_Starport) ++enemyBuildingCounter.terranStarport;
        else if (type == BWAPI::UnitTypes::Terran_Supply_Depot) ++enemyBuildingCounter.terranSupplyDepot;

        // Zerg
        else if (type == BWAPI::UnitTypes::Zerg_Creep_Colony) ++enemyBuildingCounter.zergCreepColony;
        else if (type == BWAPI::UnitTypes::Zerg_Defiler_Mound) ++enemyBuildingCounter.zergDefilerMound;
        else if (type == BWAPI::UnitTypes::Zerg_Evolution_Chamber) ++enemyBuildingCounter.zergEvolutionChamber;
        else if (type == BWAPI::UnitTypes::Zerg_Extractor) ++enemyBuildingCounter.zergExtractor;
        else if (type == BWAPI::UnitTypes::Zerg_Greater_Spire) ++enemyBuildingCounter.zergGreaterSpire;
        else if (type == BWAPI::UnitTypes::Zerg_Hatchery) ++enemyBuildingCounter.zergHatchery;
        else if (type == BWAPI::UnitTypes::Zerg_Hive) ++enemyBuildingCounter.zergHive;
        else if (type == BWAPI::UnitTypes::Zerg_Hydralisk_Den) ++enemyBuildingCounter.zergHydraliskDen;
        else if (type == BWAPI::UnitTypes::Zerg_Infested_Command_Center) ++enemyBuildingCounter.zergInfestedCommandCenter;
        else if (type == BWAPI::UnitTypes::Zerg_Lair) ++enemyBuildingCounter.zergLair;
        else if (type == BWAPI::UnitTypes::Zerg_Nydus_Canal) ++enemyBuildingCounter.zergNydusCanal;
        else if (type == BWAPI::UnitTypes::Zerg_Queens_Nest) ++enemyBuildingCounter.zergQueensNest;
        else if (type == BWAPI::UnitTypes::Zerg_Spawning_Pool) ++enemyBuildingCounter.zergSpawningPool;
        else if (type == BWAPI::UnitTypes::Zerg_Spire) ++enemyBuildingCounter.zergSpire;
        else if (type == BWAPI::UnitTypes::Zerg_Spore_Colony) ++enemyBuildingCounter.zergSporeColony;
        else if (type == BWAPI::UnitTypes::Zerg_Sunken_Colony) ++enemyBuildingCounter.zergSunkenColony;
        else if (type == BWAPI::UnitTypes::Zerg_Ultralisk_Cavern) ++enemyBuildingCounter.zergUltraliskCavern;
    }
}

void InformationManager::onUnitCreate(BWAPI::Unit unit)
{
    if (unit->getPlayer() != BWAPI::Broodwar->self())
        return;
    if (unit == nullptr)
        return;
    // Currently unused
}

void InformationManager::onUnitComplete(BWAPI::Unit unit)
{
    if (unit->getPlayer() != BWAPI::Broodwar->self())
        return;

    if (unit == nullptr)
        return;

    incrementFriendlyUnit(friendlyUnitCounter, unit->getType());
	incrementFriendlyBuilding(friendlyBuildingCounter, unit->getType());
}

void InformationManager::onUnitMorph(BWAPI::Unit unit)
{
    if (unit->getType() == BWAPI::UnitTypes::Protoss_Assimilator)
        friendlyBuildingCounter.assimilator++;
}

void InformationManager::onUnitDestroy(BWAPI::Unit unit)
{
    if (!unit) return;

	if (unit->getPlayer() == BWAPI::Broodwar->self())
    {
		decrementFriendlyUnit(friendlyUnitCounter, unit->getType());
		decrementFriendlyBuilding(friendlyBuildingCounter, unit->getType());
    }
    else
    {
        int id = unit->getID();

        // Mark in trackedEnemies
        auto it = trackedEnemies.find(id);
        if (it != trackedEnemies.end())
            it->second.destroyed = true;

        if (unit->getPlayer() == BWAPI::Broodwar->enemy())
        {
            // Remove threat from grid
            threatGrid.removeEnemy(id);

            // Mark destroyed building
            if (unit->getType().isBuilding())
            {
                auto it = _knownEnemyBuildings.find(unit);
                if (it != _knownEnemyBuildings.end())
                    it->second.destroyed = true;
            }

            // Remove from knownEnemies
            _knownEnemies.erase(unit);
        }
    }
}


void InformationManager::printKnownEnemies() const
{
    std::cout << "Known enemy units:\n";
    for (auto& unit : _knownEnemies)
    {
        if (unit && unit->exists())
            std::cout << " - " << unit->getType().c_str() << " at " << unit->getPosition() << "\n";
    }
}

void InformationManager::printKnownEnemyBuildings() const
{
    std::cout << "Known enemy buildings:\n";
    for (auto& [unit, info] : _knownEnemyBuildings)
    {
        std::cout << " - " << info.type.c_str()
            << " last seen at " << info.lastKnownPosition
            << (info.destroyed ? " [destroyed]" : "") << "\n";
    }
}

void InformationManager::printEnemyBuildingCounter() const
{
    std::cout << "Enemy Building Count:\n";
    std::cout << " - Arbiter Tribunal: " << enemyBuildingCounter.arbiterTribunal << "\n";
    std::cout << " - Assimilator: " << enemyBuildingCounter.assimilator << "\n";
    std::cout << " - Citadel Of Adun: " << enemyBuildingCounter.citadelOfAdun << "\n";
    std::cout << " - Cybernetics Core: " << enemyBuildingCounter.cyberneticsCore << "\n";
    std::cout << " - Fleet Beacon: " << enemyBuildingCounter.fleetBeacon << "\n";
    std::cout << " - Forge: " << enemyBuildingCounter.forge << "\n";
    std::cout << " - Gateway: " << enemyBuildingCounter.gateway << "\n";
    std::cout << " - Nexus: " << enemyBuildingCounter.nexus << "\n";
    std::cout << " - Observatory: " << enemyBuildingCounter.observatory << "\n";
    std::cout << " - Photon Cannon: " << enemyBuildingCounter.photonCannon << "\n";
    std::cout << " - Pylon: " << enemyBuildingCounter.pylon << "\n";
    std::cout << " - Robotics Facility: " << enemyBuildingCounter.roboticsFacility << "\n";
    std::cout << " - Robotics Support Bay: " << enemyBuildingCounter.roboticsSupportBay << "\n";
    std::cout << " - Shield Battery: " << enemyBuildingCounter.shieldBattery << "\n";
    std::cout << " - Stargate: " << enemyBuildingCounter.stargate << "\n";
    std::cout << " - Templar Archives: " << enemyBuildingCounter.templarArchives << "\n";
    std::cout << " - Terran Academy: " << enemyBuildingCounter.terranAcademy << "\n";
    std::cout << " - Terran Armory: " << enemyBuildingCounter.terranArmory << "\n";
    std::cout << " - Terran Barracks: " << enemyBuildingCounter.terranBarracks << "\n";
    std::cout << " - Terran Bunker: " << enemyBuildingCounter.terranBunker << "\n";
    std::cout << " - Terran Command Center: " << enemyBuildingCounter.terranCommandCenter << "\n";
    std::cout << " - Terran Engineering Bay: " << enemyBuildingCounter.terranEngineeringBay << "\n";
    std::cout << " - Terran Factory: " << enemyBuildingCounter.terranFactory << "\n";
    std::cout << " - Terran Missile Turret: " << enemyBuildingCounter.terranMissileTurret << "\n";
    std::cout << " - Terran Refinery: " << enemyBuildingCounter.terranRefinery << "\n";
    std::cout << " - Terran Science Facility: " << enemyBuildingCounter.terranScienceFacility << "\n";
    std::cout << " - Terran Starport: " << enemyBuildingCounter.terranStarport << "\n";
    std::cout << " - Terran Supply Depot: " << enemyBuildingCounter.terranSupplyDepot << "\n";
    std::cout << " - Zerg Creep Colony: " << enemyBuildingCounter.zergCreepColony << "\n";
    std::cout << " - Zerg Defiler Mound: " << enemyBuildingCounter.zergDefilerMound << "\n";
    std::cout << " - Zerg Evolution Chamber: " << enemyBuildingCounter.zergEvolutionChamber << "\n";
    std::cout << " - Zerg Extractor: " << enemyBuildingCounter.zergExtractor << "\n";
    std::cout << " - Zerg Greater Spire: " << enemyBuildingCounter.zergGreaterSpire << "\n";
    std::cout << " - Zerg Hatchery: " << enemyBuildingCounter.zergHatchery << "\n";
    std::cout << " - Zerg Hive: " << enemyBuildingCounter.zergHive << "\n";
    std::cout << " - Zerg Hydralisk Den: " << enemyBuildingCounter.zergHydraliskDen << "\n";
    std::cout << " - Zerg Infested Command Center: " << enemyBuildingCounter.zergInfestedCommandCenter << "\n";
    std::cout << " - Zerg Lair: " << enemyBuildingCounter.zergLair << "\n";
    std::cout << " - Zerg Nydus Canal: " << enemyBuildingCounter.zergNydusCanal << "\n";
    std::cout << " - Zerg Queen's Nest: " << enemyBuildingCounter.zergQueensNest << "\n";
    std::cout << " - Zerg Spawning Pool: " << enemyBuildingCounter.zergSpawningPool << "\n";
    std::cout << " - Zerg Spire: " << enemyBuildingCounter.zergSpire << "\n";
    std::cout << " - Zerg Spore Colony: " << enemyBuildingCounter.zergSporeColony << "\n";
    std::cout << " - Zerg Sunken Colony: " << enemyBuildingCounter.zergSunkenColony << "\n";
    std::cout << " - Zerg Ultralisk Cavern: " << enemyBuildingCounter.zergUltraliskCavern << "\n";
}

void InformationManager::printTrackedEnemies() const
{
    std::cout << "Tracked enemies (" << trackedEnemies.size() << "):\n";

    for (const auto& [id, e] : trackedEnemies)
    {
        std::cout << " - ID: " << id
            << " | Type: " << e.type.c_str()
            << " | Last seen at: ("
            << e.lastSeenPos.x << ", " << e.lastSeenPos.y << ")"
            << " | Building: " << (e.isBuilding ? "yes" : "no")
            << " | Destroyed: " << (e.destroyed ? "yes" : "no")
            << "\n";
    }
}


double InformationManager::evaluateGameState() const
{
    // Make local snapshots to avoid iterating containers that might be mutated elsewhere.
    std::vector<BWAPI::Unit> myUnits;
    for (auto u : BWAPI::Broodwar->self()->getUnits())
        if (u && u->exists()) myUnits.push_back(u);

    std::vector<BWAPI::Unit> knownEnemies;
    knownEnemies.reserve(_knownEnemies.size());
    for (auto u : _knownEnemies)           // copy pointers (safe)
        if (u && u->exists()) knownEnemies.push_back(u);

    std::vector<EnemyBuildingInfo> knownEnemyBuildings;
    knownEnemyBuildings.reserve(_knownEnemyBuildings.size());
    for (auto const& kv : _knownEnemyBuildings)
    {
        // kv.first is a BWAPI::Unit (may be invalid), kv.second is info
        const EnemyBuildingInfo& info = kv.second;
        if (!info.type.isValid()) continue;
        if (info.destroyed) continue;
        knownEnemyBuildings.push_back(info);
    }

    // helper: unit resource value
    auto unitValue = [](const BWAPI::UnitType& t) -> int {
        if (!t.isValid()) return 0;
        return t.mineralPrice() + t.gasPrice() * 2;
        };

    // -------- 1) Army strength (very cheap) ----------
    int myPower = 0;
    for (auto u : myUnits) myPower += unitValue(u->getType());

    int enemyPower = 0;
    for (auto u : knownEnemies) enemyPower += unitValue(u->getType());

    double armyScore = 0.0;
    if (myPower + enemyPower > 0)
        armyScore = double(myPower - enemyPower) / double(myPower + enemyPower);

    // -------- 2) Worker score (safe) ----------
    auto safeCountWorkers = [&](BWAPI::Player p) -> int {
        if (!p) return 0;
        return p->allUnitCount(BWAPI::UnitTypes::Protoss_Probe)
            + p->allUnitCount(BWAPI::UnitTypes::Terran_SCV)
            + p->allUnitCount(BWAPI::UnitTypes::Zerg_Drone);
        };

    int myWorkers = safeCountWorkers(BWAPI::Broodwar->self());
    int enemyWorkers = 0;
    for (auto u : knownEnemies)
        if (u->getType().isWorker()) ++enemyWorkers;

    double workerScore = 0.0;
    if (myWorkers + enemyWorkers > 0)
        workerScore = double(myWorkers - enemyWorkers) / double(myWorkers + enemyWorkers);

    // -------- 3) Economy (bases) ----------
    auto safeCountBases = [&](BWAPI::Player p)->int {
        if (!p) return 0;
        return p->allUnitCount(BWAPI::UnitTypes::Terran_Command_Center)
            + p->allUnitCount(BWAPI::UnitTypes::Protoss_Nexus)
            + p->allUnitCount(BWAPI::UnitTypes::Zerg_Hatchery);
        };

    int myBases = safeCountBases(BWAPI::Broodwar->self());
    int enemyBases = 0;
    for (auto& binfo : knownEnemyBuildings)
        if (binfo.type.isResourceDepot()) ++enemyBases;

    double econScore = 0.0;
    if (myBases + enemyBases > 0)
        econScore = double(myBases - enemyBases) / double(myBases + enemyBases);

    // -------- 4) Tech score ----------
    auto scorePlayerTech = [&](bool ally)->int {
        int score = 0;
        if (ally)
        {
            for (auto u : myUnits)
            {
                auto t = u->getType();
                if (!t.isValid() || !t.isBuilding()) continue;
                if (t.gasPrice() > 0) score += 5;
            }
        }
        else
        {
            for (auto& b : knownEnemyBuildings)
            {
                auto t = b.type;
                if (!t.isValid()) continue;
                if (t.gasPrice() > 0) score += 5;
            }
        }
        return score;
        };

    int myTech = scorePlayerTech(true);
    int enemyTech = scorePlayerTech(false);
    double techScore = 0.0;
    if (myTech + enemyTech > 0)
        techScore = double(myTech - enemyTech) / double(myTech + enemyTech);

    // -------- 5) Map Control (Grid Sampling, extremely cheap) ----------
    long long allyInf = 0, enemyInf = 0;
    const int w = influenceMap.getWidth();
    const int h = influenceMap.getHeight();

    if (w > 0 && h > 0)
    {
        const int GRID = 12;  // 12x12 grid Å® 144 samples. Very cheap.

        for (int gx = 0; gx < GRID; gx++)
        {
            for (int gy = 0; gy < GRID; gy++)
            {
                // convert grid position Å® influence map index
                int x = (gx * w) / GRID;
                int y = (gy * h) / GRID;

                // safety clamp (just in case of rounding)
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (x >= w) x = w - 1;
                if (y >= h) y = h - 1;

                allyInf += influenceMap.getAllyInfluence(x, y);
                enemyInf += influenceMap.getEnemyInfluence(x, y);
            }
        }
    }

    double mapScore = 0.0;
    if (allyInf + enemyInf > 0)
        mapScore = double(allyInf - enemyInf) / double(allyInf + enemyInf);

    // -------- final weighted score ----------
    double finalScore = 0.0;
    finalScore += armyScore * 0.40;
    finalScore += workerScore * 0.15;
    finalScore += econScore * 0.20;
    finalScore += techScore * 0.10;
    finalScore += mapScore * 0.15;

    // clamp
    if (finalScore > 1.0) finalScore = 1.0;
    if (finalScore < -1.0) finalScore = -1.0;
    return finalScore;
}

void InformationManager::printFriendlyUnit()
{
    std::cout << "Friendly Unit Count:\n";
    std::cout << " - Arbiter: " << friendlyUnitCounter.arbiter << "\n";
    std::cout << " - Archon: " << friendlyUnitCounter.archon << "\n";
    std::cout << " - Carrier: " << friendlyUnitCounter.carrier << "\n";
    std::cout << " - Corsair: " << friendlyUnitCounter.corsair << "\n";
    std::cout << " - Dark Archon: " << friendlyUnitCounter.darkArchon << "\n";
    std::cout << " - Dark Templar: " << friendlyUnitCounter.darkTemplar << "\n";
    std::cout << " - Dragoon: " << friendlyUnitCounter.dragoon << "\n";
    std::cout << " - High Templar: " << friendlyUnitCounter.highTemplar << "\n";
    std::cout << " - Interceptor: " << friendlyUnitCounter.interceptor << "\n";
    std::cout << " - Observer: " << friendlyUnitCounter.observer << "\n";
    std::cout << " - Probe: " << friendlyUnitCounter.probe << "\n";
    std::cout << " - Reaver: " << friendlyUnitCounter.reaver << "\n";
    std::cout << " - Scout: " << friendlyUnitCounter.scout << "\n";
    std::cout << " - Shuttle: " << friendlyUnitCounter.shuttle << "\n";
    std::cout << " - Zealot: " << friendlyUnitCounter.zealot << "\n";
}

void InformationManager::printFriendlyBuilding()
{
	std::cout << "Friendly Building Count:\n";
	std::cout << " - Arbiter Tribunal: " << friendlyBuildingCounter.arbiterTribunal << "\n";
	std::cout << " - Assimilator: " << friendlyBuildingCounter.assimilator << "\n";
	std::cout << " - Citadel Of Adun: " << friendlyBuildingCounter.citadelOfAdun << "\n";
	std::cout << " - Cybernetics Core: " << friendlyBuildingCounter.cyberneticsCore << "\n";
	std::cout << " - Fleet Beacon: " << friendlyBuildingCounter.fleetBeacon << "\n";
	std::cout << " - Forge: " << friendlyBuildingCounter.forge << "\n";
	std::cout << " - Gateway: " << friendlyBuildingCounter.gateway << "\n";
	std::cout << " - Nexus: " << friendlyBuildingCounter.nexus << "\n";
	std::cout << " - Observatory: " << friendlyBuildingCounter.observatory << "\n";
	std::cout << " - Photon Cannon: " << friendlyBuildingCounter.photonCannon << "\n";
	std::cout << " - Pylon: " << friendlyBuildingCounter.pylon << "\n";
	std::cout << " - Robotics Facility: " << friendlyBuildingCounter.roboticsFacility << "\n";
	std::cout << " - Robotics Support Bay: " << friendlyBuildingCounter.roboticsSupportBay << "\n";
	std::cout << " - Shield Battery: " << friendlyBuildingCounter.shieldBattery << "\n";
	std::cout << " - Stargate: " << friendlyBuildingCounter.stargate << "\n";
	std::cout << " - Templar Archives: " << friendlyBuildingCounter.templarArchives << "\n";
}

void InformationManager::printFriendlyResearch()
{
	std::cout << "Friendly Research Status:\n";
	std::cout << " - Disruption Web: " << (friendlyTechCounter.disruptionWeb ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Psionic Storm: " << (friendlyTechCounter.psionicStorm ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Hallucination: " << (friendlyTechCounter.hallucination ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Maelstrom: " << (friendlyTechCounter.maelstrom ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Mind Control: " << (friendlyTechCounter.mindControl ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Stasis Field: " << (friendlyTechCounter.stasisField ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Recall: " << (friendlyTechCounter.recall ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Archon Warp: " << (friendlyTechCounter.archonWarp ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Dark Archon Meld: " << (friendlyTechCounter.darkArchonMeld ? "Researched" : "Not Researched") << "\n";
	std::cout << " - Feedback: " << (friendlyTechCounter.feedback ? "Researched" : "Not Researched") << "\n";
}

void InformationManager::printFriendlyUpgrades()
{
    std::cout << "Friendly Upgrade Levels:\n";
    std::cout << " - Protoss Ground Weapons: " << friendlyUpgradeCounter.groundWeapons << "\n";
    std::cout << " - Protoss Ground Armor: " << friendlyUpgradeCounter.groundArmor << "\n";
    std::cout << " - Protoss Air Weapons: " << friendlyUpgradeCounter.airWeapons << "\n";
    std::cout << " - Protoss Air Armor: " << friendlyUpgradeCounter.airArmor << "\n";
    std::cout << " - Protoss Plasma Shields: " << friendlyUpgradeCounter.plasmaShields << "\n";
    std::cout << " - Singularity Charge: " << (friendlyUpgradeCounter.singularityCharge ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Leg Enhancements: " << (friendlyUpgradeCounter.legEnhancements ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Scarab Damage: " << (friendlyUpgradeCounter.scarabDamage ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Reaver Capacity: " << (friendlyUpgradeCounter.reaverCapacity ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Gravitic Drive: " << (friendlyUpgradeCounter.graviticDrive ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Sensor Array: " << (friendlyUpgradeCounter.sensorArray ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Gravitic Boosters: " << (friendlyUpgradeCounter.graviticBoosters ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Khaydarin Amulet: " << (friendlyUpgradeCounter.khaydarinAmulet ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Apial Sensors: " << (friendlyUpgradeCounter.apialSensors ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Gravitic Thrusters: " << (friendlyUpgradeCounter.graviticThrusters ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Carrier Capacity: " << (friendlyUpgradeCounter.carrierCapacity ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Khaydarin Core: " << (friendlyUpgradeCounter.khaydarinCore ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Argus Jewel: " << (friendlyUpgradeCounter.argusJewel ? "Upgraded" : "Not Upgraded") << "\n";
    std::cout << " - Argus Talisman: " << (friendlyUpgradeCounter.argusTalisman ? "Upgraded" : "Not Upgraded") << "\n";
}

void InformationManager::TestPrintBaseOwnership() const
{
    auto playerBases = FindPlayerOwnedBases();
    auto enemyBases = FindEnemyOwnedBases();
    auto unownedBases = FindUnownedBases();
    auto nearestCandidates = FindNewBases(3);

    std::cout << "Base ownership summary:\n";
    std::cout << " - Player-owned: " << playerBases.size() << "\n";
    for (const BWEM::Base* b : playerBases)
    {
        if (!b) continue;
        auto loc = b->Location();
        std::cout << "    Player base at tile (" << loc.x << "," << loc.y << ")\n";
    }

    std::cout << " - Enemy-owned: " << enemyBases.size() << "\n";
    for (const BWEM::Base* b : enemyBases)
    {
        if (!b) continue;
        auto loc = b->Location();
        std::cout << "    Enemy base at tile (" << loc.x << "," << loc.y << ")\n";
    }

    std::cout << " - Unowned: " << unownedBases.size() << "\n";
    for (const BWEM::Base* b : unownedBases)
    {
        if (!b) continue;
        auto loc = b->Location();
        std::cout << "    Unowned base location at tile (" << loc.x << "," << loc.y << ")\n";
    }

    // Print the nearest unowned candidates (up to 3)
    std::cout << " - Nearest unowned candidate bases (up to 3): " << nearestCandidates.size() << "\n";
    // We'll also print distance to the nearest player base (if any)
    for (size_t i = 0; i < nearestCandidates.size(); ++i)
    {
        const BWEM::Base* b = nearestCandidates[i];
        if (!b) continue;
        auto loc = b->Location();

        int minDist = -1;
        auto playerList = FindPlayerOwnedBases();
        if (!playerList.empty())
        {
            BWAPI::Position bc = b->Center();
            for (const BWEM::Base* pb : playerList)
            {
                if (!pb) continue;
                int d = bc.getApproxDistance(pb->Center());
                if (minDist < 0 || d < minDist) minDist = d;
            }
        }

        std::cout << "    Candidate #" << (i+1) << " at tile (" << loc.x << "," << loc.y << ")";
        if (minDist >= 0) std::cout << " | approx px to nearest our base: " << minDist;
        std::cout << "\n";
    }
}

void InformationManager::TestDrawBaseOwnership() const
{
    const int markerRadius = 80; // pixels
    // Colors: player = green, enemy = red, unowned = yellow, candidates = blue
    for (const BWEM::Base* b : FindPlayerOwnedBases())
    {
        if (!b) continue;
        BWAPI::Position c = b->Center();
        BWAPI::Broodwar->drawCircleMap(c, markerRadius, BWAPI::Colors::Green, true);
        BWAPI::Broodwar->drawTextMap(c.x + 4, c.y + 4, "Player");
    }

    for (const BWEM::Base* b : FindEnemyOwnedBases())
    {
        if (!b) continue;
        BWAPI::Position c = b->Center();
        BWAPI::Broodwar->drawCircleMap(c, markerRadius, BWAPI::Colors::Red, true);
        BWAPI::Broodwar->drawTextMap(c.x + 4, c.y + 4, "Enemy");
    }

    for (const BWEM::Base* b : FindUnownedBases())
    {
        if (!b) continue;
        BWAPI::Position c = b->Center();
        BWAPI::Broodwar->drawCircleMap(c, markerRadius, BWAPI::Colors::Yellow, true);
        BWAPI::Broodwar->drawTextMap(c.x + 4, c.y + 4, "Unowned");
    }

    // Draw nearest candidate expansions with a distinct color/label
    auto candidates = FindNewBases(3);
    int idx = 1;
    for (const BWEM::Base* b : candidates)
    {
        if (!b) continue;
        BWAPI::Position c = b->Center();
        BWAPI::Broodwar->drawCircleMap(c, markerRadius + 16, BWAPI::Colors::Blue, true);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Candidate #%d", idx++);
        BWAPI::Broodwar->drawTextMap(c.x + 4, c.y + 4, buf);
    }
}

// --- convenience wrappers ---------------------------------------------------

std::vector<const BWEM::Base*> InformationManager::GetPlayerBases() const
{
    // Reuse the existing scan implementation
    return FindPlayerOwnedBases();
}

std::vector<const BWEM::Base*> InformationManager::GetEnemyBases() const
{
    return FindEnemyOwnedBases();
}

// Return up to 'count' nearest unowned bases from 'from' (sorted by approximate distance).
std::vector<const BWEM::Base*> InformationManager::GetNearestUnownedBases(const BWAPI::Position& from, int count) const
{
    std::vector<const BWEM::Base*> unowned = FindUnownedBases();
    if (unowned.empty() || count <= 0) return {};

    // Pair each base pointer with its approximate distance to 'from'
    std::vector<std::pair<int, const BWEM::Base*>> distList;
    distList.reserve(unowned.size());

    for (const BWEM::Base* b : unowned)
    {
        if (!b) continue;
        int d = from.getApproxDistance(b->Center());
        distList.emplace_back(d, b);
    }

    // partial sort for efficiency if count is much smaller than total
    if ((size_t)count < distList.size())
    {
        std::nth_element(distList.begin(), distList.begin() + count, distList.end(),
                         [](auto &a, auto &b){ return a.first < b.first; });
        distList.resize(count);
    }

    std::sort(distList.begin(), distList.end(), [](auto &a, auto &b){ return a.first < b.first; });

    std::vector<const BWEM::Base*> res;
    res.reserve(distList.size());
    for (auto &p : distList) res.push_back(p.second);
    return res;
}

void InformationManager::printEnemyResearch() const
{
    std::cout << "Enemy Research Status:\n";
    std::cout << " - Stim Packs: " << (enemyTechCounter.stimPacks ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Lockdown: " << (enemyTechCounter.lockdown ? "Researched" : "Not Researched") << "\n";
    std::cout << " - EMP Shockwave: " << (enemyTechCounter.EMPShockwave ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Spider Mines: " << (enemyTechCounter.spiderMines ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Scanner Sweep: " << (enemyTechCounter.scannerSweep ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Tank Siege Mode: " << (enemyTechCounter.tankSiegeMode ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Defensive Matrix: " << (enemyTechCounter.defensiveMatrix ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Irradiate: " << (enemyTechCounter.irradiate ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Yamato Gun: " << (enemyTechCounter.yamatoGun ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Cloaking Field: " << (enemyTechCounter.cloakingField ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Personnel Cloaking: " << (enemyTechCounter.personnelCloaking ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Restoration: " << (enemyTechCounter.restoration ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Optical Flare: " << (enemyTechCounter.opticalFlare ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Healing: " << (enemyTechCounter.healing ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Nuclear Strike: " << (enemyTechCounter.nuclearStrike ? "Researched" : "Not Researched") << "\n";

    std::cout << " - Burrowing: " << (enemyTechCounter.burrowing ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Infestation: " << (enemyTechCounter.infestation ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Spawn Broodlings: " << (enemyTechCounter.spawnBroodlings ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Dark Swarm: " << (enemyTechCounter.darkSwarm ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Plague: " << (enemyTechCounter.plague ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Consume: " << (enemyTechCounter.consume ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Ensnare: " << (enemyTechCounter.ensnare ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Parasite: " << (enemyTechCounter.parasite ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Lurker Aspect: " << (enemyTechCounter.lurkerAspect ? "Researched" : "Not Researched") << "\n";

    std::cout << " - Disruption Web: " << (enemyTechCounter.disruptionWeb ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Psionic Storm: " << (enemyTechCounter.psionicStorm ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Hallucination: " << (enemyTechCounter.hallucination ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Maelstrom: " << (enemyTechCounter.maelstrom ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Mind Control: " << (enemyTechCounter.mindControl ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Stasis Field: " << (enemyTechCounter.stasisField ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Recall: " << (enemyTechCounter.recall ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Feedback: " << (enemyTechCounter.feedback ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Archon Warp: " << (enemyTechCounter.archonWarp ? "Researched" : "Not Researched") << "\n";
    std::cout << " - Dark Archon Meld: " << (enemyTechCounter.darkArchonMeld ? "Researched" : "Not Researched") << "\n";
}

void InformationManager::printEnemyUpgrades() const
{
    std::cout << "Enemy Upgrade Levels / Flags:\n";
    std::cout << " - Terran Infantry Armor: " << enemyUpgradeCounter.infantryArmor << "\n";
    std::cout << " - Terran Infantry Weapons: " << enemyUpgradeCounter.infantryWeapons << "\n";
    std::cout << " - Terran Vehicle Armor: " << enemyUpgradeCounter.vehicleArmor << "\n";
    std::cout << " - Terran Vehicle Weapons: " << enemyUpgradeCounter.vehicleWeapons << "\n";
    std::cout << " - Terran Ship Armor: " << enemyUpgradeCounter.shipArmor << "\n";
    std::cout << " - Terran Ship Weapons: " << enemyUpgradeCounter.shipWeapons << "\n";
    std::cout << " - U-238 Shells: " << (enemyUpgradeCounter.U238Shells ? "Yes" : "No") << "\n";
    std::cout << " - Ion Thrusters: " << (enemyUpgradeCounter.ionThrusters ? "Yes" : "No") << "\n";
    std::cout << " - Titan Reactor: " << (enemyUpgradeCounter.titanReactor ? "Yes" : "No") << "\n";
    std::cout << " - Ocular Implants: " << (enemyUpgradeCounter.ocularImplants ? "Yes" : "No") << "\n";
    std::cout << " - Moebius Reactor: " << (enemyUpgradeCounter.MoebiusReactor ? "Yes" : "No") << "\n";
    std::cout << " - Apollo Reactor: " << (enemyUpgradeCounter.apolloReactor ? "Yes" : "No") << "\n";
    std::cout << " - Colossus Reactor: " << (enemyUpgradeCounter.colossusReactor ? "Yes" : "No") << "\n";
    std::cout << " - Caduceus Reactor: " << (enemyUpgradeCounter.caduceusReactor ? "Yes" : "No") << "\n";
    std::cout << " - Charon Boosters: " << (enemyUpgradeCounter.charonBoosters ? "Yes" : "No") << "\n";

    std::cout << " - Zerg Carapace: " << enemyUpgradeCounter.zergCarapace << "\n";
    std::cout << " - Zerg Melee Attacks: " << enemyUpgradeCounter.zergMeleeAttacks << "\n";
    std::cout << " - Zerg Missile Attacks: " << enemyUpgradeCounter.zergMissileAttacks << "\n";
    std::cout << " - Zerg Flyer Attacks: " << enemyUpgradeCounter.zergFlyerAttacks << "\n";
    std::cout << " - Zerg Flyer Carapace: " << enemyUpgradeCounter.zergFlyerCarapace << "\n";
    std::cout << " - Ventral Sacs: " << (enemyUpgradeCounter.ventralSacs ? "Yes" : "No") << "\n";
    std::cout << " - Antennae: " << (enemyUpgradeCounter.antennae ? "Yes" : "No") << "\n";
    std::cout << " - Pneumatized Carapace: " << (enemyUpgradeCounter.pneumatizedCarapace ? "Yes" : "No") << "\n";
    std::cout << " - Metabolic Boost: " << (enemyUpgradeCounter.metabolicBoost ? "Yes" : "No") << "\n";
    std::cout << " - Adrenal Glands: " << (enemyUpgradeCounter.adrenalGlands ? "Yes" : "No") << "\n";
    std::cout << " - Muscular Augments: " << (enemyUpgradeCounter.muscularAugments ? "Yes" : "No") << "\n";
    std::cout << " - Grooved Spines: " << (enemyUpgradeCounter.grooveSpines ? "Yes" : "No") << "\n";
    std::cout << " - Gamete Meiosis: " << (enemyUpgradeCounter.gameteMeiosis ? "Yes" : "No") << "\n";
    std::cout << " - Metasynaptic Nodes: " << (enemyUpgradeCounter.metasynapticNodes ? "Yes" : "No") << "\n";
    std::cout << " - Chitinous Plating: " << (enemyUpgradeCounter.chitinousPlating ? "Yes" : "No") << "\n";
    std::cout << " - Anabolic Synthesis: " << (enemyUpgradeCounter.anabolicSynthesis ? "Yes" : "No") << "\n";

    std::cout << " - Protoss Air Armor: " << enemyUpgradeCounter.airArmor << "\n";
    std::cout << " - Protoss Air Weapons: " << enemyUpgradeCounter.airWeapons << "\n";
    std::cout << " - Protoss Ground Armor: " << enemyUpgradeCounter.groundArmor << "\n";
    std::cout << " - Protoss Ground Weapons: " << enemyUpgradeCounter.groundWeapons << "\n";
    std::cout << " - Protoss Plasma Shields: " << enemyUpgradeCounter.plasmaShields << "\n";
    std::cout << " - Singularity Charge: " << (enemyUpgradeCounter.singularityCharge ? "Yes" : "No") << "\n";
    std::cout << " - Leg Enhancements: " << (enemyUpgradeCounter.legEnhancements ? "Yes" : "No") << "\n";
    std::cout << " - Scarab Damage: " << (enemyUpgradeCounter.scarabDamage ? "Yes" : "No") << "\n";
    std::cout << " - Reaver Capacity: " << (enemyUpgradeCounter.reaverCapacity ? "Yes" : "No") << "\n";
    std::cout << " - Gravitic Drive: " << (enemyUpgradeCounter.graviticDrive ? "Yes" : "No") << "\n";
    std::cout << " - Sensor Array: " << (enemyUpgradeCounter.sensorArray ? "Yes" : "No") << "\n";
    std::cout << " - Gravitic Boosters: " << (enemyUpgradeCounter.graviticBoosters ? "Yes" : "No") << "\n";
    std::cout << " - Khaydarin Amulet: " << (enemyUpgradeCounter.khaydarinAmulet ? "Yes" : "No") << "\n";
    std::cout << " - Apial Sensors: " << (enemyUpgradeCounter.apialSensors ? "Yes" : "No") << "\n";
    std::cout << " - Gravitic Thrusters: " << (enemyUpgradeCounter.graviticThrusters ? "Yes" : "No") << "\n";
    std::cout << " - Carrier Capacity: " << (enemyUpgradeCounter.carrierCapacity ? "Yes" : "No") << "\n";
    std::cout << " - Khaydarin Core: " << (enemyUpgradeCounter.khaydarinCore ? "Yes" : "No") << "\n";
    std::cout << " - Argus Jewel: " << (enemyUpgradeCounter.argusJewel ? "Yes" : "No") << "\n";
    std::cout << " - Argus Talisman: " << (enemyUpgradeCounter.argusTalisman ? "Yes" : "No") << "\n";
}

void InformationManager::checkEnemyResearch()
{
    BWAPI::Player enemy = BWAPI::Broodwar->enemy();
    if (!enemy) return;

    // Terran
    enemyTechCounter.stimPacks = enemy->hasResearched(BWAPI::TechTypes::Stim_Packs);
    enemyTechCounter.lockdown = enemy->hasResearched(BWAPI::TechTypes::Lockdown);
    enemyTechCounter.EMPShockwave = enemy->hasResearched(BWAPI::TechTypes::EMP_Shockwave);
    enemyTechCounter.spiderMines = enemy->hasResearched(BWAPI::TechTypes::Spider_Mines);
    enemyTechCounter.scannerSweep = enemy->hasResearched(BWAPI::TechTypes::Scanner_Sweep);
    enemyTechCounter.tankSiegeMode = enemy->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode);
    enemyTechCounter.defensiveMatrix = enemy->hasResearched(BWAPI::TechTypes::Defensive_Matrix);
    enemyTechCounter.irradiate = enemy->hasResearched(BWAPI::TechTypes::Irradiate);
    enemyTechCounter.yamatoGun = enemy->hasResearched(BWAPI::TechTypes::Yamato_Gun);
    enemyTechCounter.cloakingField = enemy->hasResearched(BWAPI::TechTypes::Cloaking_Field);
    enemyTechCounter.personnelCloaking = enemy->hasResearched(BWAPI::TechTypes::Personnel_Cloaking);
    enemyTechCounter.restoration = enemy->hasResearched(BWAPI::TechTypes::Restoration);
    enemyTechCounter.opticalFlare = enemy->hasResearched(BWAPI::TechTypes::Optical_Flare);
    enemyTechCounter.healing = enemy->hasResearched(BWAPI::TechTypes::Healing);
    enemyTechCounter.nuclearStrike = enemy->hasResearched(BWAPI::TechTypes::Nuclear_Strike);

    // Zerg
    enemyTechCounter.burrowing = enemy->hasResearched(BWAPI::TechTypes::Burrowing);
    enemyTechCounter.infestation = enemy->hasResearched(BWAPI::TechTypes::Infestation);
    enemyTechCounter.spawnBroodlings = enemy->hasResearched(BWAPI::TechTypes::Spawn_Broodlings);
    enemyTechCounter.darkSwarm = enemy->hasResearched(BWAPI::TechTypes::Dark_Swarm);
    enemyTechCounter.plague = enemy->hasResearched(BWAPI::TechTypes::Plague);
    enemyTechCounter.consume = enemy->hasResearched(BWAPI::TechTypes::Consume);
    enemyTechCounter.ensnare = enemy->hasResearched(BWAPI::TechTypes::Ensnare);
    enemyTechCounter.parasite = enemy->hasResearched(BWAPI::TechTypes::Parasite);
    enemyTechCounter.lurkerAspect = enemy->hasResearched(BWAPI::TechTypes::Lurker_Aspect);

    // Protoss
    enemyTechCounter.disruptionWeb = enemy->hasResearched(BWAPI::TechTypes::Disruption_Web);
    enemyTechCounter.psionicStorm = enemy->hasResearched(BWAPI::TechTypes::Psionic_Storm);
    enemyTechCounter.hallucination = enemy->hasResearched(BWAPI::TechTypes::Hallucination);
    enemyTechCounter.maelstrom = enemy->hasResearched(BWAPI::TechTypes::Maelstrom);
    enemyTechCounter.mindControl = enemy->hasResearched(BWAPI::TechTypes::Mind_Control);
    enemyTechCounter.stasisField = enemy->hasResearched(BWAPI::TechTypes::Stasis_Field);
    enemyTechCounter.recall = enemy->hasResearched(BWAPI::TechTypes::Recall);
    enemyTechCounter.feedback = enemy->hasResearched(BWAPI::TechTypes::Feedback);
    enemyTechCounter.archonWarp = enemy->hasResearched(BWAPI::TechTypes::Archon_Warp);
    enemyTechCounter.darkArchonMeld = enemy->hasResearched(BWAPI::TechTypes::Dark_Archon_Meld);
}

void InformationManager::checkEnemyUpgrades()
{
    BWAPI::Player enemy = BWAPI::Broodwar->enemy();
    if (!enemy) return;

    // Terran
    enemyUpgradeCounter.infantryArmor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Armor);
    enemyUpgradeCounter.infantryWeapons = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Weapons);
    enemyUpgradeCounter.vehicleArmor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Plating);
    enemyUpgradeCounter.vehicleWeapons = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons);
    enemyUpgradeCounter.shipArmor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Ship_Plating);
    enemyUpgradeCounter.shipWeapons = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Ship_Weapons);
    enemyUpgradeCounter.U238Shells = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells) > 0;
    enemyUpgradeCounter.ionThrusters = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Ion_Thrusters) > 0;
    enemyUpgradeCounter.titanReactor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Titan_Reactor) > 0;
    enemyUpgradeCounter.ocularImplants = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Ocular_Implants) > 0;
    enemyUpgradeCounter.MoebiusReactor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Moebius_Reactor) > 0;
    enemyUpgradeCounter.apolloReactor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Apollo_Reactor) > 0;
    enemyUpgradeCounter.colossusReactor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Colossus_Reactor) > 0;
    enemyUpgradeCounter.caduceusReactor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Caduceus_Reactor) > 0;
    enemyUpgradeCounter.charonBoosters = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Charon_Boosters) > 0;

    // Zerg
    enemyUpgradeCounter.zergCarapace = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace);
    enemyUpgradeCounter.zergMeleeAttacks = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Melee_Attacks);
    enemyUpgradeCounter.zergMissileAttacks = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Missile_Attacks);
    enemyUpgradeCounter.zergFlyerAttacks = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks);
    enemyUpgradeCounter.zergFlyerCarapace = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace);
    enemyUpgradeCounter.ventralSacs = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Ventral_Sacs) > 0;
    enemyUpgradeCounter.antennae = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Antennae) > 0;
    enemyUpgradeCounter.pneumatizedCarapace = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Pneumatized_Carapace) > 0;
    enemyUpgradeCounter.metabolicBoost = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost) > 0;
    enemyUpgradeCounter.adrenalGlands = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Adrenal_Glands) > 0;
    enemyUpgradeCounter.muscularAugments = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments) > 0;
    enemyUpgradeCounter.grooveSpines = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines) > 0;
    enemyUpgradeCounter.gameteMeiosis = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Gamete_Meiosis) > 0;
    enemyUpgradeCounter.metasynapticNodes = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Metasynaptic_Node) > 0;
    enemyUpgradeCounter.chitinousPlating = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Chitinous_Plating) > 0;
    enemyUpgradeCounter.anabolicSynthesis = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Anabolic_Synthesis) > 0;

    // Protoss
    enemyUpgradeCounter.airArmor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Armor);
    enemyUpgradeCounter.airWeapons = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons);
    enemyUpgradeCounter.groundArmor = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor);
    enemyUpgradeCounter.groundWeapons = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
    enemyUpgradeCounter.plasmaShields = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Plasma_Shields);
    enemyUpgradeCounter.singularityCharge = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) > 0;
    enemyUpgradeCounter.legEnhancements = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Leg_Enhancements) > 0;
    enemyUpgradeCounter.scarabDamage = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Scarab_Damage) > 0;
    enemyUpgradeCounter.reaverCapacity = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Reaver_Capacity) > 0;
    enemyUpgradeCounter.graviticDrive = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Drive) > 0;
    enemyUpgradeCounter.sensorArray = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Sensor_Array) > 0;
    enemyUpgradeCounter.graviticBoosters = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Boosters) > 0;
    enemyUpgradeCounter.khaydarinAmulet = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Khaydarin_Amulet) > 0;
    enemyUpgradeCounter.apialSensors = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Apial_Sensors) > 0;
    enemyUpgradeCounter.graviticThrusters = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Thrusters) > 0;
    enemyUpgradeCounter.carrierCapacity = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Carrier_Capacity) > 0;
    enemyUpgradeCounter.khaydarinCore = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Khaydarin_Core) > 0;
    enemyUpgradeCounter.argusJewel = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Argus_Jewel) > 0;
    enemyUpgradeCounter.argusTalisman = enemy->getUpgradeLevel(BWAPI::UpgradeTypes::Argus_Talisman) > 0;
}

// Communication to ThreatGrid

int InformationManager::getEnemyGroundThreatAt(BWAPI::Position p) const
{
    return threatGrid.groundThreatAt(p);
}

int InformationManager::getEnemyDetectionAt(BWAPI::Position p) const
{
    return threatGrid.detectionAt(p);
}

ThreatQueryResult InformationManager::queryThreatAt(const BWAPI::Position& pos) const
{
    ThreatQueryResult r;
    r.airThreat = threatGrid.getAirThreat(pos);
    r.detectorThreat = threatGrid.getDetection(pos);
    return r;
}

bool InformationManager::enemyHasAirTech() const
{
    // Check obvious upgrade indicators
    if (enemyUpgradeCounter.airWeapons > 0) return true;
    if (enemyUpgradeCounter.airArmor > 0) return true;
	if (enemyUpgradeCounter.shipWeapons > 0) return true;
	if (enemyUpgradeCounter.shipArmor > 0) return true;
	if (enemyUpgradeCounter.zergFlyerAttacks > 0) return true;
	if (enemyUpgradeCounter.zergFlyerCarapace > 0) return true;

    // Check known enemy buildings that enable air tech
    for (const auto &kv : _knownEnemyBuildings)
    {
        const BWAPI::UnitType& t = kv.second.type;
        if (!t.isValid()) continue;
        if (t == BWAPI::UnitTypes::Protoss_Stargate
            || t == BWAPI::UnitTypes::Protoss_Fleet_Beacon
            || t == BWAPI::UnitTypes::Zerg_Spire
            || t == BWAPI::UnitTypes::Zerg_Greater_Spire
            || t == BWAPI::UnitTypes::Terran_Starport)
        {
            return true;
        }
    }

    // Check observed enemy units that are air-capable
    for (auto u : _knownEnemies)
    {
        if (!u || !u->exists()) continue;
        BWAPI::UnitType ut = u->getType();
        if (ut == BWAPI::UnitTypes::Protoss_Carrier) return true;
        if (ut == BWAPI::UnitTypes::Protoss_Scout) return true;
        if (ut == BWAPI::UnitTypes::Protoss_Corsair) return true;
        if (ut == BWAPI::UnitTypes::Terran_Battlecruiser) return true;
        if (ut == BWAPI::UnitTypes::Terran_Wraith) return true;
        if (ut == BWAPI::UnitTypes::Terran_Valkyrie) return true;
        if (ut == BWAPI::UnitTypes::Zerg_Mutalisk) return true;
        if (ut == BWAPI::UnitTypes::Zerg_Guardian) return true;
        if (ut == BWAPI::UnitTypes::Zerg_Scourge) return true;
    }

    return false;
}

bool InformationManager::enemyHasCloakTech() const
{
    // Cloak tech can be inferred from researched techs (Cloaking Field, Personnel Cloaking)
    if (enemyTechCounter.cloakingField || enemyTechCounter.personnelCloaking) return true;

    // Also some units/buildings indicate cloak: Dark Archon/Observatory? Observatories enable observers
    for (const auto &kv : _knownEnemyBuildings)
    {
        const BWAPI::UnitType& t = kv.second.type;
        if (!t.isValid()) continue;
        if (t == BWAPI::UnitTypes::Protoss_Observatory || t == BWAPI::UnitTypes::Terran_Science_Facility)
            return true;
    }

    // Check known enemy units for cloaking-capable types (e.g., Dark Templar, Ghost, Wraith, Lurker?)
    for (auto u : _knownEnemies)
    {
        if (!u || !u->exists()) continue;
        BWAPI::UnitType ut = u->getType();
        if (ut == BWAPI::UnitTypes::Protoss_Dark_Templar) return true;
        if (ut == BWAPI::UnitTypes::Terran_Ghost) return true;
        if (ut == BWAPI::UnitTypes::Terran_Wraith) return true;
        if (ut == BWAPI::UnitTypes::Zerg_Lurker) return true;
    }

    return false;
}

