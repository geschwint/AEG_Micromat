// Non-blocking buzzer: start a beep pattern and it plays in the background
// while update() is called from loop(). Volume comes from Settings.
#pragma once
#include <Arduino.h>

class Buzzer {
public:
  void begin();
  void beep(uint8_t count, uint16_t onMs = 200, uint16_t offMs = 150);
  void click();                             // key feedback (off if Settings: key clicks off)
  void error() { beep(1, 600, 0); }         // action refused
  void done() { beep(3, 200, 150); }        // cooking finished
  void update(uint32_t now);

private:
  void sound(bool on);

  uint8_t remaining_ = 0;  // beeps still to start
  bool on_ = false;
  bool active_ = false;
  uint16_t onMs_ = 0, offMs_ = 0;
  uint32_t phaseStart_ = 0;
};
