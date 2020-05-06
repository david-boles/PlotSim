#include <cstdlib>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <iostream>

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






namespace brown {

// The current time in clock ticks
int64_t clk = 0;

// The open-loop position estimate of the plotter in steps.
int64_t actualX, actualY = 0;


int64_t startCLK = 0;
int64_t startX = 0;
int64_t startY = 0;
int64_t targetX = 0;
int64_t targetY = 0;
int64_t absTotalDeltaX = std::abs(targetX - startX);
int64_t absTotalDeltaY = std::abs(targetY - startY);
bool dirX = targetX > startX;
bool dirY = targetY > startY;
bool penDown = false;

int64_t timerX = 0;
int64_t timerY = 0;

bool startingUp = false;

int64_t nextLineIdx = 0;

void MySimulator::hardwareLoop() {
  // Determine whether to take a step
  bool stepX = false, stepY = false;
  if(timerX == 0 && actualX != targetX && !startingUp) {
    stepX = true;
    // std::cout << "took X step @ " << clk << "\n";
  }
  if(timerY == 0 && actualY != targetY && !startingUp) {
    stepY = true;
    // std::cout << "took Y step @ " << clk << "\n";
  }

  // Take step
  setpin(clk, stepX, stepY, dirX, dirY, penDown);
  actualX += stepX ? (dirX ? 1 : -1) : 0;
  actualY += stepY ? (dirY ? 1 : -1) : 0;


  // Update controllers if needed
  if(actualX == targetX && actualY == targetY) { // Target reached! Time for the next path
    if(nextLineIdx > 0 && state[nextLineIdx][0] == 0 && state[nextLineIdx][1] == 0 && state[nextLineIdx][2] == 0) {
      return; // Done!
    }

    startCLK = clk;
    startX = actualX;
    startY = actualY;
    targetX = state[nextLineIdx][1] * PULSES_PER_CM * 0.01;
    targetY = state[nextLineIdx][2] * PULSES_PER_CM * 0.01;
    absTotalDeltaX = std::abs(targetX - startX);
    absTotalDeltaY = std::abs(targetY - startY);
    dirX = targetX > startX;
    dirY = targetY > startY;
    penDown = state[nextLineIdx][0];
    startingUp = true;
    nextLineIdx++;

  }else if (stepX || stepY || startingUp) { // Step taken or new line, update clocks!
    int64_t currDeltaCLK = (clk - startCLK);
    int64_t absCurrDeltaX = std::abs(actualX - startX);
    int64_t absCurrDeltaY = std::abs(actualY - startY);
    
    // Update timeouts till next steps
    if(absTotalDeltaX >= absTotalDeltaY) { // X is major axis
      double_t scaleFactor = (absTotalDeltaX * 1.0) / absTotalDeltaY;
      timerX = TIME_TO_POSITION_IN_MOVE(absCurrDeltaX + 1, absTotalDeltaX) - currDeltaCLK;
      timerY = TIME_TO_POSITION_IN_MOVE(scaleFactor * (absCurrDeltaY + 1), absTotalDeltaX) - currDeltaCLK;
    }else {// Y is major axis
      double_t scaleFactor = (absTotalDeltaY * 1.0) / absTotalDeltaX;
      timerY = TIME_TO_POSITION_IN_MOVE(absCurrDeltaY + 1, absTotalDeltaY) - currDeltaCLK;
      timerX = TIME_TO_POSITION_IN_MOVE(scaleFactor * (absCurrDeltaX + 1), absTotalDeltaY) - currDeltaCLK;
    }
    
    startingUp = false;
  }
  

  // Increment time
  clk++;
  timerX--;
  timerY--;
  if(timerX < 0) {
    timerX = 0;
  }
  if(timerY < 0) {
    timerY = 0;
  }
}

} // namespace brown
