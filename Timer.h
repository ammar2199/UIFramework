#pragma once
#include <chrono>

class WindowRoot;

// TODO: Need to test this more.
class Timer {
 public:
   Timer(WindowRoot* const window, double interval);
   Timer(const Timer& t) = delete;

   Timer& operator=(const Timer& t) = delete;
   
   ~Timer();

   void Start();
   void Stop();

   double GetInterval() const {
     return mInterval;
   }

   void SetInterval(double interval) {
     mInterval = interval;
   }

   // Used to get the next timeout in the loop.
   double TimeTillNextInterval(
       std::chrono::time_point<std::chrono::high_resolution_clock> current) const;
   int IntervalsPassed(
       std::chrono::time_point<std::chrono::high_resolution_clock> current) const;

 private:
  WindowRoot* mWindow; // Again, a weak-pointer of sorts, the Window should 
                       // always outlive it's Timer. Actually, a View
                       // should always outlive it too.
  double mInterval; // Unit in Seconds
  std::chrono::time_point<std::chrono::high_resolution_clock> mStart;
};
