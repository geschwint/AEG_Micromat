// RTC test for the Soldered 333051 (PCF85063A, I2C 0x51) on the shared I2C bus
// (GPIO47/48) of the Waveshare ESP32-S3-Touch-LCD-2.
// The RTC holds UTC; local time is derived with a POSIX TZ rule (handles DST).
// Prints the time every second and shows it on screen.
// Serial commands (115200):
//   T YYYY-MM-DD HH:MM:SS   set RTC to this UTC time (clock restarts on receipt)
//   R                       print RTC registers
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <time.h>

#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1
#define LCD_BL   1

#define I2C_SDA 48
#define I2C_SCL 47

#define RTC_ADDR      0x51
#define REG_CONTROL_1 0x00
#define REG_SECONDS   0x04  // bit 7 = OS (oscillator stopped since last set)

#define CTRL1_STOP 0x20  // stops the clock and resets the sub-second prescaler

// Central European Time with EU summer time rules (matches the PC: W. Europe Standard Time).
#define LOCAL_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 1 /* landscape */, true /* IPS */, 240, 320);

uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

bool rtcRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)RTC_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool rtcWrite(uint8_t reg, const uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(reg);
  Wire.write(buf, len);
  return Wire.endTransmission() == 0;
}

// Read UTC time from the RTC. osFlag is true if the oscillator stopped
// (e.g. power lost with no/flat battery), meaning the time is not trustworthy.
bool rtcGetTime(struct tm &t, bool &osFlag) {
  uint8_t r[7];  // seconds, minutes, hours, days, weekdays, months, years
  if (!rtcRead(REG_SECONDS, r, sizeof(r))) return false;
  osFlag = r[0] & 0x80;
  t = {};
  t.tm_sec = bcd2bin(r[0] & 0x7F);
  t.tm_min = bcd2bin(r[1] & 0x7F);
  t.tm_hour = bcd2bin(r[2] & 0x3F);  // 24-hour mode (power-on default)
  t.tm_mday = bcd2bin(r[3] & 0x3F);
  t.tm_wday = r[4] & 0x07;
  t.tm_mon = bcd2bin(r[5] & 0x1F) - 1;
  t.tm_year = bcd2bin(r[6]) + 100;  // RTC stores 00..99 for 2000..2099
  return true;
}

// Set UTC time. STOP is set while writing so the clock (and its sub-second
// prescaler) restarts exactly when STOP is cleared; this also clears OS.
bool rtcSetTime(const struct tm &t) {
  uint8_t ctrl1;
  if (!rtcRead(REG_CONTROL_1, &ctrl1, 1)) return false;
  uint8_t stopped = ctrl1 | CTRL1_STOP;
  if (!rtcWrite(REG_CONTROL_1, &stopped, 1)) return false;

  uint8_t r[7] = {
    bin2bcd(t.tm_sec),  // writing seconds clears the OS flag (bit 7 = 0)
    bin2bcd(t.tm_min),
    bin2bcd(t.tm_hour),
    bin2bcd(t.tm_mday),
    (uint8_t)t.tm_wday,
    bin2bcd(t.tm_mon + 1),
    bin2bcd(t.tm_year - 100),
  };
  bool ok = rtcWrite(REG_SECONDS, r, sizeof(r));

  uint8_t running = ctrl1 & ~CTRL1_STOP;
  return rtcWrite(REG_CONTROL_1, &running, 1) && ok;
}

// Parse "YYYY-MM-DD HH:MM:SS" (UTC) and fill in the weekday.
bool parseUtc(const String &s, struct tm &t) {
  int Y, M, D, h, m, sec;
  if (sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &sec) != 6) return false;
  if (Y < 2000 || Y > 2099 || M < 1 || M > 12 || D < 1 || D > 31 || h > 23 || m > 59 || sec > 59) return false;
  t = {};
  t.tm_year = Y - 1900;
  t.tm_mon = M - 1;
  t.tm_mday = D;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = sec;
  // Weekday via mktime in UTC (TZ is switched back to local afterwards).
  setenv("TZ", "UTC0", 1);
  tzset();
  time_t epoch = mktime(&t);
  setenv("TZ", LOCAL_TZ, 1);
  tzset();
  struct tm check;
  gmtime_r(&epoch, &check);
  t.tm_wday = check.tm_wday;
  return check.tm_mday == D;  // rejects dates like 2026-02-31
}

// Convert RTC UTC time to an epoch, then to local time via the TZ rule.
time_t utcToEpoch(struct tm t) {
  setenv("TZ", "UTC0", 1);
  tzset();
  time_t e = mktime(&t);
  setenv("TZ", LOCAL_TZ, 1);
  tzset();
  return e;
}

void showTime(const struct tm &local, const struct tm &utc, bool osFlag) {
  char buf[32];
  gfx->setTextColor(osFlag ? RGB565_RED : RGB565_WHITE, RGB565_BLACK);
  gfx->setTextSize(5);
  gfx->setCursor(40, 60);
  strftime(buf, sizeof(buf), "%H:%M:%S", &local);
  gfx->print(buf);

  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_CYAN, RGB565_BLACK);
  gfx->setCursor(40, 120);
  strftime(buf, sizeof(buf), "%a %Y-%m-%d %Z", &local);
  gfx->print(buf);

  gfx->setTextColor(RGB565_DARKGREY, RGB565_BLACK);
  gfx->setCursor(40, 150);
  strftime(buf, sizeof(buf), "UTC %H:%M:%S", &utc);
  gfx->print(buf);

  gfx->setCursor(40, 190);
  gfx->setTextColor(osFlag ? RGB565_RED : RGB565_GREEN, RGB565_BLACK);
  gfx->print(osFlag ? "Time invalid - set it " : "Clock OK              ");
}

void printRegisters() {
  uint8_t r[11];
  if (!rtcRead(0x00, r, sizeof(r))) {
    Serial.println("register read failed");
    return;
  }
  Serial.print("regs 00-0A:");
  for (uint8_t v : r) Serial.printf(" %02X", v);
  Serial.println();
}

void handleSerial() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (line.length() < 40) line += c;
      continue;
    }
    line.trim();
    if (line.startsWith("T ")) {
      struct tm t;
      if (parseUtc(line.substring(2), t) && rtcSetTime(t)) {
        Serial.printf("SET OK %s\n", line.substring(2).c_str());
      } else {
        Serial.println("SET FAILED (format: T YYYY-MM-DD HH:MM:SS, UTC)");
      }
    } else if (line == "R") {
      printRegisters();
    } else if (line.length()) {
      Serial.println("commands: T YYYY-MM-DD HH:MM:SS (UTC) | R");
    }
    line = "";
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Test_RTC starting");
  setenv("TZ", LOCAL_TZ, 1);
  tzset();

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(40, 20);
  gfx->print("RTC test (PCF85063A)");

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  Serial.print("I2C scan:");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a);
  }
  Serial.println();
  printRegisters();
}

void loop() {
  handleSerial();

  static int lastSec = -1;
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll < 50) return;
  lastPoll = millis();

  struct tm utc;
  bool osFlag;
  if (!rtcGetTime(utc, osFlag)) {
    static unsigned long lastErr = 0;
    if (millis() - lastErr > 2000) {
      lastErr = millis();
      Serial.println("RTC read failed (not at 0x51?)");
    }
    return;
  }
  if (utc.tm_sec == lastSec) return;
  lastSec = utc.tm_sec;

  time_t epoch = utcToEpoch(utc);
  struct tm local;
  localtime_r(&epoch, &local);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &local);
  Serial.printf("RTC %04d-%02d-%02d %02d:%02d:%02d UTC | local %s%s\n",
                utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec,
                buf, osFlag ? " | OS FLAG: time invalid" : "");
  showTime(local, utc, osFlag);
}
