#include "ScoutingManager.h"
#include "ProtoBotCommander.h"

ScoutingManager::ScoutingManager(ProtoBotCommander* commander)
    : commanderRef(commander) {
}

void ScoutingManager::onStart() 
{
    std::visit([&](auto& b) {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) b.onStart();
        }, behavior);

    drawScoutTags();
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

    drawScoutTags();
}

void ScoutingManager::assignScout(BWAPI::Unit unit) 
{
    if (!unit || !unit->exists()) return;
    constructBehaviorFor(unit);
    markScout(unit);
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
    enemyMainCache_ = tp;
    if (commanderRef) commanderRef->onEnemyMainFound(tp);
    std::visit([&](auto& b) 
        {   
            using T = std::decay_t<decltype(b)>;
            if constexpr (!std::is_same_v<T, std::monostate>) b.setEnemyMain(tp);
        }, behavior);
}

void ScoutingManager::setEnemyNatural(const BWAPI::TilePosition& tp) 
{
    enemyNaturalCache_ = tp;
    if (commanderRef) commanderRef->onEnemyNaturalFound(tp);
}

void ScoutingManager::onUnitDestroy(BWAPI::Unit unit) 
{
    unmarkScout(unit);
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
        behavior.emplace<ScoutingProbe>(commanderRef, this);
        BWAPI::Broodwar->printf("[SM] behavior = Probe");
    }
    else if (t == BWAPI::UnitTypes::Protoss_Zealot || t == BWAPI::UnitTypes::Protoss_Dragoon)
    {
        behavior.emplace<ScoutingZealot>(commanderRef, this);
        BWAPI::Broodwar->printf("[SM] behavior = Zealot");
    }
   /* else if (t == BWAPI::UnitTypes::Protoss_Dragoon)
    {
        //behavior.emplace<ScoutingDragoon>(commanderRef, this);
    }
    else if (t == BWAPI::UnitTypes::Protoss_Observer)
    {
        //behavior.emplace<ScoutingObserver>(commanderRef, this);
    }*/
    else 
    {
        // default to Probe behavior for now
        behavior.emplace<ScoutingProbe>(commanderRef, this);
        BWAPI::Broodwar->printf("[SM] behavior = Fallback->Probe");
    }

    if (enemyMainCache_) {
        std::visit([&](auto& b) {
            using T = std::decay_t<decltype(b)>;
            if constexpr (!std::is_same_v<T, std::monostate>)
                b.setEnemyMain(*enemyMainCache_);
            }, behavior);
    }
}

void ScoutingManager::markScout(BWAPI::Unit u) 
{
    const bool isCombat = u && !u->getType().isWorker();
    markScout(u, isCombat);
}

void ScoutingManager::markScout(BWAPI::Unit u, bool isCombat) 
{
    if (!u || !u->exists()) return;

    if (isCombat) 
    {
        if (std::find(combatScouts_.begin(), combatScouts_.end(), u) == combatScouts_.end())
            combatScouts_.push_back(u);
    }
    else 
    {
        // only one worker scout; if someone calls this while occupied, ignore
        if (!workerScout_) workerScout_ = u;
    }
}

void ScoutingManager::unmarkScout(BWAPI::Unit u) 
{
    if (!u) return;
    if (workerScout_ == u) workerScout_ = nullptr;
    combatScouts_.erase(std::remove(combatScouts_.begin(), combatScouts_.end(), u), combatScouts_.end());
}

bool ScoutingManager::isScout(BWAPI::Unit u) const 
{
    if (!u) return false;
    if (workerScout_ == u) return true;
    return std::find(combatScouts_.begin(), combatScouts_.end(), u) != combatScouts_.end();
}

bool ScoutingManager::isCombatScout(BWAPI::Unit u) const 
{
    if (!u) return false;
    return std::find(combatScouts_.begin(), combatScouts_.end(), u) != combatScouts_.end();
}

bool ScoutingManager::hasWorkerScout() const { return workerScout_ && workerScout_->exists(); }
int  ScoutingManager::numCombatScouts() const { return int(combatScouts_.size()); }

void ScoutingManager::drawScoutTags() const 
{
    // worker
    if (workerScout_ && workerScout_->exists()) 
    {
        const auto p = workerScout_->getPosition();
        BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 20, "\x07SCOUT");
    }
    // combats
    for (auto u : combatScouts_) 
    {
        if (!u || !u->exists()) continue;
        const auto p = u->getPosition();
        BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 20, "\x07SCOUT");
    }
}