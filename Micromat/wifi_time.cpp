#include "wifi_time.h"
#include "debug_log.h"
#include <WiFi.h>
#include <esp_sntp.h>
#include "config.h"
#include "settings.h"
#include "wifi_creds.h"
#include "wifi_setup.h"
#include "fw_update.h"

static constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
static constexpr uint32_t NTP_TIMEOUT_MS = 15000;
static constexpr uint32_t RESYNC_MS = 24UL * 3600 * 1000;
static constexpr uint32_t RETRY_MS = 10UL * 60 * 1000;

static bool enabled() { return settings.wifiOn && settings.autoTime && credsHaveNetwork(); }

void WifiTime::begin(Clock &clock) {
  clock_ = &clock;
  nextTry_ = millis();
}

void WifiTime::syncNow() { nextTry_ = millis(); }

const char *WifiTime::status() const {
  if (!settings.wifiOn) return "Wi-Fi off";
  if (!settings.autoTime) return "auto time off";
  if (!credsHaveNetwork()) return "no network set";
  if (wifiSetup.active()) return "setup running";
  if (firmwareUpdate.active()) return "update screen open";
  return status_;
}

void WifiTime::cancel() {
  if (phase_ != Phase::Idle) finish(millis(), false, "cancelled");
}

void WifiTime::update(uint32_t now) {
  if (wifiSetup.active() || firmwareUpdate.active()) return;  // those screens own the radio
  if (phase_ == Phase::Idle) {
    if (!enabled() || (int32_t)(now - nextTry_) < 0) return;
    char pass[WIFI_PASS_MAX + 1];
    if (!credsPassword(pass, sizeof(pass))) {
      nextTry_ = now + RETRY_MS;
      snprintf(status_, sizeof(status_), "can't read password");
      Log.printf("[%7lu] wifi: %s, retry in 10 min\n", now, status_);
      return;
    }
    Log.printf("[%7lu] wifi: connecting to \"%s\"\n", now, credsSsid());
    WiFi.mode(WIFI_STA);
    WiFi.begin(credsSsid(), pass);
    memset(pass, 0, sizeof(pass));
    phase_ = Phase::Connecting;
    phaseStart_ = now;
    snprintf(status_, sizeof(status_), "connecting");
    return;
  }
  if (!enabled()) {
    finish(now, false, "cancelled");
    return;
  }
  if (phase_ == Phase::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      Log.printf("[%7lu] wifi: connected, asking NTP\n", now);
      sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
      configTzTime(LOCAL_TZ, "pool.ntp.org", "time.google.com");
      phase_ = Phase::WaitNtp;
      phaseStart_ = now;
      snprintf(status_, sizeof(status_), "waiting for NTP");
    } else if (now - phaseStart_ > CONNECT_TIMEOUT_MS) {
      finish(now, false, "no connection");
    }
  } else if (phase_ == Phase::WaitNtp) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      bool ok = clock_->setUtcEpoch(time(nullptr));
      finish(now, ok, ok ? "synced" : "RTC write failed");
    } else if (now - phaseStart_ > NTP_TIMEOUT_MS) {
      finish(now, false, "no NTP answer");
    }
  }
}

void WifiTime::finish(uint32_t now, bool ok, const char *why) {
  if (phase_ == Phase::WaitNtp) esp_sntp_stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  phase_ = Phase::Idle;
  nextTry_ = now + (ok ? RESYNC_MS : RETRY_MS);
  if (ok) {
    time_t t = time(nullptr);
    struct tm local;
    localtime_r(&t, &local);
    snprintf(status_, sizeof(status_), "synced at %02d:%02d", local.tm_hour, local.tm_min);
  } else {
    snprintf(status_, sizeof(status_), "%s, retry in 10 min", why);
  }
  Log.printf("[%7lu] wifi: %s, Wi-Fi off\n", now, status_);
}
