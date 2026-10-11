// Firmware update over Wi-Fi (Settings -> Firmware update), only while that
// screen is open and the oven is idle. It opens the oven's hotspot (hotspot.h)
// and, if a home network is set up, joins that too. Two ways to send the new
// firmware (the .bin from `arduino-cli compile --export-binaries`):
//   - browser: http://192.168.4.1 (or the home-network address), enter the
//     password on screen, choose the file (a phone's small captive-portal
//     browser may not open a file picker: use the normal browser then);
//   - arduino-cli / espota over the network (ArduinoOTA, port 3232), same password.
// The password is the hotspot's, new and random each time the screen opens.
//
// The transfer itself blocks loop() for 10-40 s (WebServer / ArduinoOTA read
// the whole image), so the loop watchdog is paused and a progress hook keeps
// the door, outputs, buzzer and display going meanwhile. The image goes into
// the other OTA slot; the board restarts into it after a successful check.
//
// Rollback: the new firmware starts "on trial" (bootloader rollback is enabled
// in this core). confirmIfHealthy() marks it valid after HEALTHY_AFTER_MS of
// normal running; if it crashes or hangs before that, the next reset goes back
// to the previous firmware.
//
// The same screen and page, opened from Settings -> Debug log -> Download
// (Mode::Logs), offer the debug log files on the SD card instead (download,
// delete; debug_log.h). There is no firmware upload or ArduinoOTA then, and
// a sign-in stays valid until the screen closes.
#pragma once
#include <Arduino.h>

class FirmwareUpdate {
public:
  enum class State : uint8_t { Off, Waiting, Receiving, Done, Failed, TimedOut };
  enum class Mode : uint8_t { Firmware, Logs };

  void start(Mode mode = Mode::Firmware);
  void stop();
  void update(uint32_t now);
  void setProgressHook(void (*hook)()) { hook_ = hook; }  // runs during the blocking transfer
  void confirmIfHealthy(uint32_t now);                    // call every loop()

  bool active() const { return running_; }
  Mode mode() const { return mode_; }
  State state() const { return state_; }
  uint8_t progress() const { return progress_; }  // percent while receiving
  const char *error() const { return error_; }
  const char *homeIp() const { return homeIp_; }  // "" if not on the home network
  static const char *version();                   // build fingerprint (8 hex digits)
  static const char *runningSlot();               // "app0" / "app1"

  // Used by the transfer callbacks.
  void transferStart(size_t total);
  void transferProgress(size_t done);
  void transferDone();
  void transferFailed(const char *why);

private:
  void registerPages();
  void sendLogFile(uint8_t i);
  void startOta();

  bool running_ = false;
  Mode mode_ = Mode::Firmware;
  State state_ = State::Off;
  uint8_t progress_ = 0;
  size_t total_ = 0;
  uint32_t lastActivity_ = 0;
  uint32_t lastHook_ = 0;
  uint32_t restartAt_ = 0;
  bool joiningHome_ = false;
  bool otaStarted_ = false;
  char homeIp_[16] = "";
  char error_[40] = "";
  char token_[17] = "";  // after a correct sign-in: one upload (Firmware) or until closed (Logs)
  void (*hook_)() = nullptr;
};

extern FirmwareUpdate firmwareUpdate;
