// Log output. Everything the firmware logs goes through `Log` (a Print, used
// like Serial: Log.printf(...)), which writes it to USB serial and, while
// Settings -> Debug log is on, also to a file on the microSD card, each line
// prefixed with the local date and time.
//
// The card shares the display's SPI bus (CS = PIN_SD_CS). Text is collected
// in a RAM buffer and written to the card every LOG_FLUSH_MS from update(), so
// a slow card write never happens in the middle of other work. The current
// file rolls over to the previous one at LOG_FILE_MAX bytes; only those two
// are kept. No card, or a card that fails: logging to the card stops and the
// oven runs on as normal.
//
// The files can be downloaded from Settings -> Debug log -> Download (the
// oven's hotspot, password on screen; see fw_update.h).
#pragma once
#include <Arduino.h>
#include <FS.h>

class Clock;

class DebugLog : public Print {
public:
  static constexpr uint8_t FILES = 2;  // 0 = current, 1 = previous

  void begin(Clock &clock);   // after loadSettings(); mounts the card if logging is on.
                              // Text logged before this is kept and goes to the card first.
  void update(uint32_t now);  // writes buffered text to the card now and then
  bool setEnabled(bool on);   // from Settings; false if logging is on but no card was found
  bool active() const { return active_; }  // logging to the card right now
  void flushNow();            // write everything buffered (before a restart or a download)

  // Files (for the download page and the menu)
  bool cardOk();              // mounts the card if needed (blocks ~1.5 s when there is none)
  static const char *fileName(uint8_t i);  // "micromat.log", "micromat-old.log"
  size_t fileSize(uint8_t i); // 0 = missing
  File openFile(uint8_t i);
  void removeAll();

  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t *buf, size_t len) override;

private:
  bool mount();
  void unmount();
  bool openCurrent();
  void buffer(const uint8_t *buf, size_t len);  // into the RAM buffer, date and time at each line start
  size_t take(char *out, size_t max);  // from the RAM buffer
  void writeOut(size_t max);
  void fail(const char *what);

  Clock *clock_ = nullptr;
  SemaphoreHandle_t lock_ = nullptr;  // other tasks (Wi-Fi events) log too
  char *buf_ = nullptr;
  size_t head_ = 0, tail_ = 0;        // ring buffer: write at head_, read at tail_
  uint32_t dropped_ = 0;              // bytes lost because the buffer was full
  bool lineStart_ = true;
  // Start-up text (firmware, reset reason, crash report...) logged before
  // begin(), while it isn't known yet whether logging to the card is on.
  bool early_ = true;
  char *earlyBuf_ = nullptr;
  size_t earlyLen_ = 0;
  bool mounted_ = false;
  bool active_ = false;
  File file_;
  uint32_t lastFlush_ = 0;
};

extern DebugLog Log;
