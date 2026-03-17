#pragma once

#include <BWAPI.h>
#include <unordered_map>
#include <vector>

class ThreatGrid
{
public:
    void onStart();
    void onFrameStart(int frame);

    void addOrUpdateEnemy(int id, BWAPI::UnitType type, BWAPI::Position pos, bool completed, bool burrowed, bool immobile);
    void removeEnemy(int id);

    int groundThreatAt(BWAPI::Position p) const;
    int detectionAt(BWAPI::Position p) const;
    int airThreatAt(BWAPI::Position p) const;
    int getAirThreat(BWAPI::Position p) const;
    int getDetection(BWAPI::Position p) const;
    int airThreatValue(BWAPI::UnitType t) const;
    int airThreatRangePx(BWAPI::UnitType t) const;

private:
    struct EnemyStamp
    {
        BWAPI::UnitType type;
        BWAPI::Position pos;
        bool completed = false;
        bool burrowed = false;
        bool immobile = false;
    };

    struct GridData
    {
        int w = 0;
        int h = 0;
        std::vector<int> data;

        void init(int walkW, int walkH)
        {
            w = walkW;
            h = walkH;
            data.assign(w * h, 0);
        }

        int idx(int x, int y) const
        {
            return x * h + y;
        }

        int get(int x, int y) const
        {
            if (x < 0 || y < 0 || x >= w || y >= h)
            {
                return 0;
            }
            return data[idx(x, y)];
        }

        void add(int x, int y, int delta)
        {
            if (x < 0 || y < 0 || x >= w || y >= h)
            {
                return;
            }
            data[idx(x, y)] += delta;
        }
    };

    int currentFrame_ = 0;
    int rangeBufferPx_ = 16;

    GridData groundThreat_;
    GridData airThreat_;
    GridData detection_;


    std::unordered_map<int, EnemyStamp> stamps_;

    static int toWalkX(BWAPI::Position p)
    {
        return p.x >> 3;
    }

    static int toWalkY(BWAPI::Position p)
    {
        return p.y >> 3;
    }

    void paintDisc(GridData& g, BWAPI::Position center, int rangePx, int delta);
    void stampEnemy(const EnemyStamp& s, int delta);

    int detectorRadiusPx(BWAPI::UnitType t) const;
    int groundThreatValue(BWAPI::UnitType t) const;
    int groundThreatRangePx(BWAPI::UnitType t) const;
};



