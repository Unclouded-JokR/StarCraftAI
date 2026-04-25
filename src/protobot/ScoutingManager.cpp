/// <summary>
/// Manages all scouting behavior in ProtoBot.
/// 
/// Responsibilities:
/// - Assign scouting units and attach behaviors
/// - Manage scouting state and unit roles
/// - Track enemy main and natural locations
/// - Coordinate transitions between scouting and combat
/// 
/// Uses a BehaviorVariant system to support multiple scout types:
/// - Probe
/// - Zealot / Dragoon
/// - Observer
/// - Dark Templar
/// </summary>

#include "ScoutingManager.h"
#include "ProtoBotCommander.h"

/// <summary>
/// Initializes the ScoutingManager with a reference to the commander.
/// </summary>
/// <param name="commander">Pointer to ProtoBotCommander</param>
/// 
ScoutingManager::ScoutingManager(ProtoBotCommander* commander)
    : commanderRef(commander)
{
}

/// <summary>
/// Called once at game start.
/// 
/// Used to initialize scouting systems.
/// </summary>

void ScoutingManager::onStart() 
{
    //std::cout << "Scouting Manager Initialized\n";
    //drawScoutTags();
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

/// <summary>
/// Main update loop executed every frame.
/// 
/// Updates all active scouting behaviors and handles:
/// - Deferred enemy main detection
/// - Returning scouts to combat
/// - Debug drawing
/// </summary>

void ScoutingManager::onFrame() 
{
    // iterate all active behaviors; each owns its own state
    inBehaviorFrame_ = true;
    for (auto& [id, sb] : behaviors_)
    {
        visit_frame(sb);
    }
    inBehaviorFrame_ = false;

    if (!enemyMainCache_.has_value() && pendingEnemyMain_.has_value())
    {
        const auto tp = *pendingEnemyMain_;
        pendingEnemyMain_.reset();
        setEnemyMain(tp); // now it is safe to broadcast
    }

    maybeReturnCombatScoutsToCombat();

    if (scoutingDebugEnabled_) 
    {
        drawDebug();
    }
}

/// <summary>
/// Assigns a unit as a scout and attaches the appropriate behavior.
/// 
/// Handles different unit types:
/// - Worker (Probe scout)
/// - Observer (detector scouting)
/// - Combat units (Zealot / Dragoon scouting)
/// 
/// Also removes the unit from combat control.
/// </summary>
/// <param name="unit">Unit to assign as a scout</param>

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
        if (!canAcceptCombatScout(unit->getType()))
        {
            return;
        }
    }
    // build a behavior instance for THIS unit and store by id
    BehaviorVariant sb = constructBehaviorFor(unit);
    
    if (t == BWAPI::UnitTypes::Protoss_Zealot)
    {
        if (proxyPatrolZealotId_ == -1)
        {
            proxyPatrolZealotId_ = unit->getID();
        }

        if (auto* z = std::get_if<ScoutingZealot>(&sb))
        {
            z->setProxyPatroller(unit->getID() == proxyPatrolZealotId_);
        }
    }

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

    visit_start(sb);
    visit_assign(sb, unit);

    // cache into the map (overwrite if re-assigning the same id)
    behaviors_[unit->getID()] = std::move(sb);

    if (commanderRef)
    {
        commanderRef->combatManager.detachUnit(unit);
    }

    // bookkeeping
    markScout(unit);
}

/// <summary>
/// Checks if any scouting units are currently active.
/// </summary>
/// <returns>True if at least one scout exists</returns>

bool ScoutingManager::hasScout() const 
{
    if (workerScout_ && workerScout_->exists())
    {
        return true;
    }

    for (auto u : combatZealots_)
    {
        if (u && u->exists())
        {
            return true;
        }
    }

    for (auto u : combatDragoons_)
    {
        if (u && u->exists())
        {
            return true;
        }
    }

    for (auto u : observerScouts_)
    {
        if (u && u->exists())
        {
            return true;
        }
    }

    for (auto u : darkTemplarScouts_)
    {
        if (u && u->exists())
        {
            return true;
        }
    }

    return false;
}

/// <summary>
/// Sets the enemy main base location and broadcasts it to all scouts.
/// 
/// If called during a behavior frame, the update is deferred
/// to avoid modifying state mid-iteration.
/// </summary>
/// <param name="tp">Enemy main tile position</param>

void ScoutingManager::setEnemyMain(const BWAPI::TilePosition& tp) 
{
    if (!tp.isValid())
    {
        return;
    }

    // Already known: ignore duplicates
    if (enemyMainCache_.has_value())
    {
        return;
    }

    // If called from inside behavior onFrame, defer the broadcast
    if (inBehaviorFrame_)
    {
        pendingEnemyMain_ = tp;
        return;
    }

    // Commit + broadcast
    enemyMainCache_ = tp;

    if (commanderRef)
    {
        commanderRef->onEnemyMainFound(tp);
    }

    for (auto& [id, sb] : behaviors_)
    {
        visit_setEnemyMain(sb, tp);
    }
}

/// <summary>
/// Sets the enemy natural expansion location.
/// </summary>
/// <param name="tp">Enemy natural tile position</param>

void ScoutingManager::setEnemyNatural(const BWAPI::TilePosition& tp) 
{
    enemyNaturalCache_ = tp;
    if (commanderRef) commanderRef->onEnemyNaturalFound(tp);
}

/// <summary>
/// Handles cleanup when a unit is destroyed.
/// 
/// Removes scouting behavior and updates tracking structures.
/// </summary>
/// <param name="unit">Destroyed unit</param>

void ScoutingManager::onUnitDestroy(BWAPI::Unit unit)
{
    if (!unit)
    {
        return;
    }

    const int id = unit->getID();

    // Only do anything if it was a scout or had a behavior.
    const bool hadBehavior = behaviors_.find(id) != behaviors_.end();
    const bool wasScout = isScout(unit);

    if (!hadBehavior && !wasScout)
    {
        return;
    }

    unmarkScout(unit);

    if (id == proxyPatrolZealotId_)
    {
        proxyPatrolZealotId_ = -1;
    }

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

/// <summary>
/// Creates the appropriate scouting behavior for a unit.
/// 
/// Supports:
/// - Probe
/// - Zealot / Dragoon
/// - Observer
/// - Dark Templar
/// 
/// Falls back to Probe behavior if type is unsupported.
/// </summary>
/// <param name="unit">Unit to create behavior for</param>
/// <returns>BehaviorVariant representing the unit's scouting logic</returns>

BehaviorVariant ScoutingManager::constructBehaviorFor(BWAPI::Unit unit)
{
    const auto t = unit->getType();

    if (t == BWAPI::UnitTypes::Protoss_Probe) 
    {
        ScoutingProbe probe(commanderRef, this);
        // pass cache if we already know main
        if (enemyMainCache_) probe.setEnemyMain(*enemyMainCache_);
        //BWAPI::Broodwar->printf("[SM] behavior = Probe for id=%d", unit->getID());
        return probe;
    }

    if (t == BWAPI::UnitTypes::Protoss_Zealot || t == BWAPI::UnitTypes::Protoss_Dragoon) 
    {
        ScoutingZealot z(commanderRef, this);
        if (enemyMainCache_) z.setEnemyMain(*enemyMainCache_);
        //BWAPI::Broodwar->printf("[SM] behavior = ZealotBehavior for %s id=%d",
          //  t.c_str(), unit->getID());
        return z;
    }

    if (t == BWAPI::UnitTypes::Protoss_Observer) 
    {
        ScoutingObserver o(commanderRef, this);
        if (enemyMainCache_) o.setEnemyMain(*enemyMainCache_);
        //BWAPI::Broodwar->printf("[SM] behavior = Observer for id=%d", unit->getID());
        return o;
    }

    if (t == BWAPI::UnitTypes::Protoss_Dark_Templar)
    {
        DarkTemplar dt(commanderRef, this);

        if (enemyMainCache_)
        {
            dt.setEnemyMain(*enemyMainCache_);
        }

        return dt;
    }

    // fallback
    ScoutingProbe fallback(commanderRef, this);
    if (enemyMainCache_) fallback.setEnemyMain(*enemyMainCache_);
   // BWAPI::Broodwar->printf("[SM] behavior = Fallback->Probe for id=%d", unit->getID());
    return fallback;
}

BWAPI::Unit ScoutingManager::findUnitById(int id) 
{
    for (auto u : BWAPI::Broodwar->self()->getUnits())
        if (u && u->exists() && u->getID() == id) return u;
    return nullptr;
}

/// <summary>
/// Marks a unit as a scout and tracks it by type.
/// 
/// Adds the unit to the appropriate scouting collection.
/// </summary>
/// <param name="u">Unit to mark as scout</param>

void ScoutingManager::markScout(BWAPI::Unit u)
{
    if (!u || !u->exists()) return;

    const auto t = u->getType();

    if (t.isWorker()) {
        if (!workerScout_) workerScout_ = u;
        return;
    }

    if (t == BWAPI::UnitTypes::Protoss_Observer)
    {
        if (std::find(observerScouts_.begin(), observerScouts_.end(), u) == observerScouts_.end())
        {
            observerScouts_.push_back(u);
        }
        return;
    }
    if (t == BWAPI::UnitTypes::Protoss_Zealot) {
        if ((int)combatZealots_.size() < maxZealotScouts_ &&
            std::find(combatZealots_.begin(), combatZealots_.end(), u) == combatZealots_.end()) {
            combatZealots_.push_back(u);
        }
        return;
    }

    if (t == BWAPI::UnitTypes::Protoss_Dragoon) {
        if ((int)combatDragoons_.size() < maxDragoonScouts_ &&
            std::find(combatDragoons_.begin(), combatDragoons_.end(), u) == combatDragoons_.end()) {
            combatDragoons_.push_back(u);
        }
        return;
    }
    if (t == BWAPI::UnitTypes::Protoss_Dark_Templar)
    {
        if (std::find(darkTemplarScouts_.begin(), darkTemplarScouts_.end(), u) == darkTemplarScouts_.end())
        {
            darkTemplarScouts_.push_back(u);
        }
        return;
    }
}

/// <summary>
/// Removes a unit from scout tracking structures.
/// </summary>
/// <param name="u">Unit to remove</param>

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

    if (u->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar)
    {
        darkTemplarScouts_.erase(std::remove(darkTemplarScouts_.begin(), darkTemplarScouts_.end(), u),
            darkTemplarScouts_.end());
        return;
    }

    combatZealots_.erase(std::remove(combatZealots_.begin(), combatZealots_.end(), u),
        combatZealots_.end());
    combatDragoons_.erase(std::remove(combatDragoons_.begin(), combatDragoons_.end(), u),
        combatDragoons_.end());
    observerScouts_.erase(std::remove(observerScouts_.begin(), observerScouts_.end(), u), 
        observerScouts_.end());
    darkTemplarScouts_.erase(std::remove(darkTemplarScouts_.begin(), darkTemplarScouts_.end(), u),
        darkTemplarScouts_.end());
}

/// <summary>
/// Checks whether a unit is currently assigned as a scout.
/// </summary>
/// <param name="u">Unit to check</param>
/// <returns>True if the unit is a scout</returns>

bool ScoutingManager::isScout(BWAPI::Unit u) const 
{
    if (!u)                                                                                                 return false;
    if (workerScout_ == u)                                                                                  return true;
    if (std::find(combatZealots_.begin(), combatZealots_.end(), u) != combatZealots_.end())                 return true;
    if (std::find(combatDragoons_.begin(), combatDragoons_.end(), u) != combatDragoons_.end())              return true;
    if (std::find(observerScouts_.begin(), observerScouts_.end(), u) != observerScouts_.end())              return true;
    if (std::find(darkTemplarScouts_.begin(), darkTemplarScouts_.end(), u) != darkTemplarScouts_.end())     return true;
    return false;
}

/// <summary>
/// Checks if a unit is a combat scout (Zealot or Dragoon).
/// </summary>
/// <param name="u">Unit to check</param>
/// <returns>True if unit is a combat scout</returns>

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

namespace
{
    struct DrawBehaviorDebugVisitor
    {
        void operator()(const std::monostate&) const
        {
        }

        void operator()(const ScoutingProbe& b) const
        {
            b.drawDebug();
        }

        void operator()(const ScoutingZealot& b) const
        {
            b.drawDebug();
        }

        void operator()(const ScoutingObserver& b) const
        {
            b.drawDebug();
        }

        void operator()(const DarkTemplar& b) const
        {
            b.drawDebug();
        }
    };
}

/// <summary>
/// Draws debug information for all scouting behaviors.
/// 
/// Includes:
/// - Global scouting info
/// - Enemy locations
/// - Per-unit debug visuals
/// </summary>

void ScoutingManager::drawDebug() const
{
    if (!scoutingDebugEnabled_)
    {
        return;
    }

    drawGlobalDebugPanel();
    drawKnownEnemyLocations();

    for (const auto& [id, sb] : behaviors_)
    {
        std::visit(DrawBehaviorDebugVisitor{}, sb);
    }
}

void ScoutingManager::drawGlobalDebugPanel() const
{

    int x = 10;
    int y = 30;

    BWAPI::Broodwar->drawTextScreen(x, y, "\x07Scouting Debug");
    y += 14;

    BWAPI::Broodwar->drawTextScreen(
        x,
        y,
        "Enemy Main: %s",
        enemyMainCache_.has_value() ? "\x07Known" : "\x08Unknown"
    );
    y += 12;

    BWAPI::Broodwar->drawTextScreen(
        x,
        y,
        "Worker Scout: %s",
        (workerScout_ && workerScout_->exists()) ? "\x07 Active" : "\x08None"
    );
    y += 12;

    BWAPI::Broodwar->drawTextScreen(x, y, "Combat Scouts: %d", numCombatScouts());
    y += 12;

    BWAPI::Broodwar->drawTextScreen(x, y, "Observer Scouts: %d", (int)observerScouts_.size());
    y += 12;

    const int totalScouts =
        (workerScout_ && workerScout_->exists() ? 1 : 0) +
        (int)combatZealots_.size() +
        (int)combatDragoons_.size() +
        (int)observerScouts_.size() +
        (int)darkTemplarScouts_.size();

    BWAPI::Broodwar->drawTextScreen(x, y, "All Scouts: %d", totalScouts);
    y += 12;

    BWAPI::Broodwar->drawTextScreen(
        x,
        y,
        "Combat Scouting Started: %s",
        combatScoutingStarted_ ? "\x07Yes" : "\x08No"
    );
}

void ScoutingManager::drawKnownEnemyLocations() const
{

    if (enemyMainCache_.has_value())
    {
        BWAPI::Position p(enemyMainCache_.value());
        BWAPI::Broodwar->drawCircleMap(p, 96, BWAPI::Colors::Cyan, false);
        BWAPI::Broodwar->drawTextMap(p.x, p.y - 18, "\x0f Enemy Main");
    }

}

void ScoutingManager::drawScoutDebugFor(BWAPI::Unit unit) const
{
    if (!unit || !unit->exists())
    {
        return;
    }

    const BWAPI::Position p = unit->getPosition();

    BWAPI::Broodwar->drawCircleMap(p, 18, BWAPI::Colors::Green, false);

    const BWAPI::UnitType t = unit->getType();

    const char* role = "Scout";

    if (t == BWAPI::UnitTypes::Protoss_Probe)
    {
        role = "Probe Scout";
    }
    else if (t == BWAPI::UnitTypes::Protoss_Zealot)
    {
        role = "Zealot Scout";
    }
    else if (t == BWAPI::UnitTypes::Protoss_Dragoon)
    {
        role = "Dragoon Scout";
    }
    else if (t == BWAPI::UnitTypes::Protoss_Observer)
    {
        role = "Observer Scout";
    }
    else if (t == BWAPI::UnitTypes::Protoss_Dark_Templar)
    {
        role = "DT Scout";
    }

    BWAPI::Broodwar->drawTextMap(p.x - 28, p.y - 30, "\x07%s", role);

    if (enemyMainCache_.has_value())
    {
        BWAPI::Position mainPos(enemyMainCache_.value());
        BWAPI::Broodwar->drawLineMap(p, mainPos, BWAPI::Colors::Green);
    }

    if (enemyNaturalCache_.has_value())
    {
        BWAPI::Position natPos(enemyNaturalCache_.value());
        BWAPI::Broodwar->drawCircleMap(natPos, 16, BWAPI::Colors::Orange, false);
    }
}



void ScoutingManager::drawScoutTags() const
{
    if (workerScout_ && workerScout_->exists()) 
    {
        const auto p = workerScout_->getPosition();
        //BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 20, "\x07SCOUT (Worker)");
    }
    for (auto u : combatZealots_) 
    {
        if (!u || !u->exists()) continue;
        const auto p = u->getPosition();
        //BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 20, "\x07SCOUT (Zealot)");
    }
    for (auto u : combatDragoons_) 
    {
        if (!u || !u->exists()) continue;
        const auto p = u->getPosition();
        //BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 20, "\x07SCOUT (Dragoon)");
    }
    for (auto u : observerScouts_) 
    {
        if (!u || !u->exists()) continue;
        const auto p = u->getPosition();
        //BWAPI::Broodwar->drawTextMap(p.x - 16, p.y + 32, "\x10SCOUT (Observer)");
    }
}

bool ScoutingManager::combatScoutLockActive() const
{
    return BWAPI::Broodwar->getFrameCount() < combatScoutLockUntilFrame_;
}

int ScoutingManager::combatScoutLockFramesRemaining() const
{
    const int remaining = combatScoutLockUntilFrame_ - BWAPI::Broodwar->getFrameCount();
    return remaining > 0 ? remaining : 0;
}

void ScoutingManager::maybeReturnCombatScoutsToCombat()
{
    std::vector<BWAPI::Unit> candidates;

    candidates.reserve(combatZealots_.size() + combatDragoons_.size());

    for (BWAPI::Unit u : combatZealots_)
    {
        if (u && u->exists())
        {
            candidates.push_back(u);
        }
    }

    for (BWAPI::Unit u : combatDragoons_)
    {
        if (u && u->exists())
        {
            candidates.push_back(u);
        }
    }

    for (BWAPI::Unit u : candidates)
    {
        if (!u || !u->exists())
        {
            continue;
        }

        if (isNearActiveCombatSquad(u))
        {
            tryReturnScoutToCombat(u);
        }
    }
}

/// <summary>
/// Returns a combat scout unit back to combat control.
/// 
/// Removes scouting behavior and reassigns the unit to CombatManager.
/// </summary>
/// <param name="unit">Unit to return</param>
/// <returns>True if reassignment succeeded</returns>

bool ScoutingManager::tryReturnScoutToCombat(BWAPI::Unit unit)
{
    if (!unit || !unit->exists() || !commanderRef)
    {
        return false;
    }

    const BWAPI::UnitType t = unit->getType();

    // ONLY ScoutingZealot units (Zealot + Dragoon)
    if (t != BWAPI::UnitTypes::Protoss_Zealot &&
        t != BWAPI::UnitTypes::Protoss_Dragoon)
    {
        return false;
    }

    if (!isScout(unit))
    {
        return false;
    }

    const int id = unit->getID();

    // If this was the proxy patrol unit, clear it
    if (id == proxyPatrolZealotId_)
    {
        proxyPatrolZealotId_ = -1;
    }

    // Remove behavior
    auto it = behaviors_.find(id);
    if (it != behaviors_.end())
    {
        visit_onUnitDestroy(it->second, unit);
        behaviors_.erase(it);
    }

    // Remove from scout tracking
    unmarkScout(unit);

    // Give back to combat
    if (commanderRef->combatManager.assignUnit(unit))
    {
        lockCombatScoutAssignments(kCombatScoutLockFrames_);
        return true;
    }

    return false;
}

bool ScoutingManager::isNearActiveCombatSquad(BWAPI::Unit unit) const
{
    if (!unit || !unit->exists() || !commanderRef)
    {
        return false;
    }

    auto nearAnySquad =
        [&](const std::vector<Squad*>& squads) -> bool
        {
            for (const Squad* squad : squads)
            {
                if (!squad || !squad->leader || !squad->leader->exists())
                {
                    continue;
                }

                if (unit->getDistance(squad->leader) <= kScoutReturnToCombatDistPx_)
                {
                    return true;
                }
            }

            return false;
        };

    if (nearAnySquad(CombatManager::AttackingSquads))
    {
        return true;
    }

    if (nearAnySquad(CombatManager::ReinforcingSquads))
    {
        return true;
    }

    return false;
}

void ScoutingManager::lockCombatScoutAssignments(int frames)
{
    const int now = BWAPI::Broodwar->getFrameCount();
    combatScoutLockUntilFrame_ = std::max(combatScoutLockUntilFrame_, now + frames);
}

/// <summary>
/// Retrieves an available observer and removes it from scouting.
/// 
/// Transfers control back to combat systems.
/// </summary>
/// <returns>Observer unit if available, otherwise nullptr</returns>

BWAPI::Unit ScoutingManager::getAvaliableDetectors()
{
    for (auto it = observerScouts_.begin(); it != observerScouts_.end(); ++it)
    {
        BWAPI::Unit obs = *it;
        if (!obs || !obs->exists())
            continue;

        const int id = obs->getID();

        // 1) Remove behavior so ScoutingManager stops controlling it
        auto bIt = behaviors_.find(id);
        if (bIt != behaviors_.end())
        {
            // Let the behavior clean itself up
            visit_onUnitDestroy(bIt->second, obs);
            behaviors_.erase(bIt);
        }

        // 2) Release observer slot
        releaseObserverSlot(id);

        // 3) Remove from scout bookkeeping
        observerScouts_.erase(it);

        // 4) Done: return unit to combat
        //BWAPI::Broodwar->printf("[SM] Reassigning Observer %d to combat", id);
        return obs;
    }

    // No available observers
    return nullptr;
}


