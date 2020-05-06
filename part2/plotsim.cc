#include <cstdlib>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <thread>

#include "app.hh"
#include "timer.hh"
#include "mysimulator.hh"

#include "ri.hh"



// --- CONFIG ---

#define CLOCK_FREQ_HZ 100000000.0
#define PULSES_PER_REV 800.0
#define CM_PER_REV 1.0
#define MIN_PULSES_PER_SECOND 900.0//1000.0
#define MAX_PULSES_PER_SECOND 9000.0//10000.0
#define ACCELERATION_TIME_SECS 0.2//0.150
// #define SAFETY_FACTOR 0.1

double_t PULSES_PER_CM = (PULSES_PER_REV / CM_PER_REV);
double_t MIN_PULSES_PER_TICK = /*(1 - SAFETY_FACTOR) * */(MIN_PULSES_PER_SECOND / CLOCK_FREQ_HZ);
double_t MAX_PULSES_PER_TICK = /*(1 - SAFETY_FACTOR) * */(MAX_PULSES_PER_SECOND / CLOCK_FREQ_HZ);
double_t ACCELERATION_TIME_TICKS = /*(1 + SAFETY_FACTOR) * */(ACCELERATION_TIME_SECS * CLOCK_FREQ_HZ);



// --- ACCELERATION CURVE ---

/*
This section defines the curve-specific constants and functions
used for timing out the major axis of a move.
As the acceleration limit for the simulator is constant w.r.t
speed, it makes sense to use a linear acceleration curve.
However, for a real system, this section could be modified
to instead use an exponential.
*/

// The (constant) rate of acceleration
double_t ACCELERATION_PULSES_PER_TICK_PER_TICK = ((MAX_PULSES_PER_TICK - MIN_PULSES_PER_TICK ) / ACCELERATION_TIME_TICKS);

// The distance it takes to accelerate from the mimimum speed to the maximum speed
double_t DISTANCE_TO_FULLY_ACCELERATE = ((MIN_PULSES_PER_TICK * ACCELERATION_TIME_TICKS)\
                                      + (0.5 * ACCELERATION_PULSES_PER_TICK_PER_TICK * (ACCELERATION_TIME_TICKS * ACCELERATION_TIME_TICKS)));

// The time it takes to cover a distance when accelerating from the minimum speed
double TIME_TO_ACCELERATE_OVER_DISTANCE(double distance_pulses) {
  return (-MIN_PULSES_PER_TICK + std::sqrt((MIN_PULSES_PER_TICK * MIN_PULSES_PER_TICK) + (2 * ACCELERATION_PULSES_PER_TICK_PER_TICK * distance_pulses))) / ACCELERATION_PULSES_PER_TICK_PER_TICK;
}



// --- MOVE TIMING ---

/*
This section defines the timing for a major-axis
move dependent on whatever curve is given above.
*/

// Calculate the time it takes to cover a portion of a total distance when accelerating from and then back to the minimum speed
double TIME_TO_POSITION_IN_MOVE(double targetDistance_pulses, double totalDistance_pulses) {
  // Given the length of the move (and the curve) determine what to do at which points.
  double_t accel_dist = std::min(totalDistance_pulses/2, DISTANCE_TO_FULLY_ACCELERATE);
  double_t const_vel_dist = std::max(0.0, totalDistance_pulses - (2 * accel_dist));

  if(targetDistance_pulses < accel_dist) { // Acceleration
    return TIME_TO_ACCELERATE_OVER_DISTANCE(targetDistance_pulses);
  }else {
    double_t accel_time = TIME_TO_ACCELERATE_OVER_DISTANCE(accel_dist);
    if(targetDistance_pulses < (accel_dist + const_vel_dist)) { // Constant velocity
      return accel_time + ((targetDistance_pulses - accel_dist) / MAX_PULSES_PER_TICK);
    }else { // Deceleration
      double_t const_vel_time = const_vel_dist / MAX_PULSES_PER_TICK;
      return accel_time + const_vel_time + (accel_time - TIME_TO_ACCELERATE_OVER_DISTANCE(totalDistance_pulses - targetDistance_pulses));
    }
  }
}



// --- CONTROL ---
// The open-loop position estimate of the plotter in steps.
int64_t actualX, actualY = 0;
// An running index of the next move.
int64_t nextMoveIdx = 0;
// The current move parameters in ticks and steps.
int64_t startCLK = 0; // Initial time and position
int64_t startX = 0;
int64_t startY = 0;
int64_t targetX = 0; // Target position
int64_t targetY = 0;
int64_t absTotalDeltaX = 0; // The distances being moved
int64_t absTotalDeltaY = 0;
bool dirX = false; // The directions being moved
bool dirY = false;
bool penDown = false; // Whether the pen is down or up

static void userInit(brown::Simulator& sim) {} // Startup is automatic as it it starts at its initial target (0, 0).

// Timers must have positive periods. If we want something to happen *NOW* it'll still have to wait a tick.
static void safelySetTimerPeriod(brown::Timer &tim, uint32_t period) {
    tim.setPeriod(std::max(uint32_t(1), period));
}

// Configure the x timer based on current position and move parameters
static void updateTimerX(brown::MySimulator &mysim) {
    // Number of ticks and steps that have happened since move start
    int64_t currDeltaCLK = (mysim.getTick() - startCLK);
    int64_t absCurrDeltaX = std::abs(actualX - startX);
    int64_t absCurrDeltaY = std::abs(actualY - startY);

    // Times are pretty short, prescaler can stay 1:1
    mysim.timx.setPrescaler(0);
    
    // Update timeout till next step
    if(absTotalDeltaX >= absTotalDeltaY) { // X is major axis
        safelySetTimerPeriod(mysim.timx, TIME_TO_POSITION_IN_MOVE(absCurrDeltaX + 1, absTotalDeltaX) - currDeltaCLK);
    }else {// Y is major axis
        if(absTotalDeltaX != 0) { // If it has to move at all. If not, and the timer keeps running, it'll turn itself off next update.
            // std::cout << "absTotalDeltaX: " << absTotalDeltaX << "\n"; TODO
            double_t scaleFactor = (absTotalDeltaY * 1.0) / absTotalDeltaX;
            safelySetTimerPeriod(mysim.timx, TIME_TO_POSITION_IN_MOVE(scaleFactor * (absCurrDeltaX + 1), absTotalDeltaY) - currDeltaCLK);
        }
    }

    // The period is from now, must reset any accumulated count on the timers
    mysim.resetTimerXCount();
}

// Configure the y timer based on current position and move parameters
static void updateTimerY(brown::MySimulator &mysim) {
    // Number of ticks and steps that have happened since move start
    int64_t currDeltaCLK = (mysim.getTick() - startCLK);
    int64_t absCurrDeltaX = std::abs(actualX - startX);
    int64_t absCurrDeltaY = std::abs(actualY - startY);

    // Times are pretty short, prescalers can stay 1:1
    mysim.timy.setPrescaler(0);

    // Update timeouts till next steps
    if(absTotalDeltaX >= absTotalDeltaY) { // X is major axis
        if(absTotalDeltaY != 0) { // If it has to move at all. If not, and the timer keeps running, it'll turn itself off next update.
            // std::cout << "absTotalDeltaY: " << absTotalDeltaY << "\n"; TODO
            double_t scaleFactor = (absTotalDeltaX * 1.0) / absTotalDeltaY;
            safelySetTimerPeriod(mysim.timy, TIME_TO_POSITION_IN_MOVE(scaleFactor * (absCurrDeltaY + 1), absTotalDeltaX) - currDeltaCLK);
        }
    }else {// Y is major axis
        safelySetTimerPeriod(mysim.timy, TIME_TO_POSITION_IN_MOVE(absCurrDeltaY + 1, absTotalDeltaY) - currDeltaCLK);
    }

    // The periods are from now, must reset any accumulated count on the timers
    mysim.resetTimerYCount();
}

// As moves are completed, update move parameters and restart the timers
static void userLoop(brown::Simulator& sim) {
    brown::MySimulator &mysim = (brown::MySimulator&) sim;
    
    if(actualX == targetX && actualY == targetY) { // Current move complete! Start the next one
        // Don't start the next one if there's no more to do.
        if(nextMoveIdx > 0 && state[nextMoveIdx][0] == 0 && state[nextMoveIdx][1] == 0 && state[nextMoveIdx][2] == 0) {
            return; // Done!
        }

        // Update move parameters
        startCLK = mysim.getTick();
        startX = actualX;
        startY = actualY;
        targetX = state[nextMoveIdx][1] * PULSES_PER_CM * 0.01;
        targetY = state[nextMoveIdx][2] * PULSES_PER_CM * 0.01;
        absTotalDeltaX = std::abs(targetX - startX);
        absTotalDeltaY = std::abs(targetY - startY);
        dirX = targetX > startX;
        dirY = targetY > startY;
        penDown = state[nextMoveIdx][0];
        nextMoveIdx++;

        // Configure and start the timers
        updateTimerX(mysim);
        updateTimerY(mysim);
        mysim.timx.start();
        mysim.timy.start();
    }
}

static void timxCallback(brown::Timer& tim, brown::Simulator& sim) {
    brown::MySimulator &mysim = (brown::MySimulator&) sim;

    if(actualX != targetX) {
        mysim.stepX = true;
        mysim.dirX = dirX;
        mysim.penDown = penDown;
        actualX += (dirX ? 1 : -1);
        updateTimerX(mysim); // Configure itself for the next step to take.
    }else {
        tim.stop(); // Stop moving once the target is reached
    }
}

static void timyCallback(brown::Timer& tim, brown::Simulator& sim) {
    brown::MySimulator &mysim = (brown::MySimulator&) sim;

    if(actualY != targetY) {
        mysim.stepY = true;
        mysim.dirY = dirY;
        mysim.penDown = penDown;
        actualY += (dirY ? 1 : -1);
        updateTimerY(mysim); // Configure itself for the next step to take.
    }else {
        tim.stop(); // Stop moving once the target is reached
    }
}

int main(int argc, const char * argv[]) {
    brown::Timer timx(timxCallback);
    brown::Timer timy(timyCallback);
    brown::MySimulator sim(timx, timy);
    brown::App app(sim, userInit, userLoop);
    std::thread logicThread(&brown::App::runLogic, &app);
    int retCode = app.runGraphics();
    logicThread.join();
    return retCode;
}
