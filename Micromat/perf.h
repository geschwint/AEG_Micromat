// Loop timing statistics for diagnosing lag: slowest loop() pass and how many
// passes exceeded SLOW_US since the last report.
#pragma once
#include <Arduino.h>

class LoopStats {
public:
  static constexpr uint32_t SLOW_US = 20000;

  void record(uint32_t us) {
    loops_++;
    if (us > maxUs_) maxUs_ = us;
    if (us > SLOW_US) slow_++;
  }

  uint32_t maxUs() const { return maxUs_; }

  void report() {
    Serial.printf("[%7lu] PERF %lu loops, slowest %lu us, %lu over %lu ms\n", millis(), (unsigned long)loops_,
                  (unsigned long)maxUs_, (unsigned long)slow_, (unsigned long)(SLOW_US / 1000));
    loops_ = maxUs_ = slow_ = 0;
  }

private:
  uint32_t loops_ = 0, maxUs_ = 0, slow_ = 0;
};

extern LoopStats loopStats;
