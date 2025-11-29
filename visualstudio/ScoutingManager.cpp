#include "ScoutingManager.h"
#include "ProtoBotCommander.h"

ScoutingManager::ScoutingManager(ProtoBotCommander* commander)
    : commanderRef(commander) 
{
}

void ScoutingManager::onStart() 
{
    std::cout << "Scouting Manager Initialized\n";
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

    const auto t = unit->getType();

    if (t.isWorker())
    {
        if (workerScout_ && workerScout_->exists()) return;
    }
    else if (t == BWAPI::UnitTypes::Protoss_Observer)
    {
        if (!canAcceptObserverScout()) return;
        int slot = reserveObserverSlot(unit->getID());
        if (slot < 0) return;
    }
    else
    {
        // zealot or dragoon
        if (!canAcceptCombatScout(unit->getType())) return;
    }
    // build a behavior instance for THIS unit and store by id
    BehaviorVariant sb = constructBehaviorFor(unit);
    visit_start(sb);
    visit_assign(sb, unit);

    if (t == BWAPI::UnitTypes::Protoss_Observer) 
    {
        // find slot we reserved (it’s in observerSlotOwner_)
        int slot = -1;
        for (int i = 0; i < 4; ++i) if (observerSlotOwner_[i] == unit->getID()) { slot = i; break; }
        if (slot >= 0) {
            if (auto* ob = std::get_if<ScoutingObserver>(&sb)) 
            {
                ob->setObserverSlot(slot);
            }
            observerScouts_.push_back(unit);
        }
    }

    // cache into the map (overwrite if re-assigning the same id)
    behaviors_[unit->getID()] = std::move(sb);

    // bookkeeping
    markScout(unit);
}

bool ScoutingManager::hasScout() const 
{
    if (workerScout_ && workerScout_->exists()) return true;
    for (auto u : combatZealots_) if (u && u->exists()) return true;
    for (auto u : combatDragoons_) if (u && u->exists()) return true;
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
        releaseObserverSlot(id);
        observerScouts_.erase(std::remove(observerScouts_.begin(), observerScouts_.end(), unit),
                              observerScouts_.end());
    }

    for (auto& [_, sb] : behaviors_) visit_onUnitDestroy(sb, unit);
}

int ScoutingManager::reserveObserverSlot(int unitId) 
{
    // priority order is 0..3; pick first free
    for (int i = 0; i < 4; ++i) 
    {
        if (observerSlotOwner_[i] == -1) { observerSlotOwner_[i] = unitId; return i; }
    }
    return -1;
}

void ScoutingManager::releaseObserverSlot(int unitId) 
{
    for (int i = 0; i < 4; ++i) if (observerSlotOwner_[i] == unitId) observerSlotOwner_[i] = -1;
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
        BWAPI::Broodwar->printf("[SM] behavior = ZealotBehavior for %s id=%d",
            t.c_str(), unit->getID());
        return z;
    }

    if (t == BWAPI::UnitTypes::Protoss_Observer) 
    {
        ScoutingObserver o(commanderRef, this);
        if (enemyMainCache_) o.setEnemyMain(*enemyMainCache_);
        BWAPI::Broodwar->printf("[SM] behavior = Observer for id=%d", unit->getID());
        return o;
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
    if (!u || !u->exists()) return;

    if (u->getType().isWorker()) {
        if (!workerScout_) workerScout_ = u;
        return;
    }

    if (u->getType() == BWAPI::UnitTypes::Protoss_Zealot) {
        if ((int)combatZealots_.size() < maxZealotScouts_ &&
            std::find(combatZealots_.begin(), combatZealots_.end(), u) == combatZealots_.end()) {
            combatZealots_.push_back(u);
        }
        return;
    }

    if (u->getType() == BWAPI::UnitTypes::Protoss_Dragoon) {
        if ((int)combatDragoons_.size() < maxDragoonScouts_ &&
            std::find(combatDragoons_.begin(), combatDragoons_.end(), u) == combatDragoons_.end()) {
            combatDragoons_.push_back(u);
        }
        return;
    }
}

void ScoutingManager::unmarkScout(BWAPI::Unit u)
{
    if (!u) return;

    if (workerScout_ == u) {
        workerScout_ = nullptr;
        return;
    }

    if (u->getType() == BWAPI::UnitTypes::Protoss_Zealot) {
        combatZealots_.erase(std::remove(combatZealots_.begin(), combatZealots_.end(), u),
            combatZealots_.end());
        return;
    }

    if (u->getType() == BWAPI::UnitTypes::Protoss_Dragoon) {
        combatDragoons_.erase(std::remove(combatDragoons_.begin(), combatDragoons_.end(), u),
            combatDragoons_.end());
        return;
    }

    // If type is unknown (edge case), try removing from both
    combatZealots_.erase(std::remove(combatZealots_.begin(), combatZealots_.end(), u),
        combatZealots_.end());
    combatDragoons_.erase(std::remove(combatDragoons_.begin(), combatDragoons_.end(), u),
        combatDragoons_.end());
}

bool ScoutingManager::isScout(BWAPI::Unit u) const 
{
    if (!u) return false;
    if (workerScout_ == u) return true;
    if (std::find(combatZealots_.begin(), combatZealots_.end(), u) != combatZealots_.end()) return true;
    if (std::find(combatDragoons_.begin(), combatDragoons_.end(), u) != combatDragoons_.end()) return true;
    if (std::find(observerScouts_.begin(), observerScouts_.end(), u) != observerScouts_.end()) return true;
    return false;
}

bool ScoutingManager::isCombatScout(BWAPI::Unit u) const
{
    if (!u) return false;
    if (std::find(combatZealots_.begin(), combatZealots_.end(), u) != combatZealots_.end())
        return true;
    if (std::find(combatDragoons_.begin(), combatDragoons_.end(), u) != combatDragoons_.end())
        return true;
    return false;
}

bool ScoutingManager::hasWorkerScout() const { return workerScout_ && workerScout_->exists(); }

void ScoutingManager::drawScoutTags() const
{
    if (workerScout_ && workerScout_->exists()) 
    {
        const auto p = workerScout_->getPosition();
        BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 20, "\x07SCOUT (Worker)");
    }
    for (auto u : combatZealots_) 
    {
        if (!u || !u->exists()) continue;
        const auto p = u->getPosition();
        BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 20, "\x07SCOUT (Zealot)");
    }
    for (auto u : combatDragoons_) 
    {
        if (!u || !u->exists()) continue;
        const auto p = u->getPosition();
        BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 20, "\x07SCOUT (Dragoon)");
    }
    for (auto u : observerScouts_) 
    {
        if (!u || !u->exists()) continue;
        const auto p = u->getPosition();
        BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 32, "\x10SCOUT (Observer)");
    }
}