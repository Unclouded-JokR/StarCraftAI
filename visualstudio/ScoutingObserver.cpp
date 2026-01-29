#include "ScoutingObserver.h"
#include "ProtoBotCommander.h"
#include "ScoutingManager.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <optional>

using namespace BWAPI;
using namespace BWEM;

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
    if (enemyMainPos.isValid() && posts.empty()) computePosts();
    state = (slotIndex >= 0 && !posts.empty()) ? State::MoveToPost : State::Idle;
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

    // If we don't yet know enemy main, wait.
    if (!enemyMainPos.isValid()) {
        state = State::Idle;
        return;
    }
    if (posts.empty()) computePosts();
    if (slotIndex < 0 || slotIndex >= (int)posts.size()) return;

    // Detection avoidance
    Position safePt;
    if (detectorThreat(safePt)) {
        lastThreatFrame = Broodwar->getFrameCount();
        issueMove(safePt, /*force*/true, /*reissue*/32);
        state = State::AvoidDetection;
        return;
    }

    switch (state) {
    case State::Idle:
        state = State::MoveToPost;
        break;

    case State::MoveToPost: {
        const Position tgt = postTarget();
        issueMove(tgt, /*force*/false, /*reissue*/64);
        if (observer->getDistance(tgt) <= 80) state = State::Hold;
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
        if (over >= -8) 
        {
            // step along vector from detector->me to just outside radius+margin
            const Position c = e->getPosition();
            int dx = me.x - c.x, dy = me.y - c.y;
            double len = std::max(1.0, std::sqrt(double(dx * dx + dy * dy)));
            const int want = detR + kEdgeMarginPx;
            Position out(c.x + int(dx / len * want), c.y + int(dy / len * want));
            out = clampToMapPx(out, 12);

            // pick the “most violating” detector (largest over) to react to
            if (over < bestOver) 
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
    if (t == BWAPI::UnitTypes::Zerg_Sunken_Colony)     return 224;
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
