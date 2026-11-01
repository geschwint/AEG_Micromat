// Oven control state machine. Holds the cooking settings and state, and turns
// them into output *requests* every update(); Outputs enforces the interlocks.
//
//   Idle --start--> Cooking --time up--> Done
//                    |    ^                |
//          stop/door |    | start          | door/stop/new time
//                    v    |                v
//                    Paused ---stop---->  Idle
//
// The fan runs while cooking. When heating stops (pause, done, clear, or power
// set to 0 W) it keeps running for FAN_RUNON_MS only after heavy use: at least
// FAN_RUNON_MIN_HEAT_S of heating in the last FAN_RUNON_WINDOW_S.
// At 0 W the oven is a plain timer: it counts down and beeps, with every relay off.
#pragma once
#include <Arduino.h>
#include "outputs.h"
#include "buzzer.h"
#include "config.h"

enum class OvenState : uint8_t { Idle, Cooking, Paused, Done };

class Oven {
public:
  void begin(Outputs &outputs, Buzzer &buzzer);
  void update(uint32_t now, bool doorClosed);

  // Actions (from UI, panel buttons, encoder or serial)
  void addSeconds(int32_t seconds);
  void setSeconds(uint32_t seconds);
  void nextPower();
  void setPower(uint16_t watts, bool click = true);  // nearest level
  void start();
  void stop();
  void quickStart();     // start as set; +30 s while cooking; 30 s if nothing set
  void toggleRotate();
  void setRotate(bool on);
  void doorChanged(bool closed, uint32_t now);
  void clearUnused();             // a set or paused time left unused: back to Idle, 0 s (no click)
  void setLampSleep(bool off) { lampSleep_ = off; }  // door left open: the lamp sleeps with the display

  // View
  OvenState state() const { return state_; }
  static const char *stateName(OvenState s);
  uint32_t remainingSeconds() const { return (remainingMs_ + 999) / 1000; }
  uint16_t power() const;  // watts (POWER_LEVELS)
  bool timerOnly() const { return power() == 0; }
  bool rotate() const { return rotate_; }
  bool fanRunningOn() const;
  uint16_t recentHeatSeconds() const { return heatCount_; }  // heating seconds in the run-on window

private:
  void setState(OvenState s);
  bool magnetronWanted(uint32_t now) const;
  void recordHeat(uint32_t now, bool heating);

  Outputs *out_ = nullptr;
  Buzzer *buzzer_ = nullptr;

  OvenState state_ = OvenState::Idle;
  uint32_t remainingMs_ = 0;
  uint8_t powerIdx_ = 0;  // index into POWER_LEVELS (0 = MAX_POWER_W)
  bool rotate_ = true;
  bool doorClosed_ = true;

  uint32_t lastUpdate_ = 0;
  uint32_t cookStart_ = 0;     // start of the current Cooking stretch (duty cycle reference)
  uint32_t runOnUntil_ = 0;    // fan run-on end
  bool runOn_ = false;
  bool heating_ = false;       // cooking with power > 0 in the previous update()
  uint8_t heatHist_[(FAN_RUNON_WINDOW_S + 7) / 8] = {};  // 1 bit per second: heating, ring buffer
  uint16_t heatCount_ = 0;     // bits set in heatHist_
  uint32_t heatSec_ = 0;       // last second recorded (millis / 1000)
  bool lampSleep_ = false;     // lamp off although the door is open (App: door left open)
};
