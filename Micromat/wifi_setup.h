// Wi-Fi setup from a phone (Settings -> Wi-Fi -> Network). The oven opens its
// own temporary network ("Micromat-XXXX", new random password each time, shown
// on screen with a QR code to join it). A small web page (captive portal, also
// at 192.168.4.1) lists the networks in range; the user picks one and types
// the password. The oven tests it and saves it only if it connects (password
// encrypted, see wifi_creds.h).
//
// The setup network is closed when the user leaves the screen, 20 s after a
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
  const char *apName() const { return apName_; }
  const char *apPass() const { return apPass_; }
  const char *qrText() const { return qr_; }       // WIFI:... (join by scanning)
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
  uint8_t stations_ = 0;  // phones joined to the setup network (polled each second)
  uint32_t lastStationPoll_ = 0;
  char apName_[20] = "";
  char apPass_[12] = "";
  char qr_[64] = "";
  char testSsid_[33] = "";
  char testPass_[65] = "";
};

extern WifiSetup wifiSetup;
