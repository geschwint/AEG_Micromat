// Door state. With SIMULATE_DOOR the state is set from the UI/serial; otherwise
// PIN_DOOR is read with a pull-up and debounced (LOW = closed, so a broken wire
// reads as open). The hardware interlocks cut the magnetron regardless.
#pragma once
#include <Arduino.h>

class Door {
public:
  void begin();
  void update(uint32_t now);
  bool closed() const { return closed_; }
  bool simulated() const;
  void simulate(bool closed);  // only used when SIMULATE_DOOR

private:
  bool closed_ = false;  // assume open until known
  bool lastRaw_ = false;
  uint32_t rawSince_ = 0;
};
