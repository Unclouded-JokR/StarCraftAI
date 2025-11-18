#include "ScoutingManager.h"
#include "ProtoBotCommander.h"

ScoutingManager::ScoutingManager(ProtoBotCommander* commander)
    : commanderRef(commander) {
}

void ScoutingManager::onStart() 
{
    // behavior created on assignScout
}

void ScoutingManager::onFrame() 
{
    std::visit([&](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) 
        {
            b.onFrame();
        }
        }, behavior);
}

void ScoutingManager::assignScout(BWAPI::Unit unit) 
{
    if (!unit || !unit->exists()) return;
    constructBehaviorFor(unit);

    std::visit([&](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) 
        {
            b.onStart();
            b.assign(unit);
        }
        }, behavior);
}

bool ScoutingManager::hasScout() const 
{
    return std::visit([&](auto const& b)->bool 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (std::is_same_v<T, std::monostate>) return false;
        else return b.hasScout();
        }, behavior);
}

void ScoutingManager::setEnemyMain(const BWAPI::TilePosition& tp) 
{
    std::visit([&](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) b.setEnemyMain(tp);
        }, behavior);
}

void ScoutingManager::onUnitDestroy(BWAPI::Unit unit) 
{
    std::visit([&](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) b.onUnitDestroy(unit);
        }, behavior);
}

void ScoutingManager::constructBehaviorFor(BWAPI::Unit unit) 
{
    const BWAPI::UnitType t = unit->getType();

    if (t == BWAPI::UnitTypes::Protoss_Probe)
    {
        behavior.emplace<ScoutingProbe>(commanderRef);
    }
    /*else if (t == BWAPI::UnitTypes::Protoss_Zealot)
    {
        //behavior.emplace<ScoutingZealot>(commanderRef);
    }
    else if (t == BWAPI::UnitTypes::Protoss_Dragoon)
    {
        //behavior.emplace<ScoutingDragoon>(commanderRef);
    }
    else if (t == BWAPI::UnitTypes::Protoss_Observer)
    {
        //behavior.emplace<ScoutingObserver>(commanderRef);
    }*/
    else 
    {
        // default to Probe behavior for now
        behavior.emplace<ScoutingProbe>(commanderRef);
    }
}
