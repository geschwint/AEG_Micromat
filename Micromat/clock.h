// Wall clock from the PCF85063A RTC (I2C 0x51). The RTC holds UTC; local time
// comes from LOCAL_TZ. update() re-reads the RTC a few times per second.
#pragma once
#include <Arduino.h>
#include <time.h>

class Clock {
public:
  void begin();  // Wire must already be started
  void update(uint32_t now);
  bool valid() const { return valid_; }  // RTC answered and its time is trustworthy
  const struct tm &local() const { return local_; }
  bool setUtc(const char *text);  // "YYYY-MM-DD HH:MM:SS"
  bool setUtcEpoch(time_t t);     // e.g. from NTP
  bool setLocalTime(int hour, int minute);  // today's date kept, seconds = 0

private:
  bool writeUtc(const struct tm &utc);
  bool valid_ = false;
  struct tm local_ = {};
  uint32_t lastRead_ = 0;
};
