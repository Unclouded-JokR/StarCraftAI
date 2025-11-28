#include "ScoutingManager.h"
#include "ProtoBotCommander.h"

ScoutingManager::ScoutingManager(ProtoBotCommander* commander)
    : commanderRef(commander) 
{
}

void ScoutingManager::onStart() 
{
    drawScoutTags();
}

static void visit_start(BehaviorVariant& sb) 
{
    std::visit([](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) b.onStart();
        }, sb);
}

static void visit_frame(BehaviorVariant& sb)
{
    std::visit([](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) b.onFrame();
        }, sb);
}

static void visit_assign(BehaviorVariant& sb, BWAPI::Unit u)
{
    std::visit([&](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) b.assign(u);
        }, sb);
}

static void visit_setEnemyMain(BehaviorVariant& sb, const BWAPI::TilePosition& tp)
{
    std::visit([&](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) b.setEnemyMain(tp);
        }, sb);
}

static void visit_onUnitDestroy(BehaviorVariant& sb, BWAPI::Unit u)
{
    std::visit([&](auto& b) 
        {
        using T = std::decay_t<decltype(b)>;
        if constexpr (!std::is_same_v<T, std::monostate>) b.onUnitDestroy(u);
        }, sb);
}

void ScoutingManager::onFrame() 
{
    // iterate all active behaviors; each owns its own state
    for (auto& [id, sb] : behaviors_) 
    {
        visit_frame(sb);
    }

    drawScoutTags();
}

void ScoutingManager::assignScout(BWAPI::Unit unit) 
{
    if (!unit || !unit->exists()) return;

    // build a behavior instance for THIS unit and store by id
    BehaviorVariant sb = constructBehaviorFor(unit);
    visit_start(sb);
    visit_assign(sb, unit);

    // cache into the map (overwrite if re-assigning the same id)
    behaviors_[unit->getID()] = std::move(sb);

    // bookkeeping
    markScout(unit);
}

bool ScoutingManager::hasScout() const 
{
    // You can keep your semantics (true if we have any worker or combat scouts)
    if (workerScout_ && workerScout_->exists()) return true;
    for (auto u : combatScouts_) if (u && u->exists()) return true;
    return false;
}

void ScoutingManager::setEnemyMain(const BWAPI::TilePosition& tp) 
{
    enemyMainCache_ = tp;
    if (commanderRef) commanderRef->onEnemyMainFound(tp);

    // broadcast to all current behaviors
    for (auto& [id, sb] : behaviors_) visit_setEnemyMain(sb, tp);
}

void ScoutingManager::setEnemyNatural(const BWAPI::TilePosition& tp) 
{
    enemyNaturalCache_ = tp;
    if (commanderRef) commanderRef->onEnemyNaturalFound(tp);
}

void ScoutingManager::onUnitDestroy(BWAPI::Unit unit) 
{
    unmarkScout(unit);
    const int id = unit ? unit->getID() : -1;
    if (id != -1) 
    {
        auto it = behaviors_.find(id);
        if (it != behaviors_.end()) 
        {
            visit_onUnitDestroy(it->second, unit);
            behaviors_.erase(it);
        }
    }

    for (auto& [_, sb] : behaviors_) visit_onUnitDestroy(sb, unit);
}

BehaviorVariant ScoutingManager::constructBehaviorFor(BWAPI::Unit unit)
{
    const auto t = unit->getType();

    if (t == BWAPI::UnitTypes::Protoss_Probe) 
    {
        ScoutingProbe probe(commanderRef, this);
        // pass cache if we already know main
        if (enemyMainCache_) probe.setEnemyMain(*enemyMainCache_);
        BWAPI::Broodwar->printf("[SM] behavior = Probe for id=%d", unit->getID());
        return probe;
    }

    if (t == BWAPI::UnitTypes::Protoss_Zealot || t == BWAPI::UnitTypes::Protoss_Dragoon) 
    {
        ScoutingZealot z(commanderRef, this);
        if (enemyMainCache_) z.setEnemyMain(*enemyMainCache_);
        BWAPI::Broodwar->printf("[SM] behavior = Zealot for id=%d", unit->getID());
        return z;
    }

    // fallback
    ScoutingProbe fallback(commanderRef, this);
    if (enemyMainCache_) fallback.setEnemyMain(*enemyMainCache_);
    BWAPI::Broodwar->printf("[SM] behavior = Fallback->Probe for id=%d", unit->getID());
    return fallback;
}

BWAPI::Unit ScoutingManager::findUnitById(int id) 
{
    for (auto u : BWAPI::Broodwar->self()->getUnits())
        if (u && u->exists() && u->getID() == id) return u;
    return nullptr;
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