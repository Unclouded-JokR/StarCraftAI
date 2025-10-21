#include "InformationManager.h"
#include "ProtoBotCommander.h"

InformationManager::InformationManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

void InformationManager::onStart()
{
    _knownEnemies.clear();
    _knownEnemyBuildings.clear();
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

    // Comment this out if you don't want the terminal output to be flooded
    if (BWAPI::Broodwar->getFrameCount() % 120 == 0)
    {
        std::cout << "Tracking " << _knownEnemies.size() << " enemy units, "
            << _knownEnemyBuildings.size() << " enemy buildings.\n";
        printKnownEnemies();
        printKnownEnemyBuildings();
    }
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