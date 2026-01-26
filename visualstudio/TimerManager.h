#pragma once
#include "Timer.h"
#include <BWAPI.h>
#include <vector> 

class TimerManager
{
	std::vector<Timer> moduleTimers;
    std::string timerNames[8] = { "All", "Economy", "Scouting", "Build", "Combat", "Information", "Strategy", "MapTools"};
    int moduleBarWidth = 0;

public:
	enum Module {All, Economy, Scouting, Build, Combat, Information, Strategy, MapTools, NumModules};

    TimerManager();

    void startTimer(const TimerManager::Module module);
    void stopTimer(const TimerManager::Module module);
    void displayTimers(int x, int y);
    double getTotalElapsed();
};

