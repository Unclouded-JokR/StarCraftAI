#include "ScoutingProbe.h"
#include "ProtoBotCommander.h"
#include "ScoutingManager.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <functional>
#include <chrono>

using namespace BWAPI;
using namespace BWEM;

namespace {
    inline double deg2rad(double d) { return d * 3.1415926535 / 180.0; }
    inline Position orbitPoint(const Position& c, int r, int aDeg) {
        return Position(c.x + int(r * std::cos(deg2rad(aDeg))),
            c.y + int(r * std::sin(deg2rad(aDeg))));
    }
    static inline int mapWpx() { return BWAPI::Broodwar->mapWidth() * 32; }
    static inline int mapHpx() { return BWAPI::Broodwar->mapHeight() * 32; }

    using Clock = std::chrono::high_resolution_clock;

    static bool IsFreeForUnitFootprint(const BWAPI::Position& center, BWAPI::UnitType ut)
    {
        if (!center.isValid())
        {
            return false;
        }

        const BWAPI::Position topLeft(center.x - ut.width() / 2, center.y - ut.height() / 2);
        const BWAPI::Position bottomRight(center.x + ut.width() / 2, center.y + ut.height() / 2);

        if (!topLeft.isValid() || !bottomRight.isValid())
        {
            return false;
        }

        const BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRectangle(
            topLeft,
            bottomRight,
            BWAPI::Filter::IsBuilding || (BWAPI::Filter::IsNeutral && !BWAPI::Filter::IsCritter)
        );

        return units.empty();
    }

    struct ScopedMsTimer
    {
        const char* label;
        int* outMs;
        Clock::time_point t0;

        ScopedMsTimer(const char* lbl, int* out)
            : label(lbl)
            , outMs(out)
            , t0(Clock::now())
        {
        }

        ~ScopedMsTimer()
        {
            const auto t1 = Clock::now();
            const int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            if (outMs)
            {
                *outMs = ms;
            }
        }
    };

    static void debugEveryNFrames(int& lastPrintFrame, int everyNFrames, const std::string& msg)
    {
        const int now = BWAPI::Broodwar->getFrameCount();
        if (now - lastPrintFrame < everyNFrames)
        {
            return;
        }
        lastPrintFrame = now;
        //BWAPI::Broodwar->printf("%s", msg.c_str());
    }

    static bool shouldPrintEveryNFrames(int& lastPrintFrame, int everyNFrames)
    {
        const int now = BWAPI::Broodwar->getFrameCount();
        if (now - lastPrintFrame < everyNFrames)
        {
            return false;
        }
        lastPrintFrame = now;
        return true;
    }

    static void debugEveryNFramesIfMsUnder
    (
        int& lastPrintFrame,
        int everyNFrames,
        int measuredMs,
        int limitMs,
        const std::function<std::string()>& buildMsg
    )
    {
        if (measuredMs > limitMs)
        {
            return;
        }

        if (!shouldPrintEveryNFrames(lastPrintFrame, everyNFrames))
        {
            return;
        }

        const std::string msg = buildMsg();
        //BWAPI::Broodwar->printf("%s", msg.c_str());
    }
}

void ScoutingProbe::onStart() {
    if (!Map::Instance().Initialized()) {
        //Broodwar->printf("ScoutingProbe: BWEM not initialized.");
        return;
    }
    buildStartTargets();
    enemyMainTile.reset();
    enemyMainPos = Positions::Invalid;
    gasStealDone = false;
    targetGeyser = nullptr;
    nextGasStealRetryFrame = 0;
    lastMoveIssueFrame = 0;
    orbitWaypoints.clear();
    orbitIdx = 0;
    dwellUntilFrame = 0;
    gasStealHoldingForMinerals = false;
    nextMineralCheckFrame = 0;
    state = startTargets.empty() ? State::Done : State::Search;
    aStarEscapeUntilFrame = 0;
    aStarEscapeGoal = BWAPI::Positions::Invalid;

    lastStuckPos = BWAPI::Positions::Invalid;
    lastStuckCheckFrame = 0;
    stuckFrames = 0;
}

void ScoutingProbe::assign(BWAPI::Unit unit) {
    if (!unit || !unit->exists()) return;
    scout = unit;
    scout->stop();
    //Broodwar->printf("[Scouting] Assigned scout: %s (id=%d)", scout->getType().c_str(), scout->getID());
    lastHP = scout->getHitPoints();
    lastShields = scout->getShields();
    aStarEscapeUntilFrame = 0;
    aStarEscapeGoal = BWAPI::Positions::Invalid;
    stuckFrames = 0;
    if (scout->isCarryingMinerals() || scout->isCarryingGas())
        state = State::ReturningCargo;
    else
        state = startTargets.empty() ? State::Done : State::Search;
}

void ScoutingProbe::onUnitDestroy(BWAPI::Unit unit) {
    if (unit && scout && unit == scout) {
        //Broodwar->printf("[Scouting] Scout destroyed. Clearing assignment.");
        scout = nullptr;
        state = State::Done;
    }
}

void ScoutingProbe::onFrame() {
    if (!scout || !scout->exists() || state == State::Done) return;

    const int now = Broodwar->getFrameCount();
    const int hp = scout->getHitPoints();
    const int sh = scout->getShields();
    const bool tookDamage = (lastHP >= 0 && lastShields >= 0) && (hp + sh) < (lastHP + lastShields);
    const bool imminent = scout->isUnderAttack() || threatenedNow();

    if ((tookDamage || imminent) && (now - lastThreatFrame >= kThreatRearmFrames)) {
        lastThreatFrame = now;
        ensureOrbitWaypoints();

        if (!hasPlannedPath() || scout->getDistance(currentPlannedWaypoint()) < 80) {
            if (hasPlannedPath()) plannedPath.erase(plannedPath.begin());
            if (!hasPlannedPath()) planTerrainPathTo(currentOrbitPoint());
        }
        const auto wp = currentPlannedWaypoint();
        if (wp.isValid()) issueMoveToward(wp, /*reissue*/48);
        state = State::Orbit;
        lastHP = hp; lastShields = sh;
        return;
    }

    switch (state) {
    case State::ReturningCargo:
        handleReturningCargoState();
        break;

    case State::Search:
    {
        if (nextTarget >= (int)startTargets.size())
        {
            state = State::Done;
            break;
        }

        const auto tp = startTargets[nextTarget];
        const BWAPI::Position goal(tp);

        if (plannedPath.empty() || !lastPlannedGoal.isValid() || lastPlannedGoal.getApproxDistance(goal) > 32)
        {
            planTerrainPathTo(goal);
            lastPlannedGoal = goal;
            nextReplanFrame = now + 999999;
        }

        followPlannedPath(goal, 64);

        if (seeAnyEnemyBuildingNear(goal, 800))
        {
            //Broodwar->printf("[Scouting] Enemy main at (%d,%d)", tp.x, tp.y);

            if (manager && !manager->getEnemyMain().has_value())
            {
                manager->setEnemyMain(tp);
            }
            else
            {
                enemyMainTile = tp;
                enemyMainPos = goal;
                state = State::GasSteal;
            }

            break;
        }

        if (scout->getDistance(goal) <= kCloseEnoughToTarget)
        {
            ++nextTarget;
        }

        break;
    }

    case State::GasSteal:
        if (tryGasSteal()) break;
        if (gasStealDone) state = State::Harass;
        break;

    case State::Harass:
        if (enemyMainPos.isValid() && tryHarassWorker()) break;
        state = State::Orbit;
        break;

    case State::Orbit:
    {
        if (threatenedNow())
        {
            lastThreatFrame = now;
        }

        if (enemyMainPos.isValid()
            && (now - lastThreatFrame) >= kCalmFramesToResumeHarass)
        {
            plannedPath.clear();
            aStarEscapeUntilFrame = 0;
            aStarEscapeGoal = BWAPI::Positions::Invalid;
            stuckFrames = 0;
            state = State::Harass;
            break;
        }

        ensureOrbitWaypoints();

        // If we are currently in escape mode, follow A* until timeout or success
        if (aStarEscapeUntilFrame > now && aStarEscapeGoal.isValid())
        {
            followPlannedPath(aStarEscapeGoal, 64);

            // If we got far enough from where we were jammed, return to orbit
            if (scout->getDistance(aStarEscapeGoal) < 96)
            {
                aStarEscapeUntilFrame = 0;
                aStarEscapeGoal = BWAPI::Positions::Invalid;
                plannedPath.clear();
                stuckFrames = 0;
            }

            advanceOrbitIfArrived();
            break;
        }

        // Normal mode: old orbit movement (no A*)
        issueMoveToward(currentOrbitPoint(), 64, false);
        advanceOrbitIfArrived();

        // Detect jam and trigger A* escape
        if (isStuck(now))
        {
            aStarEscapeGoal = computeEscapeGoal();

            if (aStarEscapeGoal.isValid())
            {
                plannedPath.clear();
                planAStarPathTo(aStarEscapeGoal, false);

                // stay in escape mode for ~2 seconds max
                aStarEscapeUntilFrame = now + 48;

                // prevent immediate re-trigger loop
                stuckFrames = 0;
            }
        }

        break;
    }

    case State::Done:
    default:
        break;
    }
}

void ScoutingProbe::setEnemyMain(const TilePosition& tp) {
    enemyMainTile = tp;
    enemyMainPos = Position(tp);
    state = State::GasSteal;
}

/* ---------- returning cargo ---------- */
void ScoutingProbe::handleReturningCargoState() {
    if (!scout) return;
    if (!scout->isCarryingMinerals() && !scout->isCarryingGas()) {
        state = startTargets.empty() ? State::Done : State::Search;
        return;
    }
    if (scout->getOrder() != Orders::ReturnMinerals && scout->getOrder() != Orders::ReturnGas) {
        if (!scout->returnCargo()) {
            if (BWAPI::Unit depot = findClosestDepot(scout->getPosition())) {
                scout->rightClick(depot);
            }
        }
    }
    //Broodwar->drawTextMap(scout->getPosition(), "Returning cargo...");
}

BWAPI::Unit ScoutingProbe::findClosestDepot(const BWAPI::Position& from) const {
    BWAPI::Unit best = nullptr; int bestDist = INT_MAX;
    for (auto& u : BWAPI::Broodwar->self()->getUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isResourceDepot()) continue;
        int d = u->getDistance(from);
        if (d < bestDist) { bestDist = d; best = u; }
    }
    return best;
}

/* ---------- helpers (your originals) ---------- */
void ScoutingProbe::buildStartTargets() {
    startTargets.clear();
    const TilePosition myStart = Broodwar->self()->getStartLocation();
    for (const auto& area : Map::Instance().Areas()) {
        for (const auto& base : area.Bases()) {
            if (base.Starting() && base.Location() != myStart) {
                startTargets.push_back(base.Location());
            }
        }
    }
    std::sort(startTargets.begin(), startTargets.end(),
        [&](const TilePosition& a, const TilePosition& b) {
            return Position(myStart).getDistance(Position(a)) <
                Position(myStart).getDistance(Position(b));
        });
    //Broodwar->printf("[Scouting] %d start targets.", static_cast<int>(startTargets.size()));
}

void ScoutingProbe::issueMoveToward(const Position& p, int reissueDist, bool force)
{
    if (!scout || !scout->exists() || !p.isValid())
    {
        return;
    }

    if (!force && Broodwar->getFrameCount() - lastMoveIssueFrame < kMoveCooldownFrames)
    {
        return;
    }

    BWAPI::Position target = p;

    target = snapToNearestWalkableClear(target, scout->getType(), 192);

    // If we are trying to move into a building footprint, trigger A* escape mode
    if (!IsFreeForUnitFootprint(target, scout->getType()))
    {
        aStarEscapeGoal = snapToNearestWalkableClear(target, scout->getType(), 256);
        if (!aStarEscapeGoal.isValid())
        {
            aStarEscapeGoal = snapToNearestWalkable(target, 256);
        }

        if (aStarEscapeGoal.isValid())
        {
            plannedPath.clear();
            planAStarPathTo(aStarEscapeGoal, false);
            aStarEscapeUntilFrame = Broodwar->getFrameCount() + 48;
            stuckFrames = 0;
            return;
        }
    }

    if (force || scout->getDistance(target) > reissueDist || !scout->isMoving())
    {
        scout->move(target);
        currentMoveGoal = target;
        lastMoveIssueFrame = Broodwar->getFrameCount();
    }

    //Broodwar->drawLineMap(scout->getPosition(), target, Colors::Yellow);
}



bool ScoutingProbe::seeAnyEnemyBuildingNear(const Position& p, int radius) const {
    for (auto& u : Broodwar->enemy()->getUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isResourceDepot()) continue;
        if (u->getPosition().isValid() && u->getDistance(p) <= radius) return true;
    }
    return false;
}

bool ScoutingProbe::anyRefineryOn(BWAPI::Unit geyser) const {
    if (!geyser || !geyser->exists()) return false;
    const TilePosition tp = geyser->getTilePosition();
    for (auto& u : Broodwar->getAllUnits()) {
        if (!u || !u->exists()) continue;
        if (!u->getType().isRefinery()) continue;
        if (u->getTilePosition() == tp) return true;
    }
    return false;
}

bool ScoutingProbe::tryGasSteal()
{
    if (gasStealDone)
    {
        return false;
    }

    if (!scout || !scout->exists() || !scout->getType().isWorker())
    {
        return false;
    }

    if (!enemyMainPos.isValid())
    {
        return false;
    }

    const int now = Broodwar->getFrameCount();

    if (now < nextGasStealRetryFrame)
    {
        return false;
    }

    if (Broodwar->self()->getRace() != Races::Protoss)
    {
        gasStealDone = true;
        return false;
    }

    // 1) Strategy permission (for now assume true)
    const bool allowedByStrategy = commanderRef->shouldGasSteal();
    if (!allowedByStrategy)
    {
        gasStealDone = true;
        return false;
    }

    // If approved but we don't have minerals yet, orbit and wait
    if (gasStealApproved && Broodwar->self()->minerals() < UnitTypes::Protoss_Assimilator.mineralPrice())
    {
        gasStealHoldingForMinerals = true;

        // Keep the scout near enemy main, but don't harass while waiting
        ensureOrbitWaypoints();
        issueMoveToward(currentOrbitPoint(), 64, false);
        advanceOrbitIfArrived();

        // Don't spam retry every frame
        nextGasStealRetryFrame = now + 12;

        return true; // stay in GasSteal state
    }

    gasStealHoldingForMinerals = false;
    // 2) Find enemy geyser once
    if (!targetGeyser || !targetGeyser->exists())
    {
        BWAPI::Unit best = nullptr;
        int bestDist = INT_MAX;

        for (auto g : Broodwar->getGeysers())
        {
            if (!g || !g->exists())
            {
                continue;
            }

            const int d = enemyMainPos.getDistance(g->getPosition());
            if (d > 400)
            {
                continue;
            }

            if (anyRefineryOn(g))
            {
                gasStealDone = true;
                return false;
            }

            if (d < bestDist)
            {
                bestDist = d;
                best = g;
            }
        }

        targetGeyser = best;

        if (!targetGeyser)
        {
            gasStealDone = true;
            return false;
        }
    }

    if (anyRefineryOn(targetGeyser))
    {
        gasStealDone = true;
        return false;
    }

    // 3) Make the request once
    if (!gasStealRequested)
    {
        if (commanderRef)
        {
            commanderRef->requestCheese(UnitTypes::Protoss_Assimilator, scout);
        }

        gasStealRequested = true;
        gasStealRequestFrame = now;
        nextCheesePollFrame = now;
        //Broodwar->printf("[GasSteal] Requested Assimilator cheese (unit=%d).", scout->getID());
    }

    // 4) Poll approval (throttle to avoid spamming)
    if (!gasStealApproved && now >= nextCheesePollFrame)
    {
        gasStealApproved = commanderRef ? commanderRef->checkCheeseRequest(scout) : true;
        nextCheesePollFrame = now + 12;
    }

    if (!gasStealApproved)
    {
        // While waiting for approval, keep moving toward the geyser
        if (scout->getDistance(targetGeyser) > 96 || !scout->isMoving())
        {
            planAStarPathTo(targetGeyser->getPosition(), true);
            followPlannedPath(targetGeyser->getPosition(), 64);
            nextGasStealRetryFrame = now + kGasStealRetryCooldown;
            return true;
        }

        return true;
    }

    // 5) Once approved, ensure we can afford it
    if (Broodwar->self()->minerals() < UnitTypes::Protoss_Assimilator.mineralPrice())
    {
        nextGasStealRetryFrame = now + kGasStealRetryCooldown;
        return true;
    }

    // 6) Go build it
    const TilePosition gtp = targetGeyser->getTilePosition();

    if (scout->getDistance(targetGeyser) > 96)
    {
        planAStarPathTo(targetGeyser->getPosition(), true);
        followPlannedPath(targetGeyser->getPosition(), 64);
        nextGasStealRetryFrame = now + kGasStealRetryCooldown;
        return true;
    }
    // Try to issue the build (may take multiple frames)
    if (BWAPI::Unit a = findAssimilatorOnTargetGeyser())
    {
        //BWAPI::Broodwar->printf("[GasSteal] Assimilator started (id=%d).", a->getID());
        gasStealDone = true;
        return true;
    }

    if (scout->build(BWAPI::UnitTypes::Protoss_Assimilator, gtp))
    {
        //BWAPI::Broodwar->printf("[GasSteal] Build command accepted at (%d,%d). Waiting for start...", gtp.x, gtp.y);
        nextGasStealRetryFrame = now + 12; // short retry window while we wait for it to appear
        return true;
    }

    // If build fails this frame, nudge and retry later
    planAStarPathTo(targetGeyser->getPosition(), true);
    followPlannedPath(targetGeyser->getPosition(), 64);
    nextGasStealRetryFrame = now + kGasStealRetryCooldown;
    return true;
}


bool ScoutingProbe::tryHarassWorker() {
    if (!scout || !scout->exists() || !enemyMainPos.isValid()) return false;
    BWAPI::Unit best = nullptr; int bestDist = 999999;
    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists()) continue;
        if (!e->getType().isWorker()) continue;
        if (e->getDistance(enemyMainPos) > kHarassRadiusFromMain) continue;
        int d = e->getDistance(scout);
        if (d < bestDist) { bestDist = d; best = e; }
    }
    if (!best) return false;
    if (Broodwar->getFrameCount() - lastMoveIssueFrame >= kMoveCooldownFrames) {
        scout->attack(best);
        lastMoveIssueFrame = Broodwar->getFrameCount();
    }
    //Broodwar->drawCircleMap(best->getPosition(), 10, Colors::Red, true);
    return true;
}

void ScoutingProbe::ensureOrbitWaypoints() {
    if (!orbitWaypoints.empty())
    {
        return;
    }

    int buildMs = 0;
    {
        ScopedMsTimer t("ensureOrbitWaypoints", &buildMs);

        orbitWaypoints.clear();
        orbitWaypoints.reserve(8);

        const Position center = clampToMapPx(computeOrbitCenter(), 64);
        const int radius = kOrbitRadius;

        int snapTotalMs = 0;

        for (int deg = 0; deg < 360; deg += 45)
        {
            Position raw = orbitPoint(center, radius, deg);
            raw = clampToMapPx(raw, 48);

            int snapMs = 0;
            Position snapped;
            {
                ScopedMsTimer ts("snapToNearestWalkable", &snapMs);
                const BWAPI::UnitType ut = (scout && scout->exists()) ? scout->getType() : BWAPI::UnitTypes::Protoss_Probe;
                snapped = snapToNearestWalkableClear(raw, ut, 160);
            }

            snapTotalMs += snapMs;

            if (snapped.isValid())
            {
                orbitWaypoints.push_back(snapped);
            }
        }

        dbgLastSnapMs = snapTotalMs;

        if (orbitWaypoints.empty())
        {
            orbitWaypoints.push_back(center);
        }

        Position threat = getAvgPosition();
        int oppDeg = threat.isValid()
            ? angleDeg(center, threat) + 180
            : (scout && scout->exists() ? angleDeg(center, scout->getPosition()) + 180 : 0);

        oppDeg = normDeg(oppDeg);

        const int step = 45;
        int startIdx = (oppDeg + step / 2) / step;
        orbitIdx = static_cast<std::size_t>(startIdx % int(orbitWaypoints.size()));
        dwellUntilFrame = 0;
    }

    dbgLastOrbitBuildMs = buildMs;

    /*debugEveryNFramesIfMsUnder(
        dbgLastOrbitPrintFrame,
        24,
        dbgLastOrbitBuildMs,
        200,
        [&]()
        {
            return "[Orbit] built waypoints in " + std::to_string(dbgLastOrbitBuildMs) +
                "ms (snap total " + std::to_string(dbgLastSnapMs) + "ms)";
        }
    );*/
}

BWAPI::Position ScoutingProbe::currentOrbitPoint() const {
    if (orbitWaypoints.empty()) return enemyMainPos;
    return orbitWaypoints[orbitIdx % orbitWaypoints.size()];
}

void ScoutingProbe::advanceOrbitIfArrived() {
    const auto goal = currentOrbitPoint();
    if (!goal.isValid()) return;
    const int now = Broodwar->getFrameCount();

    if (scout->getDistance(goal) < 80) {
        if (dwellUntilFrame == 0) dwellUntilFrame = now + 12;
        if (now >= dwellUntilFrame) {
            orbitIdx = (orbitIdx + 1) % orbitWaypoints.size();
            dwellUntilFrame = 0;
        }
    }
    else {
        dwellUntilFrame = 0;
    }
}

bool ScoutingProbe::threatenedNow() const {
    if (!scout || !scout->exists()) return false;
    if (scout->isUnderAttack()) return true;

    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists()) continue;
        if (e->getType().isWorker()) continue;
        if (!e->getType().canAttack()) continue;
        if (e->getDistance(scout) < 192) return true;
    }
    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists() || !e->getType().isWorker()) continue;
        if (e->getDistance(scout) <= 112 &&
            (e->isAttacking() || e->getOrder() == Orders::AttackUnit || e->getTarget() == scout))
            return true;
    }
    return false;
}

BWAPI::Position ScoutingProbe::getAvgPosition() {
    long long sumX = 0, sumY = 0; int numUnits = 0;
    const Unitset& group = Broodwar->enemy()->getUnits();
    for (auto u : group) {
        if (!u || !u->exists()) continue;
        const Position p = u->getPosition();
        if (!p.isValid()) continue;
        sumX += p.x; sumY += p.y; ++numUnits;
    }
    return (numUnits > 0) ? Position(int(sumX / numUnits), int(sumY / numUnits)) : Positions::Invalid;
}

int ScoutingProbe::angleDeg(const Position& from, const Position& to) {
    const double ang = std::atan2(double(to.y - from.y), double(to.x - from.x)) * 180.0 / 3.141592653589793;
    int d = int(std::lround(ang)); d %= 360; if (d < 0) d += 360; return d;
}

BWAPI::Position ScoutingProbe::computeOrbitCenter() const {
    if (enemyMainPos.isValid())
    {
        return enemyMainPos;
    }

    if (scout && scout->exists())
    {
        return scout->getPosition();
    }

    return Position(0, 0);
}

bool ScoutingProbe::planTerrainPathTo(const Position& goal) {
    plannedPath.clear();
    if (!scout || !scout->exists() || !goal.isValid()) return false;
    const auto& cpPath = BWEM::Map::Instance().GetPath(scout->getPosition(), goal, nullptr);
    if (cpPath.empty()) { plannedPath.push_back(goal); return true; }
    for (const auto* cp : cpPath) {
        if (!cp) continue;
        plannedPath.push_back(Position(cp->Center()));
    }
    plannedPath.push_back(goal);
    return true;
}

BWAPI::Position ScoutingProbe::currentPlannedWaypoint() const {
    if (plannedPath.empty()) return Positions::Invalid;
    return plannedPath.front();
}

BWAPI::Position ScoutingProbe::computeSidestepTarget(const BWAPI::Position& goal)
{
    if (!scout || !scout->exists() || !goal.isValid())
    {
        return BWAPI::Positions::Invalid;
    }

    const BWAPI::Position from = scout->getPosition();
    const BWAPI::Position d = goal - from;

    if (!d.isValid() || (d.x == 0 && d.y == 0))
    {
        return BWAPI::Positions::Invalid;
    }

    const double len = std::sqrt(double(d.x) * double(d.x) + double(d.y) * double(d.y));
    if (len < 1.0)
    {
        return BWAPI::Positions::Invalid;
    }

    const double nx = double(d.x) / len;
    const double ny = double(d.y) / len;

    // perpendicular unit vector, flip using sidestepDir
    const double px = -ny * double(sidestepDir);
    const double py = nx * double(sidestepDir);

    const int steps[] = { 64, 96, 128, 160 };

    for (int step : steps)
    {
        BWAPI::Position raw(from.x + int(px * step), from.y + int(py * step));

        raw = clampToMapPx(raw, 32);

        BWAPI::Position snapped = snapToNearestWalkable(raw, 160);
        if (snapped.isValid())
        {
            return snapped;
        }
    }

    // swap direction once and retry
    sidestepDir = -sidestepDir;

    for (int step : steps)
    {
        BWAPI::Position raw(from.x + int(-px * step), from.y + int(-py * step));

        raw = clampToMapPx(raw, 32);

        BWAPI::Position snapped = snapToNearestWalkable(raw, 160);
        if (snapped.isValid())
        {
            return snapped;
        }
    }

    return BWAPI::Positions::Invalid;
}


BWAPI::Position ScoutingProbe::clampToMapPx(const Position& p, int marginPx) {
    int x = std::max(marginPx, std::min(p.x, mapWpx() - 1 - marginPx));
    int y = std::max(marginPx, std::min(p.y, mapHpx() - 1 - marginPx));
    return Position(x, y);
}

BWAPI::Position ScoutingProbe::snapToNearestWalkable(Position p, int maxRadiusPx) {
    p = clampToMapPx(p, 32);
    WalkPosition w0(p);
    const int rMax = std::max(1, maxRadiusPx / 8);
    auto toPosCenter = [](const WalkPosition& w) { return Position(w.x * 8 + 4, w.y * 8 + 4); };
    if (w0.isValid() && Broodwar->isWalkable(w0)) return toPosCenter(w0);

    int bestD2 = INT_MAX; WalkPosition best(-1, -1);
    for (int r = 1; r <= rMax; ++r) {
        bool foundThisRing = false;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                WalkPosition w(w0.x + dx, w0.y + dy);
                if (!w.isValid()) continue;
                if (!Broodwar->isWalkable(w)) continue;
                int d2 = dx * dx + dy * dy;
                if (d2 < bestD2) { bestD2 = d2; best = w; foundThisRing = true; }
            }
        }
        if (foundThisRing) break;
    }
    if (best.x != -1) return toPosCenter(best);
    return Positions::Invalid;
}

BWAPI::Position ScoutingProbe::snapToNearestWalkableClear
(
    BWAPI::Position p,
    BWAPI::UnitType ut,
    int maxRadiusPx
)
{
    p = clampToMapPx(p, 32);
    BWAPI::WalkPosition w0(p);

    const int rMax = std::max(1, maxRadiusPx / 8);

    auto toPosCenter =
        [](const BWAPI::WalkPosition& w)
        {
            return BWAPI::Position(w.x * 8 + 4, w.y * 8 + 4);
        };

    if (w0.isValid() && BWAPI::Broodwar->isWalkable(w0))
    {
        const BWAPI::Position c = toPosCenter(w0);
        if (IsFreeForUnitFootprint(c, ut))
        {
            return c;
        }
    }

    int bestD2 = INT_MAX;
    BWAPI::WalkPosition best(-1, -1);

    for (int r = 1; r <= rMax; ++r)
    {
        bool foundThisRing = false;

        for (int dy = -r; dy <= r; ++dy)
        {
            for (int dx = -r; dx <= r; ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != r)
                {
                    continue;
                }

                BWAPI::WalkPosition w(w0.x + dx, w0.y + dy);
                if (!w.isValid())
                {
                    continue;
                }

                if (!BWAPI::Broodwar->isWalkable(w))
                {
                    continue;
                }

                const BWAPI::Position c = toPosCenter(w);
                if (!IsFreeForUnitFootprint(c, ut))
                {
                    continue;
                }

                const int d2 = dx * dx + dy * dy;
                if (d2 < bestD2)
                {
                    bestD2 = d2;
                    best = w;
                    foundThisRing = true;
                }
            }
        }

        if (foundThisRing)
        {
            break;
        }
    }

    if (best.x != -1)
    {
        return toPosCenter(best);
    }

    return BWAPI::Positions::Invalid;
}

BWAPI::Unit ScoutingProbe::findAssimilatorOnTargetGeyser() const
{
    if (!targetGeyser || !targetGeyser->exists())
    {
        return nullptr;
    }

    const BWAPI::TilePosition gtp = targetGeyser->getTilePosition();

    for (BWAPI::Unit u : BWAPI::Broodwar->getAllUnits())
    {
        if (!u || !u->exists())
        {
            continue;
        }

        if (u->getType() != BWAPI::UnitTypes::Protoss_Assimilator)
        {
            continue;
        }

        if (u->getTilePosition() == gtp)
        {
            return u;
        }
    }

    return nullptr;
}

bool ScoutingProbe::planAStarPathTo(const BWAPI::Position& goal, bool interactableEndpoint)
{
    plannedPath.clear();

    if (!scout || !scout->exists() || !goal.isValid())
    {
        return false;
    }

    int astarMs = 0;
    BWAPI::Position safeGoal = goal;

    if (!interactableEndpoint)
    {
        const BWAPI::UnitType ut = scout->getType();
        if (!IsFreeForUnitFootprint(safeGoal, ut))
        {
            safeGoal = snapToNearestWalkableClear(safeGoal, ut, 256);
        }
    }

    if (!safeGoal.isValid())
    {
        plannedPath.push_back(goal);
        return true;
    }
    Path p;
    {
        ScopedMsTimer t("AStar::GeneratePath", &astarMs);
        p = AStar::GeneratePath(scout->getPosition(), scout->getType(), safeGoal, interactableEndpoint);
    }
    dbgLastAStarMs = astarMs;

    if (p.positions.empty())
    {
        plannedPath.push_back(goal);
        return true;
    }

    plannedPath = p.positions;

    if (!plannedPath.empty() && scout->getDistance(plannedPath.front()) < 24)
    {
        plannedPath.erase(plannedPath.begin());
    }

    return !plannedPath.empty();
}

void ScoutingProbe::followPlannedPath(const BWAPI::Position& finalGoal, int reissueDist)
{
    if (!scout || !scout->exists() || !finalGoal.isValid())
    {
        return;
    }

    const int now = BWAPI::Broodwar->getFrameCount();

    bool needReplan = false;

    if (now >= nextReplanFrame)
    {
        needReplan = true;
    }

    if (lastPlannedGoal.isValid() && finalGoal.getApproxDistance(lastPlannedGoal) > kGoalChangeReplanDist)
    {
        needReplan = true;
    }

    if (!hasPlannedPath())
    {
        needReplan = true;
    }

    

    if (needReplan)
    {
        int replanMs = 0;
        {
            ScopedMsTimer t("followPlannedPath replan", &replanMs);
            planAStarPathTo(finalGoal, false);
        }
        dbgLastOrbitReplanMs = replanMs;

        lastPlannedGoal = finalGoal;
        nextReplanFrame = now + kReplanFrames;

        /*debugEveryNFramesIfMsUnder(
            dbgLastOrbitPrintFrame,
            24,
            dbgLastOrbitReplanMs,
            200,
            [&]()
            {
                return "[Orbit] replan " + std::to_string(dbgLastOrbitReplanMs) +
                    "ms (A* " + std::to_string(dbgLastAStarMs) +
                    "ms) pathLen=" + std::to_string((int)plannedPath.size());
            }
        );*/
    }

    if (plannedPath.empty())
    {
        issueMoveToward(finalGoal, reissueDist, true);
        return;
    }

    while (!plannedPath.empty() && scout->getDistance(plannedPath.front()) < 72)
    {
        plannedPath.erase(plannedPath.begin());
    }

    BWAPI::Position wp = plannedPath.empty() ? finalGoal : plannedPath.front();
    issueMoveToward(wp, reissueDist, false);
}

bool ScoutingProbe::isStuck(int now)
{
    if (!scout || !scout->exists())
    {
        return false;
    }

    static constexpr int kStuckCheckEvery = 4;
    static constexpr int kStuckMoveEps = 16;
    static constexpr int kStuckTriggerFrames = 8;

    if (now - lastStuckCheckFrame < kStuckCheckEvery)
    {
        return stuckFrames >= kStuckTriggerFrames;
    }

    lastStuckCheckFrame = now;

    const BWAPI::Position cur = scout->getPosition();

    if (!lastStuckPos.isValid())
    {
        lastStuckPos = cur;
        stuckFrames = 0;
        return false;
    }

    const int moved = cur.getApproxDistance(lastStuckPos);
    lastStuckPos = cur;

    if (moved < kStuckMoveEps)
    {
        stuckFrames += kStuckCheckEvery;
    }
    else
    {
        stuckFrames = 0;
    }

    return stuckFrames >= kStuckTriggerFrames;
}

BWAPI::Position ScoutingProbe::computeEscapeGoal() const
{
    if (!scout || !scout->exists())
    {
        return BWAPI::Positions::Invalid;
    }

    const BWAPI::Position from = scout->getPosition();

    // Prefer orbit target, but if it's invalid fall back to enemy main center
    BWAPI::Position baseGoal = currentOrbitPoint();
    if (!baseGoal.isValid())
    {
        baseGoal = enemyMainPos;
    }

    if (!baseGoal.isValid())
    {
        return BWAPI::Positions::Invalid;
    }

    // Push outward away from threat direction to break worker surrounds
    const BWAPI::Position d = baseGoal - from;
    if (!d.isValid() || (d.x == 0 && d.y == 0))
    {
        return snapToNearestWalkable(from, 192);
    }

    const double len = std::sqrt(double(d.x) * double(d.x) + double(d.y) * double(d.y));
    if (len < 1.0)
    {
        return snapToNearestWalkable(from, 192);
    }

    const double nx = double(d.x) / len;
    const double ny = double(d.y) / len;

    // step a bit toward the orbit target so A* can route around minerals/workers
    BWAPI::Position raw(from.x + int(nx * 256.0), from.y + int(ny * 256.0));
    raw = clampToMapPx(raw, 32);

    // Make sure it's walkable-ish
    return snapToNearestWalkable(raw, 256);
}

bool ScoutingProbe::tryConfirmEnemyMainByStartLocations()
{
    if (enemyMainTile.has_value() || startTargets.empty())
    {
        return false;
    }

    BWAPI::Unit found = nullptr;
    BWAPI::TilePosition foundTp = BWAPI::TilePositions::Invalid;

    static constexpr int kStartConfirmRadius = 9000; 

    for (auto& e : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (!e || !e->exists())
        {
            continue;
        }

        if (!e->getType().isBuilding())
        {
            continue;
        }

        const BWAPI::Position bp = e->getPosition();
        if (!bp.isValid())
        {
            continue;
        }

        for (const auto& tp : startTargets)
        {
            const BWAPI::Position sp(tp);
            if (bp.getDistance(sp) <= kStartConfirmRadius)
            {
                found = e;
                foundTp = tp;
                break;
            }
        }

        if (found)
        {
            break;
        }
    }

    if (!found || !foundTp.isValid())
    {
        return false;
    }

    enemyMainTile = foundTp;
    enemyMainPos = BWAPI::Position(foundTp);

    if (manager)
    {
        manager->setEnemyMain(foundTp);
    }

    //BWAPI::Broodwar->printf("[Scouting] Enemy main confirmed near start (%d,%d)", foundTp.x, foundTp.y);
    state = State::GasSteal;
    return true;
}

void ScoutingProbe::drawDebug() const
{
    if (!scout || !scout->exists())
    {
        return;
    }

    BWAPI::Position p = scout->getPosition();

    const char* stateName = "Unknown";
    switch (state)
    {
    case State::Search:
        stateName = "Search";
        break;
    case State::GasSteal:
        stateName = "GasSteal";
        break;
    case State::Harass:
        stateName = "Harass";
        break;
    case State::Orbit:
        stateName = "Orbit";
        break;
    case State::ReturningCargo:
        stateName = "ReturningCargo";
        break;
    case State::Done:
        stateName = "Done";
        break;
    }

    BWAPI::Broodwar->drawCircleMap(p, 18, BWAPI::Colors::Yellow, false);
    BWAPI::Broodwar->drawTextMap(p.x - 40, p.y - 46, "\x03Probe Scout");
    BWAPI::Broodwar->drawTextMap(p.x - 40, p.y - 34, "\x11State: %s", stateName);

    if (enemyMainPos.isValid())
    {
        BWAPI::Broodwar->drawCircleMap(enemyMainPos, 18, BWAPI::Colors::Red, false);
        //BWAPI::Broodwar->drawTextMap(enemyMainPos.x + 8, enemyMainPos.y - 8, "\x08" "Enemy Main");
    }

    if (state == State::Search)
    {
        if (nextTarget < startTargets.size())
        {
            BWAPI::Position tgt(startTargets[nextTarget]);
            BWAPI::Broodwar->drawLineMap(p, tgt, BWAPI::Colors::Green);
            BWAPI::Broodwar->drawCircleMap(tgt, 10, BWAPI::Colors::Green, false);
            BWAPI::Broodwar->drawTextMap(tgt.x + 6, tgt.y - 6, "\x07Search Target");
        }

        for (std::size_t i = 0; i < startTargets.size(); ++i)
        {
            BWAPI::Position tp(startTargets[i]);
            BWAPI::Color c = (i == nextTarget) ? BWAPI::Colors::Green : BWAPI::Colors::White;
            BWAPI::Broodwar->drawCircleMap(tp, 6, c, false);
            BWAPI::Broodwar->drawTextMap(tp.x + 4, tp.y - 4, "#%d", (int)i);
        }
    }

    if (state == State::GasSteal && targetGeyser && targetGeyser->exists())
    {
        BWAPI::Position gp = targetGeyser->getPosition();
        BWAPI::Broodwar->drawLineMap(p, gp, BWAPI::Colors::Orange);
        BWAPI::Broodwar->drawCircleMap(gp, 14, BWAPI::Colors::Orange, false);
        BWAPI::Broodwar->drawTextMap(gp.x + 6, gp.y - 6, "\x10Target Geyser");

        BWAPI::Broodwar->drawTextMap(
            p.x - 40,
            p.y - 22,
            "\x10Req:%d Appr:%d Hold:%d",
            gasStealRequested ? 1 : 0,
            gasStealApproved ? 1 : 0,
            gasStealHoldingForMinerals ? 1 : 0
        );
    }

    if (state == State::Harass)
    {
        BWAPI::Broodwar->drawTextMap(p.x - 40, p.y - 22, "\x08Mode: Harass");
    }

    if (state == State::Orbit)
    {
        BWAPI::Position orbitGoal = currentOrbitPoint();
        if (orbitGoal.isValid())
        {
            BWAPI::Broodwar->drawLineMap(p, orbitGoal, BWAPI::Colors::Cyan);
            BWAPI::Broodwar->drawCircleMap(orbitGoal, 10, BWAPI::Colors::Cyan, false);
            BWAPI::Broodwar->drawTextMap(orbitGoal.x + 6, orbitGoal.y - 6, "\x0fOrbit Goal");
        }

        for (std::size_t i = 0; i < orbitWaypoints.size(); ++i)
        {
            BWAPI::Color c = (i == orbitIdx % (orbitWaypoints.empty() ? 1 : orbitWaypoints.size()))
                ? BWAPI::Colors::Cyan
                : BWAPI::Colors::Blue;

            BWAPI::Broodwar->drawCircleMap(orbitWaypoints[i], 6, c, false);

            if (i + 1 < orbitWaypoints.size())
            {
                BWAPI::Broodwar->drawLineMap(orbitWaypoints[i], orbitWaypoints[i + 1], BWAPI::Colors::Blue);
            }
        }

        if (!orbitWaypoints.empty())
        {
            BWAPI::Broodwar->drawLineMap(
                orbitWaypoints.back(),
                orbitWaypoints.front(),
                BWAPI::Colors::Blue
            );
        }
    }

    if (!plannedPath.empty())
    {
        BWAPI::Position prev = p;
        for (const BWAPI::Position& wp : plannedPath)
        {
            if (!wp.isValid())
            {
                continue;
            }

            BWAPI::Broodwar->drawLineMap(prev, wp, BWAPI::Colors::White);
            BWAPI::Broodwar->drawCircleMap(wp, 4, BWAPI::Colors::White, true);
            prev = wp;
        }
    }

    if (currentMoveGoal.isValid())
    {
        BWAPI::Broodwar->drawLineMap(p, currentMoveGoal, BWAPI::Colors::Green);
        BWAPI::Broodwar->drawCircleMap(currentMoveGoal, 8, BWAPI::Colors::Green, false);
    }

    if (aStarEscapeGoal.isValid())
    {
        BWAPI::Broodwar->drawLineMap(p, aStarEscapeGoal, BWAPI::Colors::Purple);
        BWAPI::Broodwar->drawCircleMap(aStarEscapeGoal, 10, BWAPI::Colors::Purple, false);
        BWAPI::Broodwar->drawTextMap(aStarEscapeGoal.x + 6, aStarEscapeGoal.y - 6, "\x05" "Escape");
    }

}