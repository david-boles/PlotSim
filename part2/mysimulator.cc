#include <cstdlib>
#include <iostream>

#include "mysimulator.hh"
#include "ri.hh"

namespace brown {

void MySimulator::hardwareLoop() {
    // Update timers if they're running and their target has been hit
    if(this->timx.running() && this->timXCount == (this->timx.t() - 1)) {
        this->timx.callback(*this);
    }
    if(this->timy.running() && this->timYCount == (this->timy.t() - 1)) {
        this->timy.callback(*this);
    }

    // Output pulses if applicable
    setpin(clk, this->stepX, this->stepY, this->dirX, this->dirY, this->penDown);
    this->stepX = false;
    this->stepY = false;

    // Increment clock, timers
    clk++;
    if(this->timx.running()) {
        if(this->timx.t() == 0) {
            std::cout << "timx.t(): " << 0 << "\n"; // TODO
        }
        this->timXCount = (this->timXCount + 1) % this->timx.t();
    }
    if(this->timy.running()) {
        if(this->timy.t() == 0) {
            std::cout << "timy.t(): " << 0 << "\n";
        }
        this->timYCount = (this->timYCount + 1) % this->timy.t();
    }
}

std::int64_t MySimulator::getTick() {
    return this->clk;
}

void MySimulator::resetTimerXCount() {
    this->timXCount = 0;
}

void MySimulator::resetTimerYCount() {
    this->timYCount = 0;
}

} // namespace brown
