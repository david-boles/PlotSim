#ifndef _MYSIMULATOR_HH_
#define _MYSIMULATOR_HH_

#include <cstdint>

#include "timer.hh"
#include "simulator.hh"

namespace brown {

class MySimulator: public Simulator {
public:
    MySimulator(Timer& timx, Timer& timy):
        Simulator(), timx(timx), timy(timy) {};
    void hardwareLoop();
    std::int64_t getTick();
    void resetTimerXCount();
    void resetTimerYCount();
    // "Registers" that allow the user code to communicate with the hardware.
    bool stepX = false;
    bool stepY = false;
    bool dirX = false;
    bool dirY = false;
    bool penDown = false;

    // Timers that the user code can configure.
    Timer& timx;
    Timer& timy;

private:
    std::int64_t timXCount = 0;
    std::int64_t timYCount = 0;
    std::int64_t clk = 0;
};

} // namespace brown

#endif // _MYSIMULATOR_HH_
