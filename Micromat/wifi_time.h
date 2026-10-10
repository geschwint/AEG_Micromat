// Automatic clock over Wi-Fi (NTP), when Wi-Fi and "Auto time" are on in
// Settings and a network is set up (Settings -> Wi-Fi -> Network, or serial
// `ssid` + `pass`). Waits while the Wi-Fi setup screen uses the radio.
// Connects at start and then once a day, writes the RTC, and turns Wi-Fi off
// again. A failed try is retried after 10 minutes. Non-blocking, except that
// starting Wi-Fi stalls loop() for a moment (driver start-up).
#pragma once
#include <Arduino.h>
#include "clock.h"

class WifiTime {
public:
  void begin(Clock &clock);
  void update(uint32_t now);
  void syncNow();              // try at the next update()
  void cancel();               // stop an attempt now (radio off)
  const char *status() const;  // for the service screen / console

private:
  enum class Phase : uint8_t { Idle, Connecting, WaitNtp };
  void finish(uint32_t now, bool ok, const char *why);

  Clock *clock_ = nullptr;
  Phase phase_ = Phase::Idle;
  uint32_t phaseStart_ = 0;
  uint32_t nextTry_ = 0;
  char status_[40] = "not synced yet";
};
