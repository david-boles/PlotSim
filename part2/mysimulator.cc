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
        this->timXCount = (this->timXCount + jump) % this->timx.t();
    }
    if(this->timy.running()) {
        this->timYCount = (this->timYCount + jump) % this->timy.t();
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
