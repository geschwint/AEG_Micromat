// Wi-Fi setup from a phone (Settings -> Wi-Fi -> Network). Opens the oven's
// own temporary network (hotspot.h: QR code on screen to join it). A small web
// page (captive portal, also at 192.168.4.1) lists the networks in range; the
// user picks one and types the password. The oven tests it and saves it only
// if it connects (password encrypted, see wifi_creds.h).
//
// The hotspot is closed when the user leaves the screen, 20 s after a
// successful save, or after SETUP_IDLE_MS without activity. All non-blocking;
// starting and stopping the radio stalls loop() for a moment.
#pragma once
#include <Arduino.h>

class WifiSetup {
public:
  enum class State : uint8_t { Off, Waiting, Testing, Success, NotFound, NoConnect, SaveFailed, TimedOut };

  void start();
  void stop();
  void update(uint32_t now);

  bool active() const { return running_; }  // radio in use (auto time waits)
  State state() const { return state_; }
  bool phoneConnected() const;
  const char *testSsid() const { return testSsid_; }

private:
  void finishTest(State result);
  void registerPages();

  bool running_ = false;
  State state_ = State::Off;
  uint32_t lastActivity_ = 0;
  uint32_t testStart_ = 0;
  uint32_t successAt_ = 0;
  bool scanning_ = false;
  char testSsid_[33] = "";
  char testPass_[65] = "";
};

extern WifiSetup wifiSetup;
