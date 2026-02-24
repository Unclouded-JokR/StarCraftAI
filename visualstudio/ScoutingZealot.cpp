#include "ScoutingZealot.h"
#include "ProtoBotCommander.h"
#include <algorithm>
#include <cmath>
#include <climits>



using namespace BWAPI;
using namespace BWEM;

namespace {
    static inline int mapWpx() { return Broodwar->mapWidth() * 32; }
    static inline int mapHpx() { return Broodwar->mapHeight() * 32; }

    // squared distance helper
    int d2(const Position& a, const Position& b) {
        const int dx = a.x - b.x, dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    static BWAPI::Position g_myMainPos = BWAPI::Positions::Invalid;

    // Pathing fixes

    bool isWalkablePx(const BWAPI::Position& p)
    {
        if (!p.isValid())
        {
            return false;
        }

        const int wx = p.x / 8;
        const int wy = p.y / 8;

        if (wx < 0 || wy < 0)
        {
            return false;
        }

        if (wx >= BWAPI::Broodwar->mapWidth() * 4 || wy >= BWAPI::Broodwar->mapHeight() * 4)
        {
            return false;
        }

        return BWAPI::Broodwar->isWalkable(wx, wy);
    }

    bool hasGroundPath(const BWAPI::Position& from, const BWAPI::Position& to)
    {
        if (!from.isValid() || !to.isValid())
        {
            return false;
        }

        const auto& path = BWEM::Map::Instance().GetPath(from, to, nullptr);
        return !path.empty();
    }
}

void ScoutingZealot::onStart() {
    // We only act when assigned + enemy main becomes known
    state = State::Idle;
    lastMoveIssueFrame = 0;
    lastThreatFrame = -10000;
    g_myMainPos = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    BWAPI::Broodwar->printf("[ZealotScout] onStart()");
}

void ScoutingZealot::assign(BWAPI::Unit unit) {

    if (!unit || !unit->exists()) return;

    zealot = unit;
    BWAPI::Broodwar->printf("[ZealotScout] assign id=%d type=%s",
        zealot->getID(), zealot->getType().c_str());

    if (isProxyPatroller)
    {
        rebuildProxyPoints();
        state = State::ProxyPatrol;
        return;
    }
    state = enemyMainPos.isValid() ? State::MoveToNatural : State::WaitEnemyMain;
}

void ScoutingZealot::setEnemyMain(const TilePosition& tp) {
    enemyMainTile = tp;
    enemyMainPos = Position(tp);

    // If this zealot is the proxy patrol zealot,
    // do NOT switch state into enemy natural logic.
    if (state == State::ProxyPatrol)
    {
        return;
    }

    computeEnemyNatural();
    state = enemyNaturalPos.isValid() ? State::MoveToNatural : State::WaitEnemyMain;

    // Immediate course set to the natural edge perch (don’t step into main first)
    if (enemyNaturalPos.isValid()) {
        issueMove(pickEdgeOfVisionSpot(), /*force*/true);
        BWAPI::Broodwar->printf("[ZealotScout] setEnemyMain (%d,%d)", tp.x, tp.y);
    }
}
void ScoutingZealot::onUnitDestroy(BWAPI::Unit unit) {
    if (zealot && unit == zealot) {
        zealot = nullptr;
        state = State::Done;
    }
}

void ScoutingZealot::onFrame() {
    if (!zealot || !zealot->exists() || state == State::Done) return;
    BWAPI::Broodwar->drawTextMap(zealot->getPosition(),
        "\x11Zealot[%d] %s", zealot->getID(),
           (state == State::Idle ?              "Idle" :
            state == State::ProxyPatrol ?       "ProxyPatrol" :
            state == State::WaitEnemyMain ?     "WaitEnemyMain" :
            state == State::MoveToNatural ?     "MoveToNatural" :
            state == State::HoldEdge ?          "HoldEdge" :
            state == State::Reposition ?        "Reposition" : "Done"));

    // --- hard avoid enemy main every frame ---
    if (enemyMainTile.has_value() && zealot && zealot->exists()) {
        const auto* mainArea = BWEM::Map::Instance().GetArea(*enemyMainTile);
        const auto* hereArea = BWEM::Map::Instance().GetArea(zealot->getTilePosition());
        if (mainArea && hereArea == mainArea) {
            BWAPI::Broodwar->drawTextMap(zealot->getPosition(), "\x08[AVOID MAIN]");
            issueMove(pickEdgeOfVisionSpot(), /*force*/true);
            return;
        }
    }

    switch (state) {
    case State::Idle:
        // wait until we know the main
        if (isProxyPatroller)
        {
            rebuildProxyPoints();
            state = State::ProxyPatrol;
        }
        else
        {
            state = enemyMainPos.isValid() ? State::MoveToNatural : State::WaitEnemyMain;
        }
        break;

    case State::WaitEnemyMain:
        // commander will call setEnemyMain() once your Probe finds it
        if (commanderRef) 
        {
            const auto& e = commanderRef->enemy();
            if (e.main.has_value()) 
            {
                setEnemyMain(*e.main);       
                break;
            }
        }
        break;

    case State::MoveToNatural:
    {
        if (!enemyMainPos.isValid())
        {
            state = State::WaitEnemyMain;
            break;
        }

        if (!enemyNaturalPos.isValid())
        {
            computeEnemyNatural();
        }

        BWAPI::Unit threat = findPrimaryThreat(kThreatRadiusPx);

        if (threat || zealot->isUnderAttack())
        {
            lastThreatFrame = BWAPI::Broodwar->getFrameCount();
            returnStateAfterReposition = State::MoveToNatural;
            state = State::Reposition;

            retreatHomeMicro(threat);
            break;
        }

        if (enemyNaturalPos.isValid())
        {
            const BWAPI::Position perch = pickEdgeOfVisionSpot();
            issueMove(perch);

            if (zealot->getDistance(perch) < 96)
            {
                state = State::HoldEdge;
            }
        }

        break;
    }

    case State::HoldEdge:
    {
        BWAPI::Unit threat = findPrimaryThreat(kThreatRadiusPx);

        if (threat || zealot->isUnderAttack())
        {
            lastThreatFrame = BWAPI::Broodwar->getFrameCount();
            returnStateAfterReposition = State::HoldEdge;
            state = State::Reposition;

            retreatHomeMicro(threat);
            break;
        }



        break;
    }

    case State::Reposition:
    {
        const int now = BWAPI::Broodwar->getFrameCount();
        BWAPI::Unit threat = findPrimaryThreat(kThreatRadiusPx);

        if (threat || zealot->isUnderAttack())
        {
            lastThreatFrame = now;
            retreatHomeMicro(threat);
            break;
        }

        if (now - lastThreatFrame >= kCalmFramesToReturn)
        {
            if (returnStateAfterReposition == State::MoveToNatural)
            {
                issueMove(pickEdgeOfVisionSpot(), true);
                state = State::MoveToNatural;
            }
            else
            {
                issueMove(pickEdgeOfVisionSpot(), true);
                state = State::HoldEdge;
            }

            break;
        }

        break;
    }
    case State::ProxyPatrol:
    {
        const int now = Broodwar->getFrameCount();

        // If zealot is missing points, rebuild here.
        if (proxyPoints.empty())
        {
            rebuildProxyPoints();
        }

        if (proxyPoints.empty())
        {
            // fallback: just stand
            if (!zealot->isMoving() && !zealot->isAttacking())
            {
                zealot->holdPosition();
            }
            break;
        }

        // advance target when reached
        if (!proxyCurTarget.isValid() || zealot->getDistance(proxyCurTarget) <= 96)
        {
            if (proxyNextIdx >= (int)proxyPoints.size())
            {
                proxyNextIdx = 0;
            }

            proxyCurTarget = proxyPoints[proxyNextIdx++];
        }

        // throttle move commands using existing issueMove cooldown
        issueMove(proxyCurTarget, /*force*/false, /*reissueDist*/64);

        break;
    }

    case State::Done:
    default:
        break;
    }

    // tiny debug
    if (enemyNaturalPos.isValid())
        Broodwar->drawCircleMap(enemyNaturalPos, 12, Colors::Cyan, true);
}

/* -------- helpers -------- */

// retreat target: go home
BWAPI::Position ScoutingZealot::homeRetreatPoint() const
{
    BWAPI::Position home = g_myMainPos.isValid()
        ? g_myMainPos
        : BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    return ScoutingZealot::clampToMapPx(home, 32);
}

void ScoutingZealot::computeEnemyNatural() {
    enemyNaturalTile = BWAPI::TilePositions::Invalid;
    enemyNaturalPos = BWAPI::Positions::Invalid;
    if (!enemyMainTile.has_value()) return;

    const BWAPI::TilePosition enemyMain = *enemyMainTile;
    const BWAPI::Position enemyMainCenter = BWAPI::Position(enemyMain) + BWAPI::Position(16, 16);

    const BWEM::Area* mainArea = BWEM::Map::Instance().GetArea(enemyMain);
    double bestDist = 1e30;

    // Look at all non-starting bases NOT in the enemy main's area; pick shortest ground path
    for (const BWEM::Area& area : BWEM::Map::Instance().Areas()) {
        for (const BWEM::Base& base : area.Bases()) {
            if (base.Starting()) continue;

            const BWEM::Area* bArea = BWEM::Map::Instance().GetArea(base.Location());
            if (bArea == mainArea) continue; // don't consider bases inside enemy main area

            BWAPI::Position basePos = BWAPI::Position(base.Location());
            const auto& path = BWEM::Map::Instance().GetPath(enemyMainCenter, basePos, nullptr);
            if (path.empty()) continue; // unreachable

            double gdist = ScoutingZealot::groundPathLengthPx(enemyMainCenter, basePos);
            if (gdist < bestDist) {
                bestDist = gdist;
                enemyNaturalTile = base.Location();
                enemyNaturalPos = basePos;
            }
        }
    }

    if (enemyNaturalPos.isValid()) {
        Broodwar->drawCircleMap(enemyNaturalPos, 10, Colors::Cyan, true);
        Broodwar->drawTextMap(enemyNaturalPos, "nat");
    }

}

BWAPI::Position ScoutingZealot::pickEdgeOfVisionSpot()
{
    if (!zealot || !zealot->exists())
    {
        return BWAPI::Positions::Invalid;
    }

    if (!enemyNaturalPos.isValid())
    {
        return enemyNaturalPos;
    }

    const int now = BWAPI::Broodwar->getFrameCount();

    // Reuse cached perch for a bit to prevent thrash.
    if (cachedPerch.isValid())
    {
        const bool recent = (now - cachedPerchFrame) < kPerchRecalcFrames;
        const bool closeEnough = zealot->getDistance(cachedPerch) <= 96;

        if (recent || closeEnough)
        {
            return cachedPerch;
        }
    }

    const auto* natArea = BWEM::Map::Instance().GetArea(enemyNaturalTile);
    const auto* mainArea = enemyMainTile.has_value()
        ? BWEM::Map::Instance().GetArea(*enemyMainTile)
        : nullptr;

    BWAPI::Position natCenter = enemyNaturalPos;
    if (natArea)
    {
        for (const auto& b : natArea->Bases())
        {
            if (b.Location() == enemyNaturalTile)
            {
                long long sx = 0;
                long long sy = 0;
                int n = 0;

                for (auto* m : b.Minerals())
                {
                    if (!m)
                    {
                        continue;
                    }

                    BWAPI::Position p(m->Pos());
                    if (!p.isValid())
                    {
                        continue;
                    }

                    sx += p.x;
                    sy += p.y;
                    ++n;
                }

                if (n > 0)
                {
                    natCenter = BWAPI::Position(int(sx / n), int(sy / n));
                }

                break;
            }
        }
    }

    BWAPI::Position myCenter = g_myMainPos.isValid()
        ? g_myMainPos
        : BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

    int dirx = myCenter.x - natCenter.x;
    int diry = myCenter.y - natCenter.y;
    if (dirx == 0 && diry == 0)
    {
        dirx = 1;
    }

    const int sight = BWAPI::UnitTypes::Protoss_Zealot.sightRange();
    const int want = std::max(0, sight - kEdgeMarginPx);

    const double len = std::max(1.0, std::sqrt(double(dirx * dirx + diry * diry)));

    BWAPI::Position candidate(
        natCenter.x + int((dirx / len) * want),
        natCenter.y + int((diry / len) * want)
    );

    BWAPI::Position clamped = clampToMapPx(candidate, 8);

    if (mainArea)
    {
        const auto* tgtArea = BWEM::Map::Instance().GetArea(BWAPI::TilePosition(clamped));
        if (tgtArea == mainArea)
        {
            for (int i = 0; i < 3; ++i)
            {
                const auto* a = BWEM::Map::Instance().GetArea(BWAPI::TilePosition(clamped));
                if (a != mainArea)
                {
                    break;
                }

                clamped = BWAPI::Position(
                    clamped.x + int((myCenter.x - clamped.x) * 0.3),
                    clamped.y + int((myCenter.y - clamped.y) * 0.3)
                );

                clamped = clampToMapPx(clamped, 8);
            }
        }
    }

    BWAPI::Position reachable = findReachableNearby(clamped);

    // Cache result to prevent constant goal changes.
    cachedPerch = reachable;
    cachedPerchFrame = now;

    return cachedPerch;
}

BWAPI::Position ScoutingZealot::findReachableNearby(const BWAPI::Position& desired) const
{
    if (!zealot || !zealot->exists())
    {
        return desired;
    }

    const BWAPI::Position from = zealot->getPosition();
    if (!from.isValid() || !desired.isValid())
    {
        return desired;
    }

    auto isOk = [&](const BWAPI::Position& p)
        {
            if (!p.isValid())
            {
                return false;
            }

            if (!isWalkablePx(p))
            {
                return false;
            }

            if (!hasGroundPath(from, p))
            {
                return false;
            }

            return true;
        };

    if (isOk(desired))
    {
        return desired;
    }

    // Search ring: try a few radii around the desired point
    static const int radii[] = { 32, 64, 96, 128, 160, 192 };
    static const int dirs[][2] =
    {
        {  1,  0 }, { -1,  0 }, {  0,  1 }, {  0, -1 },
        {  1,  1 }, {  1, -1 }, { -1,  1 }, { -1, -1 }
    };

    for (int r : radii)
    {
        for (const auto& d : dirs)
        {
            BWAPI::Position p(desired.x + d[0] * r, desired.y + d[1] * r);
            p = clampToMapPx(p, 8);

            if (isOk(p))
            {
                return p;
            }
        }
    }

    // Last resort: head toward natural center if reachable
    if (enemyNaturalPos.isValid() && isOk(enemyNaturalPos))
    {
        return enemyNaturalPos;
    }

    // Give up, return desired (caller may still handle stuck)
    return desired;
}

double ScoutingZealot::groundPathLengthPx(const BWAPI::Position& from, const BWAPI::Position& to)
{
    auto& m = BWEM::Map::Instance();
    const auto& path = m.GetPath(from, to, nullptr);
    if (path.empty()) {
        return from.getDistance(to);
    }
    double sum = 0.0;
    BWAPI::Position prev = from;
    for (const BWEM::ChokePoint* cp : path) {
        if (!cp) continue;
        BWAPI::Position c(cp->Center());
        sum += prev.getDistance(c);
        prev = c;
    }
    sum += prev.getDistance(to);
    return sum;
}


bool ScoutingZealot::threatenedNow() const {
    if (!zealot || !zealot->exists()) return false;
    if (zealot->isUnderAttack()) return true;

    for (auto& e : Broodwar->enemy()->getUnits()) {
        if (!e || !e->exists()) continue;
        if (e->getType().isWorker()) continue;         // <-- ignore workers
        if (!e->getType().canAttack()) continue;       // skip non-combat
        if (e->getDistance(zealot) < kThreatRadiusPx)  // inside danger bubble
            return true;
    }
    return false;
}

void ScoutingZealot::issueMove(const BWAPI::Position& p, bool force, int reissueDist)
{
    if (!zealot || !zealot->exists() || !p.isValid())
    {
        return;
    }

    const int now = BWAPI::Broodwar->getFrameCount();

    // If the goal changed meaningfully, reset stuck tracking.
    if (lastIssuedGoal.isValid())
    {
        const int goalDelta = p.getApproxDistance(lastIssuedGoal);
        if (goalDelta > kGoalChangeResetDist)
        {
            stuckFrames = 0;
        }
    }
    lastIssuedGoal = p;

    // Stuck detection: only when we intend to be moving toward the same goal.
    if (lastPos.isValid())
    {
        const int moved = zealot->getPosition().getApproxDistance(lastPos);

        if (moved < 4 && zealot->isMoving())
        {
            stuckFrames += 1;
        }
        else
        {
            stuckFrames = 0;
        }
    }
    lastPos = zealot->getPosition();

    if (stuckFrames > 24)
    {
        // Try a few nudges around current position, biased toward goal.
        BWAPI::Position here = zealot->getPosition();
        BWAPI::Position best = BWAPI::Positions::Invalid;

        const int dx = p.x - here.x;
        const int dy = p.y - here.y;

        auto tryNudge = [&](int ox, int oy)
            {
                if (best.isValid())
                {
                    return;
                }

                BWAPI::Position np(here.x + ox, here.y + oy);
                np = clampToMapPx(np, 8);

                if (!isWalkablePx(np))
                {
                    return;
                }

                if (!hasGroundPath(here, np))
                {
                    return;
                }

                best = np;
            };

        // Forward-ish, then side steps, then back-ish
        const int step = 64;
        const int sx = (dx >= 0) ? 1 : -1;
        const int sy = (dy >= 0) ? 1 : -1;

        tryNudge(step * sx, 0);
        tryNudge(0, step * sy);
        tryNudge(step * sx, step * sy);

        tryNudge(0, step * -sy);
        tryNudge(step * -sx, 0);
        tryNudge(step * -sx, step * sy);
        tryNudge(step * sx, step * -sy);

        if (best.isValid())
        {
            zealot->move(best);
            lastMoveIssueFrame = now;
            stuckFrames = 0;
            return;
        }

        // If no good nudge found, just reset and let normal move happen.
        stuckFrames = 0;
    }

    if (!force && now - lastMoveIssueFrame < kMoveCooldownFrames)
    {
        return;
    }

    if (force || zealot->getDistance(p) > reissueDist || !zealot->isMoving())
    {
        zealot->move(p);
        lastMoveIssueFrame = now;
    }

    Broodwar->drawLineMap(zealot->getPosition(), p, Colors::Orange);
}

BWAPI::Position ScoutingZealot::clampToMapPx(const BWAPI::Position& p, int marginPx) {
    const int x = (std::max)(marginPx, (std::min)(p.x, mapWpx() - 1 - marginPx));
    const int y = (std::max)(marginPx, (std::min)(p.y, mapHpx() - 1 - marginPx));
    return BWAPI::Position(x, y);
}


bool ScoutingZealot::isNear(const BWAPI::Position& a, const BWAPI::Position& b, int distPx) const
{
    return a.getApproxDistance(b) <= distPx;
}

void ScoutingZealot::rebuildProxyPoints()
{
    proxyPoints.clear();
    proxyNextIdx = 0;
    proxyCurTarget = BWAPI::Positions::Invalid;

    if (!zealot || !zealot->exists())
    {
        return;
    }


    

    const BWAPI::TilePosition myMainTp = BWAPI::Broodwar->self()->getStartLocation();
    const BWEM::Area* myMainArea = BWEM::Map::Instance().GetArea(myMainTp);
    if (!myMainArea)
    {
        return;
    }

    const BWAPI::Position myMainCenter = BWAPI::Position(myMainTp) + BWAPI::Position(16, 16);

    auto isInArea = [&](const BWEM::Area* area, const BWAPI::TilePosition& t) -> bool
    {
        if (!t.isValid() || !area)
        {
            return false;
        }

        const BWEM::Area* a = BWEM::Map::Instance().GetArea(t);
        return a == area;
    };

    auto addPoint = [&](BWAPI::Position p)
        {
            if (!p.isValid())
            {
                return;
            }

            p = clampToMapPx(p, 16);

            if (!isWalkablePx(p))
            {
                return;
            }

            if (!hasGroundPath(myMainCenter, p))
            {
                return;
            }

            for (const auto& existing : proxyPoints)
            {
                if (isNear(existing, p, 5 * 32))
                {
                    return;
                }
            }

            proxyPoints.push_back(p);
        };

    // 1) Main-area chokes
    for (const BWEM::ChokePoint* cp : myMainArea->ChokePoints())
    {
        if (!cp)
        {
            continue;
        }
        addPoint(BWAPI::Position(cp->Center()));
    }

    // 2) Neighbor-area chokes (typical proxy routes)
    for (const BWEM::Area* n : myMainArea->AccessibleNeighbours())
    {
        if (!n)
        {
            continue;
        }

        for (const BWEM::ChokePoint* cp : n->ChokePoints())
        {
            if (!cp)
            {
                continue;
            }
            addPoint(BWAPI::Position(cp->Center()));
        }
    }

    // 3) Add our natural base center (closest non-starting base not in main area)
    

    double bestDist = 1e30;
    BWAPI::Position bestNat = BWAPI::Positions::Invalid;

    for (const BWEM::Area& area : BWEM::Map::Instance().Areas())
    {
        for (const BWEM::Base& base : area.Bases())
        {
            if (base.Starting())
            {
                continue;
            }

            const BWEM::Area* bArea = BWEM::Map::Instance().GetArea(base.Location());
            if (bArea == myMainArea)
            {
                continue;
            }

            BWAPI::Position basePos = base.Center();
            if (!basePos.isValid())
            {
                basePos = BWAPI::Position(base.Location()) + BWAPI::Position(16, 16);
            }

            const auto& path = BWEM::Map::Instance().GetPath(myMainCenter, basePos, nullptr);
            if (path.empty())
            {
                continue;
            }

            const double d = groundPathLengthPx(myMainCenter, basePos);
            if (d < bestDist)
            {
                bestDist = d;
                bestNat = basePos;
            }
        }
    }

    if (bestNat.isValid())
    {
        addPoint(bestNat);
    }

    // If we somehow got nothing, at least patrol home
    if (proxyPoints.empty())
    {
        addPoint(g_myMainPos);
    }

    

    // 4) Perimeter ring: boundary tiles of our main area (proxy pylons tucked around edges)
    {
        const int stepTiles = 4; // lower = more points;
        const BWAPI::TilePosition topLeft = myMainArea->TopLeft();
        const BWAPI::TilePosition bottomRight = myMainArea->BottomRight();

        for (int y = topLeft.y; y <= bottomRight.y; y += stepTiles)
        {
            for (int x = topLeft.x; x <= bottomRight.x; x += stepTiles)
            {
                BWAPI::TilePosition t(x, y);
                if (!isInArea(myMainArea, t))
                {
                    continue;
                }

                // If any 4-neighbor is NOT in our main area, this is a boundary-ish tile
                const BWAPI::TilePosition n1(x + 1, y);
                const BWAPI::TilePosition n2(x - 1, y);
                const BWAPI::TilePosition n3(x, y + 1);
                const BWAPI::TilePosition n4(x, y - 1);

                if (isInArea(myMainArea, n1) && isInArea(myMainArea, n2) && isInArea(myMainArea, n3) && isInArea(myMainArea, n4))
                {
                    continue;
                }

                addPoint(BWAPI::Position(t) + BWAPI::Position(16, 16));
            }
        }
    }

    // 5) Air-close but ground-far points (cliff/maze-y proxy spots near us)
    {
        const double kMinRatio = 1.8;
        const int kMaxAirDist = 28 * 32;
        const int sampleStepTiles = 6;
        const double kMaxGroundDist = 90.0 * 32.0;

        const int margin = 12;
        const BWAPI::TilePosition topLeft = myMainArea->TopLeft();
        const BWAPI::TilePosition bottomRight = myMainArea->BottomRight();

        for (int y = topLeft.y - margin; y <= bottomRight.y + margin; y += sampleStepTiles)
        {
            for (int x = topLeft.x - margin; x <= bottomRight.x + margin; x += sampleStepTiles)
            {
                BWAPI::TilePosition t(x, y);
                if (!t.isValid())
                {
                    continue;
                }

                BWAPI::Position p = BWAPI::Position(t) + BWAPI::Position(16, 16);
                p = clampToMapPx(p, 16);

                const int air = myMainCenter.getApproxDistance(p);
                if (air > kMaxAirDist)
                {
                    continue;
                }

                const auto& path = BWEM::Map::Instance().GetPath(myMainCenter, p, nullptr);
                if (path.empty())
                {
                    continue;
                }

                const double ground = groundPathLengthPx(myMainCenter, p);
                if (ground <= 1.0)
                {
                    continue;
                }

                if (ground > kMaxGroundDist)
                {
                    continue;
                }

                if (ground >= double(air) * kMinRatio)
                {
                    addPoint(p);
                }
            }
        }
    }

    // Order: nearest-neighbor greedy starting from zealot
    std::vector<BWAPI::Position> ordered;
    ordered.reserve(proxyPoints.size());

    BWAPI::Position cur = zealot->getPosition();

    while (!proxyPoints.empty())
    {
        int bestIdx = 0;
        int bestD = INT_MAX;

        for (int i = 0; i < (int)proxyPoints.size(); ++i)
        {
            const int d = cur.getApproxDistance(proxyPoints[i]);
            if (d < bestD)
            {
                bestD = d;
                bestIdx = i;
            }
        }

        ordered.push_back(proxyPoints[bestIdx]);
        cur = proxyPoints[bestIdx];
        proxyPoints.erase(proxyPoints.begin() + bestIdx);
    }

    

    proxyPoints = std::move(ordered);
}


BWAPI::Unit ScoutingZealot::findPrimaryThreat(int radiusPx) const
{
    if (!zealot || !zealot->exists())
    {
        return nullptr;
    }

    BWAPI::Unit best = nullptr;
    int bestD = INT_MAX;

    for (auto& e : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (!e || !e->exists())
        {
            continue;
        }

        const auto t = e->getType();

        if (t.isWorker())
        {
            continue;
        }

        if (!t.canAttack())
        {
            continue;
        }

        const int d = zealot->getDistance(e);
        if (d > radiusPx)
        {
            continue;
        }

        if (d < bestD)
        {
            bestD = d;
            best = e;
        }
    }

    return best;
}

void ScoutingZealot::retreatHomeMicro(BWAPI::Unit threat)
{
    if (!zealot || !zealot->exists())
    {
        return;
    }

    // If we're mid-windup / firing, don't change anything.
    if (zealot->isStartingAttack() || zealot->isAttackFrame())
    {
        return;
    }

    const int now = BWAPI::Broodwar->getFrameCount();

    BWAPI::Unit target = nullptr;

    // Stick to last target for a short window to avoid retarget stutter.
    if (lastAttackTarget && lastAttackTarget->exists())
    {
        if (now - lastTargetSelectFrame < kTargetStickFrames)
        {
            if (isGoodKiteTarget(lastAttackTarget, kThreatRadiusPx))
            {
                target = lastAttackTarget;
            }
        }
    }

    // If we don't have a sticky target, pick a new one.
    if (!target)
    {
        BWAPI::Unit candidate = nullptr;

        if (isGoodKiteTarget(threat, kThreatRadiusPx))
        {
            candidate = threat;
        }
        else
        {
            candidate = findPrimaryThreat(kThreatRadiusPx);
        }

        target = candidate;

        if (target)
        {
            lastAttackTarget = target;
            lastTargetSelectFrame = now;
        }
    }

    // If in range, try to shoot (tryFireAndCommit also "locks" the target).
    if (target && target->exists())
    {
        if (tryFireAndCommit(target))
        {
            return;
        }
    }

    // No shot right now: kite step.
    issueMove(homeRetreatPoint(), true, 64);
}

BWAPI::Unit ScoutingZealot::findInWeaponRangeTarget() const
{
    if (!zealot || !zealot->exists())
    {
        return nullptr;
    }

    BWAPI::Unit best = nullptr;
    int bestD = INT_MAX;

    for (auto& e : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (!e || !e->exists())
        {
            continue;
        }

        if (e->getType().isWorker())
        {
            continue;
        }

        if (!zealot->canAttackUnit(e))
        {
            continue;
        }

        if (!zealot->isInWeaponRange(e))
        {
            continue;
        }

        const int d = zealot->getDistance(e);
        if (d < bestD)
        {
            bestD = d;
            best = e;
        }
    }

    return best;
}

int ScoutingZealot::attackCommitFrames() const
{
    if (!zealot || !zealot->exists())
    {
        return 0;
    }

    const int range = zealot->getType().groundWeapon().maxRange();

    // Dragoons need a bigger window so we don't cancel their windup.
    if (range > 32)
    {
        return 8;
    }

    // Zealot (melee) is fine with a tiny commit.
    return 2;
}

bool ScoutingZealot::tryFireAndCommit(BWAPI::Unit target)
{
    if (!zealot || !zealot->exists() || !target || !target->exists())
    {
        return false;
    }

    if (!zealot->canAttackUnit(target))
    {
        return false;
    }

    if (!zealot->isInWeaponRange(target))
    {
        return false;
    }

    // Lock target choice even if weapon is cooling down.
    lastAttackTarget = target;

    const int now = BWAPI::Broodwar->getFrameCount();
    const int commit = attackCommitFrames();

    if (now - lastAttackCmdFrame < commit && lastAttackTarget == target)
    {
        lastTargetSelectFrame = now;
        return true;
    }

    if (zealot->getGroundWeaponCooldown() == 0)
    {
        zealot->attack(target);
        lastAttackCmdFrame = now;
        lastTargetSelectFrame = now;
        return true;
    }

    return false;
}

bool ScoutingZealot::isGoodKiteTarget(BWAPI::Unit u, int radiusPx) const
{
    if (!u || !u->exists())
    {
        return false;
    }

    if (u->getPlayer() != BWAPI::Broodwar->enemy())
    {
        return false;
    }

    const auto t = u->getType();

    if (t.isWorker())
    {
        return false;
    }

    if (!t.canAttack())
    {
        return false;
    }

    if (!zealot->canAttackUnit(u))
    {
        return false;
    }

    return zealot->getDistance(u) <= radiusPx;
}