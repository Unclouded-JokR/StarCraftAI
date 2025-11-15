#include "InformationManager.h"
#include "ProtoBotCommander.h"

InformationManager::InformationManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

void InformationManager::onStart()
{
    _knownEnemies.clear();
    _knownEnemyBuildings.clear();
    influenceMap.onStart();
    std::cout << "Information Manager started successfully.\n";
}

void InformationManager::onFrame()
{
    // Track visible enemies
    for (auto& enemy : BWAPI::Broodwar->enemy()->getUnits())
    {
        _knownEnemies.insert(enemy);

        // Remember buildings
        if (enemy->getType().isBuilding())
        {
            EnemyBuildingInfo& info = _knownEnemyBuildings[enemy];
            info.type = enemy->getType();
            info.lastKnownPosition = enemy->getPosition();
            info.destroyed = false;
        }
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

    // Update influence map with known enemy units
    //for (auto& enemy : _knownEnemies)
    //{
    //    if (enemy && enemy->exists())
    //    {
    //        influenceMap.addEnemyInfluence(enemy->getPosition(), enemy->getType().supplyRequired());
    //    }
    //}

    //// Update influence with known enemy buildings
    //for (auto& [unit, info] : _knownEnemyBuildings)
    //{
    //    influenceMap.addEnemyBuildingInfluence(info.lastKnownPosition, info.type);
    //}

    //// Add friendly influence
    //for (auto& myUnit : BWAPI::Broodwar->self()->getUnits())
    //{
    //    if (myUnit->getType().isBuilding())
    //        influenceMap.addAllyBuildingInfluence(myUnit->getPosition(), myUnit->getType());
    //    else
    //        influenceMap.addAllyInfluence(myUnit->getPosition(), myUnit->getType().supplyRequired());
    //}


    //influenceMap.onFrame();
    //influenceMap.draw();
    
    //COMMENT THIS OUT IF GAME FREEZES, TERMINAL CRASHES AND NOTHING IS DRAWING ANYMORE
    /*if (BWAPI::Broodwar->getFrameCount() % 12 == 0) 
    {
        gameState = evaluateGameState();
    }*/

    // Comment this out if you don't want the terminal output to be flooded
    /*if (BWAPI::Broodwar->getFrameCount() % 120 == 0)
    {
        std::cout << "Tracking " << _knownEnemies.size() << " enemy units, "
            << _knownEnemyBuildings.size() << " enemy buildings.\n";
        printKnownEnemies();
        printKnownEnemyBuildings();
    }

    if (BWAPI::Broodwar->getFrameCount() % 120 == 0)
    {
        std::cout << influenceMap.toStringSummary();
    }

    if (BWAPI::Broodwar->getFrameCount() % 120 == 0)
    {
        std::cout << "Game state is: " << gameState << std::endl;
    }*/
}

void InformationManager::onUnitDestroy(BWAPI::Unit unit)
{
    if (unit->getPlayer() == BWAPI::Broodwar->enemy())
    {
        // Mark destroyed building
        if (unit->getType().isBuilding())
        {
            auto it = _knownEnemyBuildings.find(unit);
            if (it != _knownEnemyBuildings.end())
                it->second.destroyed = true;
        }

        _knownEnemies.erase(unit);
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


