#include <cstdlib>
#include <tuple>
#include <cmath>

#include "mysimulator.hh"
#include "ri.hh"

#define CLOCK_FREQ 100_000_000

namespace brown {

// The current time in clock ticks
int64_t clk = 0;

// The open-loop position estimate of the plotter in steps.
int64_t posX, posY = 0;



void MySimulator::hardwareLoop() {
  auto newVels = getDesiredVelocity(posX, posY, prevVelX, prevVelY);
  double_t newVelX = std::get<0>(newVels);
  double_t newVelY = std::get<1>(newVels);

  bool pulX = std::abs(newVelX) * tslsX >= 1;
  bool pulY = std::abs(newVelY) * tslsY >= 1;
  bool 

  setpin(clk, );

  // Increment time
  tslsX++;
  tslsY++;
  clk++;
}

// Get the velocity that the plotter should be moving at in steps per tick.
std::tuple<double_t, double_t> getDesiredVelocity(int64_t posX, int64_t posY, double_t prevVelX, double_t prevVelY) {
  return std::make_tuple(0.000001, 0.000001);
}

} // namespace brown
