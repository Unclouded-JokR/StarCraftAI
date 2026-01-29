#include "TimerManager.h"

TimerManager::TimerManager() : moduleTimers(std::vector<Timer>(NumModules)), moduleBarWidth(40)
{
    
}

void TimerManager::startTimer(const Module module)
{
    moduleTimers[module].start();
}

void TimerManager::stopTimer(const Module module)
{ 
    moduleTimers[module].stop();
}

double TimerManager::getTotalElapsed()
{
	return moduleTimers[0].getElapsedTimeInMilliSec();
}

void TimerManager::displayTimers(int x, int y)
{
    BWAPI::Broodwar->drawBoxScreen(x - 5, y - 5, x + 110 + moduleBarWidth, y + 5 + (10 * moduleTimers.size()), BWAPI::Colors::Black, true);

    int yskip = 0;
    const double total = moduleTimers[0].getElapsedTimeInMilliSec();
    for (size_t i(0); i < moduleTimers.size(); ++i)
    {
        const double elapsed = moduleTimers[i].getElapsedTimeInMilliSec();
        if (elapsed > 42)
        {
            BWAPI::Broodwar->printf("Timer Debug: %s %lf", timerNames[i].c_str(), elapsed);
        }

        int width = (int)((elapsed == 0) ? 0 : (moduleBarWidth * (elapsed / total)));

        BWAPI::Broodwar->drawTextScreen(x, y + yskip - 3, "\x04 %s", timerNames[i].c_str());
        BWAPI::Broodwar->drawBoxScreen(x + 60, y + yskip, x + 60 + width + 1, y + yskip + 8, BWAPI::Colors::White);
        BWAPI::Broodwar->drawTextScreen(x + 70 + moduleBarWidth, y + yskip - 3, "%.4lf", elapsed);
        yskip += 10;
    }
}