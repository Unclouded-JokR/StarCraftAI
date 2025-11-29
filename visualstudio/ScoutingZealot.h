#pragma once
#include <BWAPI.h>
#include <bwem.h>
#include <optional>
#include <vector>

class ProtoBotCommander;
class ScoutingManager;

class ScoutingZealot {
public:
    explicit ScoutingZealot(ProtoBotCommander* commander, ScoutingManager* manager)
        : commanderRef(commander), manager(manager) {}

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);

    void setEnemyMain(const BWAPI::TilePosition& tp);
    void assign(BWAPI::Unit unit);
    bool hasScout() const { return zealot && zealot->exists(); }

private:
    // --- state ---
    enum class State { Idle, WaitEnemyMain, MoveToNatural, HoldEdge, Reposition, Done };

    ProtoBotCommander* commanderRef = nullptr;
    ScoutingManager* manager = nullptr;
    BWAPI::Unit zealot = nullptr;

    std::optional<BWAPI::TilePosition> enemyMainTile;
    BWAPI::Position enemyMainPos = BWAPI::Positions::Invalid;
    BWAPI::TilePosition enemyNaturalTile = BWAPI::TilePositions::Invalid;
    BWAPI::Position enemyNaturalPos = BWAPI::Positions::Invalid;

    State state = State::Idle;
    int lastMoveIssueFrame = 0;

    // --- tuning ---
    static constexpr int kMoveCooldownFrames = 8;
    static constexpr int kEdgeMarginPx = 24;    // how conservative to sit back from sight edge
    static constexpr int kThreatRadiusPx = 256;   // if hostile nearby, step back and re-hold
    static constexpr int kRepositionStepPx = 160;
    static constexpr int kCalmFramesToReturn = 72;
    int lastThreatFrame = -100000;

    // --- helpers ---
    BWAPI::Position homeRetreatPoint() const;
    void computeEnemyNatural();                 // sets enemyNaturalTile/Pos once main is known
    BWAPI::Position pickEdgeOfVisionSpot() const; // a “just outside” perch near natural choke
    bool threatenedNow() const;
    void issueMove(const BWAPI::Position& p, bool force = false, int reissueDist = 32);

    static double groundPathLengthPx(const BWAPI::Position& from, const BWAPI::Position& to);


    static BWAPI::Position clampToMapPx(const BWAPI::Position& p, int marginPx = 32);
};
