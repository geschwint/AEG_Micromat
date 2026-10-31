#include "clock.h"
#include "config.h"
#include <Wire.h>

static constexpr uint8_t RTC_ADDR = 0x51;
static constexpr uint8_t REG_CONTROL_1 = 0x00;
static constexpr uint8_t REG_SECONDS = 0x04;  // bit 7 = OS (oscillator stopped)
static constexpr uint8_t CTRL1_STOP = 0x20;
static constexpr uint32_t READ_INTERVAL_MS = 250;

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static bool rtcRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(RTC_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static bool rtcWrite(uint8_t reg, const uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(reg);
  Wire.write(buf, len);
  return Wire.endTransmission() == 0;
}

// UTC struct tm -> epoch (days from the civil date, as timegm()). Out-of-range
// days roll over, e.g. Feb 31 = Mar 3. Not via setenv("TZ") + mktime(): newlib's
// setenv leaks the old string when the value grows, and this ran 4 times a
// second, so memory ran out after ~12 h (2026-10: watchdog resets while idle).
static time_t utcToEpoch(const struct tm &t) {
  int y = t.tm_year + 1900;
  int m = t.tm_mon + 1;  // 1..12
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - era * 400;
  int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + t.tm_mday - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int64_t days = (int64_t)era * 146097 + doe - 719468;
  return (time_t)(days * 86400 + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec);
}

void Clock::begin() {
  setenv("TZ", LOCAL_TZ, 1);
  tzset();
  update(millis() + READ_INTERVAL_MS);
}

void Clock::update(uint32_t now) {
  if (now - lastRead_ < READ_INTERVAL_MS) return;
  lastRead_ = now;

  uint8_t r[7];  // seconds, minutes, hours, days, weekdays, months, years
  if (!rtcRead(REG_SECONDS, r, sizeof(r))) {
    valid_ = false;
    return;
  }
  struct tm utc = {};
  utc.tm_sec = bcd2bin(r[0] & 0x7F);
  utc.tm_min = bcd2bin(r[1] & 0x7F);
  utc.tm_hour = bcd2bin(r[2] & 0x3F);
  utc.tm_mday = bcd2bin(r[3] & 0x3F);
  utc.tm_mon = bcd2bin(r[5] & 0x1F) - 1;
  utc.tm_year = bcd2bin(r[6]) + 100;
  if (utc.tm_mon < 0 || utc.tm_mon > 11) {  // garbage from the RTC
    valid_ = false;
    return;
  }
  time_t epoch = utcToEpoch(utc);
  localtime_r(&epoch, &local_);
  valid_ = !(r[0] & 0x80);
}

// Same command format as Test_RTC, so Test_RTC/set_rtc_time.ps1 works here too.
// STOP is held while writing, so the clock restarts when STOP is released.
bool Clock::setUtc(const char *text) {
  int Y, M, D, h, m, s;
  if (sscanf(text, "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return false;
  if (Y < 2000 || Y > 2099 || M < 1 || M > 12 || D < 1 || D > 31 || h > 23 || m > 59 || s > 59) return false;
  struct tm t = {};
  t.tm_year = Y - 1900;
  t.tm_mon = M - 1;
  t.tm_mday = D;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  time_t epoch = utcToEpoch(t);
  struct tm check;
  gmtime_r(&epoch, &check);
  if (check.tm_mday != D) return false;  // e.g. 2026-02-31
  return writeUtc(check);
}

bool Clock::setUtcEpoch(time_t t) {
  struct tm utc;
  gmtime_r(&t, &utc);
  if (utc.tm_year < 100 || utc.tm_year > 199) return false;  // RTC holds 2000-2099
  return writeUtc(utc);
}

bool Clock::setLocalTime(int hour, int minute) {
  struct tm t = local_;
  if (t.tm_year < 100) {  // RTC never read: use a placeholder date (NTP fixes it)
    t = {};
    t.tm_year = 126;
    t.tm_mday = 1;
  }
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = 0;
  t.tm_isdst = -1;  // let mktime work out DST for that date
  time_t epoch = mktime(&t);  // local TZ is active
  return epoch != (time_t)-1 && setUtcEpoch(epoch);
}

bool Clock::writeUtc(const struct tm &utc) {
  uint8_t ctrl1;
  if (!rtcRead(REG_CONTROL_1, &ctrl1, 1)) return false;
  uint8_t stopped = ctrl1 | CTRL1_STOP;
  if (!rtcWrite(REG_CONTROL_1, &stopped, 1)) return false;
  uint8_t r[7] = { bin2bcd(utc.tm_sec), bin2bcd(utc.tm_min), bin2bcd(utc.tm_hour), bin2bcd(utc.tm_mday),
                   (uint8_t)utc.tm_wday, bin2bcd(utc.tm_mon + 1), bin2bcd(utc.tm_year - 100) };
  bool ok = rtcWrite(REG_SECONDS, r, sizeof(r));  // also clears the OS flag
  uint8_t running = ctrl1 & ~CTRL1_STOP;
  ok = rtcWrite(REG_CONTROL_1, &running, 1) && ok;
  lastRead_ = millis() - READ_INTERVAL_MS;  // refresh on next update()
  return ok;
}
