#include <cstdlib>
#include <iostream>

#include "mysimulator.hh"

namespace brown {

void MySimulator::hardwareLoop() {
    // Update timers if they're running and their target has been hit
    if(this->timx.running() && this->timXCount == (this->timx.t() - 1)) {
        this->timx.callback(*this);
    }
    if(this->timy.running() && this->timYCount == (this->timy.t() - 1)) {
        this->timy.callback(*this);
    }

    // Output pulses if they've been "queued up" by the software
    if(this->stepX || this->stepY) {
        setpin(clk, this->stepX, this->stepY, this->dirX, this->dirY, this->penDown);
        this->stepX = false; // "Queue"s are now empty
        this->stepY = false;
    }

    // Increment clock, timers
    std::int64_t xMaxJump = this->timx.t() - this->timXCount - 1;
    std::int64_t yMaxJump = this->timy.t() - this->timYCount - 1;

    std::int64_t jump = 1;
    if(this->timx.running() && this->timy.running()) {
        jump = std::min(xMaxJump, yMaxJump);
    }else if (this->timx.running()) {
        jump = xMaxJump;
    }else if (this->timy.running()) {
        jump = yMaxJump;
    }
    if(jump < 1) { // just a bit of safety
        jump = 1;
    }
        
    clk += jump;
    if(this->timx.running()) {
        uint32_t max = this->timx.t() > 1 ? this->timx.t() : 1; // As timers are initialized to have 0 period, this prevents dividing by 0
        this->timXCount = (this->timXCount + jump) % max;
    }
    if(this->timy.running()) {
        uint32_t max = this->timy.t() > 1 ? this->timy.t() : 1;
        this->timYCount = (this->timYCount + jump) % max;
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
