#include "Timer.h"

#include "View.h"

#include <cassert>
#include <chrono>

Timer::Timer(WindowRoot* const window, double interval) 
             : mWindow(window), mInterval(interval) {
  assert(mWindow && "Timer must be constructed with non-null window");
}

Timer::~Timer() {
  Stop();
}

void Timer::Start() {
  mStart = std::chrono::high_resolution_clock::now();
  mWindow->AddTimer(this);
}

void Timer::Stop() {
  mWindow->RemoveTimer(this);
}

double Timer::TimeTillNextInterval(
       std::chrono::time_point<std::chrono::high_resolution_clock> current) const {
  // Time-elapsed since start of interval.
  // eg: 0.30(current time) % 0.25 (interval) -> 0.05 seconds have elapsed.
  //
  // XXX: Is the current - mStart correct?
  std::chrono::duration<double> t = current - mStart;
  double elapsedInInterval = std::fmod(t.count(), mInterval);
 
  // 0.25 - 0.05 = 20 seconds remain till next interval 
  // time (0.50 seconds)
  double timeLeftTillNextInterval = mInterval - elapsedInInterval;
  
  return timeLeftTillNextInterval;
}

int Timer::IntervalsPassed(
    std::chrono::time_point<std::chrono::high_resolution_clock> current) const {
  std::chrono::duration<double> t = (current - mStart);
  return static_cast<int>(t.count() / mInterval);
}

