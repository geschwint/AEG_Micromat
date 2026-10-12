#include "debug_log.h"
#include <SD.h>
#include <SPI.h>
#include "about.h"
#include "clock.h"
#include "config.h"
#include "settings.h"

DebugLog Log;

static const char *const FILE_NAMES[DebugLog::FILES] = { "micromat.log", "micromat-old.log" };
static const char *const FILE_PATHS[DebugLog::FILES] = { "/micromat.log", "/micromat-old.log" };

// The display's SPI peripheral and pins: FSPI (SPI2), as given to
// Arduino_ESP32SPI in Micromat.ino. Another peripheral on the same pins
// would take them from the display (it goes blank), and so would
// sdSpi.end(): never call it.
static SPIClass sdSpi(FSPI);

const char *DebugLog::fileName(uint8_t i) { return i < FILES ? FILE_NAMES[i] : ""; }

// Start-up text kept until begin(); more than this is cut off (with a note).
static constexpr size_t EARLY_BYTES = 4096;

void DebugLog::begin(Clock &clock) {
  clock_ = &clock;
  if (!lock_) lock_ = xSemaphoreCreateMutex();
  if (settings.debugLog) setEnabled(true);  // writes the start-up text to the buffer first
  early_ = false;
  free(earlyBuf_);
  earlyBuf_ = nullptr;
}

// --- Writing (any task) ----------------------------------------------------------------
size_t DebugLog::write(const uint8_t *buf, size_t len) {
  Serial.write(buf, len);
  if (early_) {  // only setup() runs this early: no lock needed
    if (!earlyBuf_) earlyBuf_ = (char *)malloc(EARLY_BYTES);  // PSRAM (malloc prefers it, see crash_report.h)
    if (earlyBuf_ && earlyLen_ < EARLY_BYTES) {
      size_t n = min(len, EARLY_BYTES - earlyLen_);
      memcpy(earlyBuf_ + earlyLen_, buf, n);
      earlyLen_ += n;
      if (n < len) dropped_ += len - n;
    } else {
      dropped_ += len;
    }
    return len;
  }
  if (active_) buffer(buf, len);
  return len;
}

void DebugLog::buffer(const uint8_t *buf, size_t len) {
  if (!buf_ || !lock_) return;
  xSemaphoreTake(lock_, portMAX_DELAY);
  auto put = [this](char c) {
    size_t next = (head_ + 1) % LOG_BUF_BYTES;
    if (next == tail_) {
      dropped_++;
      return;
    }
    buf_[head_] = c;
    head_ = next;
  };
  for (size_t i = 0; i < len; i++) {
    if (lineStart_) {
      char stamp[24];
      if (clock_ && clock_->valid()) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S ", &clock_->local());
      else strlcpy(stamp, "---------- --:--:-- ", sizeof(stamp));
      for (const char *s = stamp; *s; s++) put(*s);
    }
    put((char)buf[i]);
    lineStart_ = buf[i] == '\n';
  }
  xSemaphoreGive(lock_);
}

size_t DebugLog::take(char *out, size_t max) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  size_t n = 0;
  while (n < max && tail_ != head_) {
    out[n++] = buf_[tail_];
    tail_ = (tail_ + 1) % LOG_BUF_BYTES;
  }
  xSemaphoreGive(lock_);
  return n;
}

// --- Card (loop() only) ----------------------------------------------------------------------
// Looking for a card takes about 1.5 s when there is none (loop() is blocked
// meanwhile), so after a miss there's no new try for MOUNT_RETRY_MS; several
// tries can't add up towards the loop watchdog.
static constexpr uint32_t MOUNT_RETRY_MS = 5000;

bool DebugLog::mount() {
  if (mounted_) return true;
  static uint32_t lastMiss = 0;
  static bool missed = false;
  if (missed && millis() - lastMiss < MOUNT_RETRY_MS) return false;
  static bool spiStarted = false;
  if (!spiStarted) spiStarted = sdSpi.begin(PIN_LCD_SCLK, PIN_LCD_MISO, PIN_LCD_MOSI, -1);
  uint32_t t0 = millis();
  mounted_ = SD.begin(PIN_SD_CS, sdSpi, LOG_SD_HZ, "/sd", 2);
  if (mounted_) {
    Log.printf("[%7lu] SD card: %llu MB, %llu MB free (mounted in %lu ms)\n", millis(), SD.cardSize() >> 20,
               (SD.totalBytes() - SD.usedBytes()) >> 20, millis() - t0);
  } else {
    missed = true;
    lastMiss = millis();
    Log.printf("[%7lu] SD card: none found (%lu ms)\n", millis(), millis() - t0);
  }
  return mounted_;
}

void DebugLog::unmount() {
  if (file_) file_.close();
  if (mounted_) SD.end();
  mounted_ = false;
}

bool DebugLog::cardOk() { return mount(); }

bool DebugLog::openCurrent() {
  file_ = SD.open(FILE_PATHS[0], FILE_APPEND);
  return (bool)file_;
}

// Stops logging to the card (the card is unmounted, so a new card can be
// mounted by the next action that needs one).
void DebugLog::fail(const char *what) {
  active_ = false;
  unmount();
  Log.printf("[%7lu] debug log: %s; logging to the SD card stopped\n", millis(), what);
}

bool DebugLog::setEnabled(bool on) {
  if (!on) {
    if (active_) {
      Log.printf("[%7lu] debug log: off\n", millis());
      flushNow();
    }
    active_ = false;
    if (file_) file_.close();
    return true;
  }
  if (active_) return true;
  if (!buf_) {
    buf_ = (char *)heap_caps_malloc(LOG_BUF_BYTES, MALLOC_CAP_SPIRAM);
    if (!buf_) buf_ = (char *)malloc(LOG_BUF_BYTES);
    if (!buf_) return false;
  }
  if (!mount()) return false;
  if (!openCurrent()) {
    fail("can't open the log file");
    return false;
  }
  head_ = tail_ = 0;
  lineStart_ = true;
  active_ = true;
  lastFlush_ = millis();
  if (early_) {  // from begin(): the start-up text first (stamped now; the clock wasn't read yet then)
    early_ = false;
    if (earlyLen_) buffer((const uint8_t *)earlyBuf_, earlyLen_);
    if (!lineStart_) buffer((const uint8_t *)"\n", 1);
  } else {
    dropped_ = 0;  // what was lost while logging was off doesn't count
  }
  Log.printf("[%7lu] debug log: on, writing to %s (firmware %s)\n", millis(), FILE_NAMES[0], FIRMWARE_VERSION);
  return true;
}

// Write up to `max` buffered bytes to the file; rolls over at LOG_FILE_MAX.
void DebugLog::writeOut(size_t max) {
  static char chunk[1024];
  size_t total = 0;
  while (active_ && total < max) {
    size_t n = take(chunk, sizeof(chunk));
    if (!n) break;
    if (!file_ && !openCurrent()) return fail("can't open the log file");
    if (file_.write((const uint8_t *)chunk, n) != n) return fail("write failed (card removed?)");
    total += n;
  }
  if (!active_ || !total) return;
  if (dropped_) {
    char note[64];
    int n = snprintf(note, sizeof(note), "... %lu bytes not logged (buffer full)\n", (unsigned long)dropped_);
    dropped_ = 0;
    file_.write((const uint8_t *)note, n);
  }
  file_.flush();
  if (file_.size() >= LOG_FILE_MAX) {
    file_.close();
    SD.remove(FILE_PATHS[1]);
    SD.rename(FILE_PATHS[0], FILE_PATHS[1]);
    if (!openCurrent()) return fail("can't open a new log file");
    Log.printf("[%7lu] debug log: started a new file\n", millis());
  }
}

void DebugLog::update(uint32_t now) {
  if (!active_) return;
  size_t used = (head_ + LOG_BUF_BYTES - tail_) % LOG_BUF_BYTES;
  if (now - lastFlush_ < LOG_FLUSH_MS && used < LOG_BUF_BYTES / 2) return;
  writeOut(LOG_CHUNK_BYTES);
  used = (head_ + LOG_BUF_BYTES - tail_) % LOG_BUF_BYTES;
  if (used < LOG_CHUNK_BYTES) lastFlush_ = now;  // else carry on next pass
}

void DebugLog::flushNow() {
  if (active_) writeOut(LOG_BUF_BYTES);
}

size_t DebugLog::fileSize(uint8_t i) {
  if (!mounted_ || i >= FILES) return 0;
  if (i == 0 && file_) return file_.size();
  File f = SD.open(FILE_PATHS[i], FILE_READ);
  size_t s = f ? f.size() : 0;
  if (f) f.close();
  return s;
}

File DebugLog::openFile(uint8_t i) {
  if (!mounted_ || i >= FILES) return File();
  return SD.open(FILE_PATHS[i], FILE_READ);
}

void DebugLog::removeAll() {
  if (!mounted_) return;
  flushNow();
  if (file_) file_.close();
  for (const char *p : FILE_PATHS) SD.remove(p);
  if (active_ && !openCurrent()) return fail("can't open the log file");
  Log.printf("[%7lu] debug log: files deleted\n", millis());
}
