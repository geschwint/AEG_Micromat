// Owns every output pin. Other modules only *request* states; update() applies
// the safety interlocks and writes the pins:
//  - the magnetron runs only with the door closed and the fan already running
//    for FAN_LEAD_MS
//  - the magnetron is always switched before the fan, so it never runs alone
//  - everything is OFF from the first line of begin()
#pragma once
#include <Arduino.h>

enum Output : uint8_t {
  OUT_MAGNETRON,
  OUT_FAN,
  OUT_TURNTABLE,
  OUT_LAMP,
  OUT_LED_ROTATE,
  OUT_LED_MAXPOWER,
  OUT_COUNT
};

class Outputs {
public:
  void begin();
  void request(Output o, bool on) { requested_[o] = on; }
  void update(uint32_t now, bool doorClosed);
  bool isOn(Output o) const { return actual_[o]; }
  bool isRequested(Output o) const { return requested_[o]; }
  static const char *name(Output o);

private:
  void write(Output o, bool on);

  bool requested_[OUT_COUNT] = {};
  bool actual_[OUT_COUNT] = {};
  uint32_t fanOnSince_ = 0;
};
