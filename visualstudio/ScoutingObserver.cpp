#include "ScoutingObserver.h"
#include "ProtoBotCommander.h"
#include "ScoutingManager.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <optional>

using namespace BWAPI;
using namespace BWEM;

namespace
{
    static BWAPI::Position rotatePoint(const BWAPI::Position& v, double ang)
    {
        const double c = std::cos(ang);
        const double s = std::sin(ang);

        return BWAPI::Position(
            int(v.x * c - v.y * s),
            int(v.x * s + v.y * c)
        );
    }
}

namespace {
    static inline int mapWpx() { return Broodwar->mapWidth() * 32; }
    static inline int mapHpx() { return Broodwar->mapHeight() * 32; }
}

void ScoutingObserver::onStart() {
    lastMoveFrame = 0;
    lastThreatFrame = -10000;
    posts.clear();
    state = State::Idle;
}

void ScoutingObserver::assign(BWAPI::Unit u) {
    if (!u || !u->exists()) return;

    observer = u;

    if (enemyMainPos.isValid() && posts.empty())
    {
        computePosts();
    }

    // Slot 3: home is wherever we are right now
    if (slotIndex == 3 && !slot3HomeSet)
    {
        slot3Home = observer->getPosition();
        slot3Home = clampToMapPx(slot3Home, 12);
        slot3HomeSet = true;
        slot3NextRebuildFrame = 0;
        slot3NextIdx = 0;
        slot3CurTarget = BWAPI::Positions::Invalid;
    }

    if (slotIndex == 3)
    {
        state = State::Hold;
    }
    else
    {
        state = (slotIndex >= 0 && !posts.empty()) ? State::MoveToPost : State::Idle;
    }
}

void ScoutingObserver::setEnemyMain(const TilePosition& tp) {
    enemyMainTile = tp;
    enemyMainPos = Position(tp);
    computePosts();
    if (state == State::Idle && slotIndex >= 0) state = State::MoveToPost;
}

void ScoutingObserver::onUnitDestroy(BWAPI::Unit u) {
    if (observer && u == observer) {
        observer = nullptr;
        state = State::Done;
    }
}

void ScoutingObserver::onFrame() {
    if (!observer || !observer->exists() || state == State::Done) return;

    if (!enemyMainPos.isValid())
    {
        state = State::Idle;
        return;
    }

    if (posts.empty()) computePosts();
    if (slotIndex < 0) return;

    // Detection avoidance (your existing logic)
    BWAPI::Position safePt;
    if (detectorThreat(safePt))
    {
        lastThreatFrame = Broodwar->getFrameCount();
        issueMove(safePt, /*force*/true, /*reissue*/32);
        state = State::AvoidDetection;
        return;
    }

    const int now = Broodwar->getFrameCount();

    // --------------------------
    // Slot 3: roaming expansion checker
    // --------------------------
    if (slotIndex == 3 && slot3HomeSet)
    {
        if (slot3Checkpoints.empty())
        {
            rebuildSlot3Checkpoints();
        }
        // If we have checkpoints, go visit them
        // If we have checkpoints, go visit them
        if (!slot3Checkpoints.empty())
        {
            // If we’re returning home after a sweep, ignore checkpoints until we get home.
            if (slot3ReturningHome)
            {
                if (observer->getDistance(slot3Home) <= kSlot3ArriveDist)
                {
                    slot3ReturningHome = false;

                    if (slot3NeedsRebuild)
                    {
                        rebuildSlot3Checkpoints();
                        slot3NeedsRebuild = false;
                    }
                }
                else if (now >= slot3NextMoveFrame)
                {
                    issueMove(slot3Home, false, kHoldReissueDist);
                    slot3NextMoveFrame = now + kSlot3MinBetweenMoves;
                }

                return;
            }

            // Pick/advance target
            if (!slot3CurTarget.isValid() || observer->getDistance(slot3CurTarget) <= kSlot3ArriveDist)
            {
                // Finished the list: rebuild ONCE and keep patrolling from current position.
                // Only go home if rebuild produces no nodes.
                if (slot3NextIdx >= (int)slot3Checkpoints.size())
                {
                    rebuildSlot3Checkpoints();

                    slot3NextIdx = 0;
                    slot3CurTarget = BWAPI::Positions::Invalid;

                    if (slot3Checkpoints.empty())
                    {
                        slot3ReturningHome = true; // only now do we go home
                    }

                    return;
                }
                slot3CurTarget = slot3Checkpoints[slot3NextIdx++];
            }

            // Move toward current target (throttled)
            if (slot3CurTarget.isValid() && now >= slot3NextMoveFrame)
            {
                const BWAPI::Position step = pickDetourToward(slot3CurTarget);
                issueMove(step, false, 64);
                slot3NextMoveFrame = now + kSlot3MinBetweenMoves;
            }

            return;
        }

        // No checkpoints: just hold home
        if (now - lastMoveFrame > 48)
        {
            issueMove(slot3Home, /*force*/true, kHoldReissueDist);
        }

        return;
    }

    // --------------------------
    // Normal slots 0-2 behavior (your existing switch)
    // --------------------------
    if (slotIndex >= (int)posts.size()) return;

    switch (state) {
    case State::Idle:
        state = State::MoveToPost;
        break;

    case State::MoveToPost: 
    {
        const BWAPI::Position tgt = postTarget();
        const BWAPI::Position step = pickDetourToward(tgt);

        issueMove(step, false, 64);

        if (observer->getDistance(tgt) <= 80)
        {
            state = State::Hold;
        }
        break;
    }

    case State::Hold: {
        const Position tgt = postTarget();
        if (!observer->isMoving() && !observer->isAttacking()) 
        {
            // Hold is just "don't drift"; observers can't hold-position command,
            // but reissue a move to target occasionally to keep tight.
            if (Broodwar->getFrameCount() - lastMoveFrame > 48)
                issueMove(tgt, /*force*/true, kHoldReissueDist);
        }
        break;
    }

    case State::AvoidDetection: 
    {
        // Once calm enough, return to post
        if (Broodwar->getFrameCount() - lastThreatFrame >= kSafeFramesToReturn) 
        {
            state = State::MoveToPost;
        }
        break;
    }
    default: break;
    }

    // tiny debug
    Broodwar->drawTextMap(observer->getPosition(), "\x11OBS slot %d", slotIndex);
    for (int i = 0; i < (int)posts.size(); ++i) {
        auto c = (i == 0) ? Colors::Yellow : (i == 1 ? Colors::Cyan : (i == 2 ? Colors::Purple : Colors::Orange));
        Broodwar->drawCircleMap(posts[i], 8, c, true);
        Broodwar->drawTextMap(posts[i], "#%d", i);
    }
}

/* ------------ helpers ------------- */

void ScoutingObserver::computePosts()
{
    posts.clear();
    if (!enemyMainPos.isValid() || !enemyMainTile.has_value()) return;

    const TilePosition enemyMain = *enemyMainTile;
    const Position enemyMainCenter = Position(enemyMain) + Position(16, 16);
    const auto* mainArea = BWEM::Map::Instance().GetArea(enemyMain);

    // 0) Over enemy main, slightly above center
    Position mainCenter = enemyMainCenter + Position(0, -32);
    posts.push_back(clampToMapPx(mainCenter, 16));

    struct Candidate { const BWEM::Base* base; double gdist; Position pos; };
    std::vector<Candidate> cands;

    // Collect non-starting, reachable bases NOT in the main's area
    for (const BWEM::Area& area : BWEM::Map::Instance().Areas())
    {
        for (const BWEM::Base& base : area.Bases()) {
            if (base.Starting()) continue;

            const auto* bArea = BWEM::Map::Instance().GetArea(base.Location());
            if (bArea == mainArea) continue;

            // Prefer base center if available; else location + (16,16)
            Position basePos = base.Center(); // BWEM::Base::Center is BWAPI::Position
            if (!basePos.isValid()) basePos = Position(base.Location()) + Position(16, 16);

            const auto& path = BWEM::Map::Instance().GetPath(enemyMainCenter, basePos, nullptr);
            if (path.empty()) continue; // unreachable by ground

            double g = groundPathLengthPx(enemyMainCenter, basePos);
            cands.push_back({ &base, g, basePos });
        }
    }

    std::sort(cands.begin(), cands.end(),
        [](const Candidate& a, const Candidate& b) { return a.gdist < b.gdist; });

    // 1) natural (closest), 2) second closest expo, 3) third closest expo
    if (!cands.empty())      posts.push_back(cands[0].pos);
    if (cands.size() >= 2)   posts.push_back(cands[1].pos);
    if (cands.size() >= 3)   posts.push_back(cands[2].pos);

    // Nudge every post slightly "above" and clamp
    for (auto& p : posts) p = clampToMapPx(p + Position(0, -24), 12);

    // Ensure exactly 4 posts (pad with main if needed)
    while (posts.size() < 4) posts.push_back(mainCenter);
    if (posts.size() > 4) posts.resize(4);
}

BWAPI::Position ScoutingObserver::postTarget() const 
{
    if (slotIndex >= 0 && slotIndex < (int)posts.size())
        return posts[slotIndex];
    return enemyMainPos;
}

void ScoutingObserver::issueMove(const BWAPI::Position& p, bool force, int reissueDist) 
{
    if (!observer || !observer->exists() || !p.isValid()) return;
    const int now = Broodwar->getFrameCount();
    if (!force && now - lastMoveFrame < kMoveCooldown) return;
    if (force || observer->getDistance(p) > reissueDist || !observer->isMoving()) 
    {
        observer->move(p);
        lastMoveFrame = now;
    }
    Broodwar->drawLineMap(observer->getPosition(), p, Colors::White);
}

bool ScoutingObserver::detectorThreat(BWAPI::Position& avoidTo) const 
{
    if (!observer || !observer->exists()) return false;
    const Position me = observer->getPosition();

    int bestOver = INT_MIN;
    Position bestPos = Positions::Invalid;

    for (auto e : Broodwar->enemy()->getUnits()) 
    {
        if (!e || !e->exists()) continue;
        const auto t = e->getType();
        if (!t.isDetector()) continue;

        const int detR = detectionRadiusFor(t);   // detector radius in px
        const int d = e->getDistance(me);
        const int over = detR - d;                // >0 means we are inside detection

        // If we're inside (or nearly at the edge), compute a point just outside
        if (over >= -kDetectReactBufferPx) 
        {
            // step along vector from detector->me to just outside radius+margin
            const Position c = e->getPosition();
            int dx = me.x - c.x, dy = me.y - c.y;
            double len = std::max(1.0, std::sqrt(double(dx * dx + dy * dy)));
            const int want = detR + kEdgeMarginPx + kDetectReactBufferPx;
            Position out(c.x + int(dx / len * want), c.y + int(dy / len * want));
            out = clampToMapPx(out, 12);

            // pick the “most violating” detector (largest over) to react to
            if (over > bestOver) 
            {
                bestOver = over;
                bestPos = out;
            }
        }
    }

    if (bestPos.isValid()) { avoidTo = bestPos; return true; }
    return false;
}

int ScoutingObserver::detectionRadiusFor(const BWAPI::UnitType& t) 
{
    if (t == BWAPI::UnitTypes::Terran_Missile_Turret) return 224;
    if (t == BWAPI::UnitTypes::Protoss_Photon_Cannon) return 224;
    if (t == BWAPI::UnitTypes::Zerg_Spore_Colony)     return 224;
    if (t == BWAPI::UnitTypes::Terran_Science_Vessel) return 224;
    if (t == BWAPI::UnitTypes::Protoss_Observer)      return 224; // enemy obs can reveal you to army
    int r = t.sightRange();
    if (r <= 0) r = 224;
    // a small bias outward
    return r;
}

BWAPI::Position ScoutingObserver::clampToMapPx(const BWAPI::Position& p, int margin) 
{
    const int x = std::max(margin, std::min(p.x, mapWpx() - 1 - margin));
    const int y = std::max(margin, std::min(p.y, mapHpx() - 1 - margin));
    return BWAPI::Position(x, y);
}

double ScoutingObserver::groundPathLengthPx(const BWAPI::Position& from, const BWAPI::Position& to) 
{
    auto& m = BWEM::Map::Instance();
    const auto& path = m.GetPath(from, to, nullptr);
    if (path.empty()) return from.getDistance(to);
    double sum = 0.0;
    BWAPI::Position prev = from;
    for (const BWEM::ChokePoint* cp : path) 
    {
        if (!cp) continue;
        BWAPI::Position c(cp->Center());
        sum += prev.getDistance(c);
        prev = c;
    }
    sum += prev.getDistance(to);
    return sum;
}

bool ScoutingObserver::isUnsafe(const BWAPI::Position& p) const
{
    if (!commanderRef) return false;

    const auto threat = commanderRef->queryThreatAt(p);

    if (threat.detectorThreat > 0)
    {
        return true;
    }

    if (threat.airThreat > kAirThreatThreshold)
    {
        return true;
    }

    return false;
}

BWAPI::Position ScoutingObserver::pickDetourToward(const BWAPI::Position& target) const
{
    if (!observer || !observer->exists())
    {
        return BWAPI::Positions::Invalid;
    }

    const BWAPI::Position me = observer->getPosition();

    // If target itself is safe, just go directly.
    if (!isUnsafe(target))
    {
        return target;
    }

    // Try a few detours around our current heading.
    const BWAPI::Position toTgt = BWAPI::Position(target.x - me.x, target.y - me.y);

    // If we're basically on top of target, no detour helps.
    if (std::abs(toTgt.x) + std::abs(toTgt.y) < 8)
    {
        return target;
    }

    // Candidate radii (pixels)
    const int radii[] = { 96, 160, 224, 288 };

    // Candidate angles around heading (radians)
    const double angles[] =
    {
        0.0,
        0.35, -0.35,
        0.70, -0.70,
        1.05, -1.05,
        1.40, -1.40
    };

    int bestScore = INT_MIN;
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;

    // Normalize direction roughly (avoid expensive normalize)
    BWAPI::Position dir = toTgt;
    const int len = std::max(1, int(std::sqrt(double(dir.x * dir.x + dir.y * dir.y))));
    dir = BWAPI::Position(int(double(dir.x) / len * 64.0), int(double(dir.y) / len * 64.0)); // scaled

    for (int r : radii)
    {
        for (double a : angles)
        {
            BWAPI::Position cand = me + rotatePoint(BWAPI::Position(int(dir.x * (double(r) / 64.0)), int(dir.y * (double(r) / 64.0))), a);
            cand = clampToMapPx(cand, 12);

            // Must be walkable-ish (observers fly, but avoid silly invalid positions)
            if (!cand.isValid())
            {
                continue;
            }

            // Skip unsafe candidates.
            if (isUnsafe(cand))
            {
                continue;
            }

            // Score: prefer getting closer to target, with a mild bias for larger steps.
            const int distNow = me.getApproxDistance(target);
            const int distCand = cand.getApproxDistance(target);
            const int progress = distNow - distCand;

            const int score = progress + (r / 8);

            if (score > bestScore)
            {
                bestScore = score;
                bestPos = cand;
            }
        }
    }

    // If nothing safe found, just return the safest “least bad” direction (fallback).
    if (!bestPos.isValid())
    {
        return target;
    }

    return bestPos;
}


bool ScoutingObserver::haveVisionAt(const BWAPI::Position& p, int radiusPx) const
{
    if (!p.isValid()) return true;

    // If tile is visible this frame, we definitely have vision.
    if (BWAPI::Broodwar->isVisible(BWAPI::TilePosition(p)))
    {
        return true;
    }

    // Otherwise, check if any of our units are physically near it.
    for (auto u : BWAPI::Broodwar->self()->getUnits())
    {
        if (!u || !u->exists()) continue;
        if (u->getDistance(p) <= radiusPx) return true;
    }

    return false;
}

namespace
{
    static bool hasOurNexusNear(const BWAPI::Position& p, int radiusPx)
    {
        for (auto u : BWAPI::Broodwar->self()->getUnits())
        {
            if (!u || !u->exists()) continue;
            if (u->getType() != BWAPI::UnitTypes::Protoss_Nexus) continue;
            if (u->getDistance(p) <= radiusPx) return true;
        }
        return false;
    }

    static bool isOtherObserverCovering(const BWAPI::Position& p, const BWAPI::Unit meObs, int radiusPx)
    {
        for (auto u : BWAPI::Broodwar->self()->getUnits())
        {
            if (!u || !u->exists()) continue;
            if (u == meObs) continue;
            if (u->getType() != BWAPI::UnitTypes::Protoss_Observer) continue;

            if (u->getDistance(p) <= radiusPx)
            {
                return true;
            }
        }
        return false;
    }
}

void ScoutingObserver::rebuildSlot3Checkpoints()
{
    slot3Checkpoints.clear();

    if (!observer || !observer->exists())
    {
        slot3NextIdx = 0;
        slot3CurTarget = BWAPI::Positions::Invalid;
        return;
    }

    // Tuning knobs
    const int kNexusExcludeRadius = 10 * 32;      // 10 tiles
    const int kObserverCoverRadius = 14 * 32;     // 14 tiles
    const int kMaxPatrolPoints = 16;               // cap to keep it stable
    const int kMinPatrolPoints = 2;               // ensure at least 2 so its not just freaking out

    std::vector<BWAPI::Position> candidates;
    candidates.reserve(32);

    for (const BWEM::Area& area : BWEM::Map::Instance().Areas())
    {
        for (const BWEM::Base& base : area.Bases())
        {
            BWAPI::Position pos = base.Center();
            if (!pos.isValid())
            {
                pos = BWAPI::Position(base.Location()) + BWAPI::Position(16, 16);
            }

            pos = clampToMapPx(pos, 12);

            if (hasOurNexusNear(pos, kNexusExcludeRadius))
            {
                continue;
            }

            if (isOtherObserverCovering(pos, observer, kObserverCoverRadius))
            {
                continue;
            }

            // This is your “detectors matter while moving, not in cost” rule:
            // We only use isUnsafe as a hard filter (optional).
            // If you want it NOT to be a hard filter, remove this and rely on pickDetourToward().
            if (isUnsafe(pos))
            {
                continue;
            }

            candidates.push_back(pos);
        }
    }

    if (candidates.empty())
    {
        slot3NextIdx = 0;
        slot3CurTarget = BWAPI::Positions::Invalid;
        return;
    }

    // Nearest-neighbor greedy ordering (straight-line distance).
    BWAPI::Position cur = observer->getPosition();

    while (!candidates.empty() && (int)slot3Checkpoints.size() < kMaxPatrolPoints)
    {
        int bestIdx = -1;
        int bestD = INT_MAX;

        for (int i = 0; i < (int)candidates.size(); ++i)
        {
            const int d = cur.getApproxDistance(candidates[i]);
            if (d < bestD)
            {
                bestD = d;
                bestIdx = i;
            }
        }

        if (bestIdx < 0)
        {
            break;
        }

        slot3Checkpoints.push_back(candidates[bestIdx]);
        cur = candidates[bestIdx];
        candidates.erase(candidates.begin() + bestIdx);
    }

    // If we ended up with only 1 but we had more candidates, force-add one more nearest
    // so it doesn’t look “stuck”.
    if ((int)slot3Checkpoints.size() < kMinPatrolPoints && !candidates.empty())
    {
        int bestIdx = 0;
        int bestD = INT_MAX;

        for (int i = 0; i < (int)candidates.size(); ++i)
        {
            const int d = cur.getApproxDistance(candidates[i]);
            if (d < bestD)
            {
                bestD = d;
                bestIdx = i;
            }
        }

        slot3Checkpoints.push_back(candidates[bestIdx]);
    }

    slot3NextIdx = 0;
    slot3CurTarget = BWAPI::Positions::Invalid;
}