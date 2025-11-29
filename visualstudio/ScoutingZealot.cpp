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
    state = enemyMainPos.isValid() ? State::MoveToNatural : State::WaitEnemyMain;
}

void ScoutingZealot::setEnemyMain(const TilePosition& tp) {
    enemyMainTile = tp;
    enemyMainPos = Position(tp);

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
        (state == State::Idle ? "Idle" :
            state == State::WaitEnemyMain ? "WaitEnemyMain" :
            state == State::MoveToNatural ? "MoveToNatural" :
            state == State::HoldEdge ? "HoldEdge" :
            state == State::Reposition ? "Reposition" : "Done"));

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
        state = enemyMainPos.isValid() ? State::MoveToNatural : State::WaitEnemyMain;
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
        if (!enemyMainPos.isValid()) { state = State::WaitEnemyMain; break; }
        if (!enemyNaturalPos.isValid()) computeEnemyNatural();

        if (enemyNaturalPos.isValid()) {
            const Position perch = pickEdgeOfVisionSpot();
            issueMove(perch);
            if (zealot->getDistance(perch) < 96) {
                state = State::HoldEdge;
            }
        }
        break;
    }

    case State::HoldEdge:
    {
        if (threatenedNow()) {
            const int now = Broodwar->getFrameCount();
            lastThreatFrame = now;

            // Run straight home (to our main)
            issueMove(homeRetreatPoint(), /*force*/true);
            state = State::Reposition;
            break;
        }

        if (!zealot->isMoving() && !zealot->isAttacking()) {
            zealot->holdPosition();
        }
        if (Broodwar->getFrameCount() - lastMoveIssueFrame > 48) {
            issueMove(pickEdgeOfVisionSpot(), /*force*/true, /*reissueDist*/16);
        }
        break;
    }

    case State::Reposition:
    {
        const int now = Broodwar->getFrameCount();

        // If still threatened, keep resetting calm timer and keep moving home
        if (threatenedNow()) {
            lastThreatFrame = now;
            issueMove(homeRetreatPoint(), /*force*/true, /*reissueDist*/64);
            break;
        }

        // Area calm long enough? Go back to the natural perch
        if (now - lastThreatFrame >= kCalmFramesToReturn) {
            issueMove(pickEdgeOfVisionSpot(), /*force*/true);
            state = State::HoldEdge;
            break;
        }

        // Calm but waiting out timer: hold position so we don't drift
        if (!zealot->isMoving() && !zealot->isAttacking()) {
            zealot->holdPosition();
        }
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

BWAPI::Position ScoutingZealot::pickEdgeOfVisionSpot() const {
    if (!enemyNaturalPos.isValid()) return enemyNaturalPos;

    const auto* natArea = BWEM::Map::Instance().GetArea(enemyNaturalTile);
    const auto* mainArea = enemyMainTile.has_value()
        ? BWEM::Map::Instance().GetArea(*enemyMainTile)
        : nullptr;

    // natural center: prefer mineral centroid if we can
    BWAPI::Position natCenter = enemyNaturalPos;
    if (const auto* a = natArea) {
        for (const auto& b : a->Bases()) {
            if (b.Location() == enemyNaturalTile) {
                long long sx = 0, sy = 0; int n = 0;
                for (auto* m : b.Minerals()) {
                    if (!m) continue;
                    BWAPI::Position p(m->Pos());
                    if (!p.isValid()) continue;
                    sx += p.x; sy += p.y; ++n;
                }
                if (n > 0) natCenter = BWAPI::Position(int(sx / n), int(sy / n));
                break;
            }
        }
    }

    // push 'want' pixels from the natural toward our main
    BWAPI::Position myCenter = g_myMainPos.isValid()
        ? g_myMainPos
        : BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

    int dirx = myCenter.x - natCenter.x;
    int diry = myCenter.y - natCenter.y;
    if (dirx == 0 && diry == 0) dirx = 1;

    const int zealotSight = BWAPI::UnitTypes::Protoss_Zealot.sightRange();
    const int want = std::max(0, zealotSight - kEdgeMarginPx);

    const double len = std::max(1.0, std::sqrt(double(dirx * dirx + diry * diry)));
    BWAPI::Position candidate(
        natCenter.x + int((dirx / len) * want),
        natCenter.y + int((diry / len) * want)
    );

    // clamp to map
    auto clamped = clampToMapPx(candidate, 8);

    // if we accidentally landed in the enemy main, nudge toward our main until we’re out
    if (mainArea) {
        const auto* tgtArea = BWEM::Map::Instance().GetArea(BWAPI::TilePosition(clamped));
        if (tgtArea == mainArea) {
            for (int i = 0; i < 3; ++i) {
                const auto* a = BWEM::Map::Instance().GetArea(BWAPI::TilePosition(clamped));
                if (a != mainArea) break;
                clamped = BWAPI::Position(
                    clamped.x + int((myCenter.x - clamped.x) * 0.3),
                    clamped.y + int((myCenter.y - clamped.y) * 0.3)
                );
                clamped = clampToMapPx(clamped, 8);
            }
        }
    }

    return clamped;
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

void ScoutingZealot::issueMove(const BWAPI::Position& p, bool force, int reissueDist) {
    if (!zealot || !zealot->exists() || !p.isValid()) return;
    const int now = Broodwar->getFrameCount();
    if (!force && now - lastMoveIssueFrame < kMoveCooldownFrames) return;

    if (force || zealot->getDistance(p) > reissueDist || !zealot->isMoving()) {
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
