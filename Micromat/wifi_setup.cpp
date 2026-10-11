#include "wifi_setup.h"
#include "debug_log.h"
#include <WebServer.h>
#include <WiFi.h>
#include "hotspot.h"
#include "settings.h"
#include "ui_text.h"
#include "wifi_creds.h"

WifiSetup wifiSetup;

static constexpr uint32_t SETUP_IDLE_MS = 5UL * 60 * 1000;  // no phone activity: close
static constexpr uint32_t TEST_TIMEOUT_MS = 20000;
static constexpr uint32_t TEST_MIN_MS = 4000;      // early failure codes are ignored before this
static constexpr uint32_t CLOSE_AFTER_OK_MS = 20000;  // let the phone show the result first
static constexpr uint8_t MAX_NETWORKS = 15;

static WebServer server(80);

// Networks found by the last scan, strongest first, no duplicates.
static char networks[MAX_NETWORKS][33];
static int8_t networkRssi[MAX_NETWORKS];
static uint8_t networkCount = 0;

static const char *const HEADING = "Micromat Wi-Fi";

// --- Control --------------------------------------------------------------------------------
static void startScan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);  // async; results collected in update()
}

static void collectScan() {
  int n = WiFi.scanComplete();
  if (n < 0) return;  // still running (-1) or failed (-2)
  networkCount = 0;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty() || ssid.length() > 32) continue;
    bool dup = false;
    for (uint8_t j = 0; j < networkCount && !dup; j++) dup = ssid == networks[j];
    if (dup) continue;
    // Insert sorted by signal strength.
    int8_t rssi = WiFi.RSSI(i);
    uint8_t pos = networkCount;
    while (pos > 0 && networkRssi[pos - 1] < rssi) pos--;
    if (pos >= MAX_NETWORKS) continue;
    uint8_t last = networkCount < MAX_NETWORKS ? networkCount : MAX_NETWORKS - 1;
    for (uint8_t j = last; j > pos; j--) {
      memcpy(networks[j], networks[j - 1], sizeof(networks[j]));
      networkRssi[j] = networkRssi[j - 1];
    }
    strlcpy(networks[pos], ssid.c_str(), sizeof(networks[pos]));
    networkRssi[pos] = rssi;
    if (networkCount < MAX_NETWORKS) networkCount++;
  }
  WiFi.scanDelete();
}

void WifiSetup::start() {
  if (running_) return;
  hotspotStart();
  networkCount = 0;
  startScan();
  scanning_ = true;
  registerPages();
  server.begin();
  running_ = true;
  state_ = State::Waiting;
  lastActivity_ = millis();
  Log.printf("[%7lu] wifi setup: open\n", lastActivity_);
}

// Handlers are registered once (WebServer keeps them across stop/begin).
void WifiSetup::registerPages() {
  static bool done = false;
  if (done) return;
  done = true;

  server.on("/", HTTP_GET, [this]() {
    lastActivity_ = millis();
    if (state_ == State::Testing) {
      String p = htmlStart(HEADING, true);
      p += "<p>";
      p += tr(Str::PageTesting);
      p += "</p></body></html>";
      server.send(200, "text/html", p);
      return;
    }
    if (state_ == State::Success) {
      String p = htmlStart(HEADING);
      p += "<p class='ok'>";
      p += tr(Str::PageOk);
      p += "</p></body></html>";
      server.send(200, "text/html", p);
      return;
    }
    String p = htmlStart(HEADING, scanning_);
    Str err = state_ == State::NotFound ? Str::SetupNotFound
            : state_ == State::NoConnect ? Str::SetupNoConnect
            : state_ == State::SaveFailed ? Str::SetupSaveFailed
                                          : Str::Count;
    if (err != Str::Count) {
      p += "<p class='err'>";
      p += tr(err);
      p += " (";
      htmlEscape(p, testSsid_);
      p += ")</p>";
    }
    p += "<form method='post' action='/save'><h2>";
    p += tr(Str::PageChoose);
    p += "</h2>";
    if (scanning_) {
      p += "<p>";
      p += tr(Str::PageSearching);
      p += "</p>";
    }
    for (uint8_t i = 0; i < networkCount; i++) {
      p += "<label class='n'><input type='radio' name='ssid' value='";
      htmlEscape(p, networks[i]);
      p += "'> ";
      htmlEscape(p, networks[i]);
      p += "<span class='r'>";
      p += networkRssi[i];
      p += " dBm</span></label>";
    }
    p += "<p>";
    p += tr(Str::PageOther);
    p += "<input type='text' name='other' maxlength='32' autocapitalize='none' autocorrect='off'></p><p>";
    p += tr(Str::Password);
    p += "<input type='password' name='pass' maxlength='64'></p><button type='submit'>";
    p += tr(Str::PageConnect);
    p += "</button></form><p><a href='/scan'>";
    p += tr(Str::PageRescan);
    p += "</a></p></body></html>";
    server.send(200, "text/html", p);
  });

  server.on("/save", HTTP_POST, [this]() {
    lastActivity_ = millis();
    String ssid = server.arg("other");
    ssid.trim();
    if (ssid.isEmpty()) ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (!ssid.isEmpty() && ssid.length() <= WIFI_SSID_MAX && pass.length() <= WIFI_PASS_MAX &&
        state_ != State::Testing && state_ != State::Success) {
      strlcpy(testSsid_, ssid.c_str(), sizeof(testSsid_));
      strlcpy(testPass_, pass.c_str(), sizeof(testPass_));
      Log.printf("[%7lu] wifi setup: testing \"%s\"\n", millis(), testSsid_);
      WiFi.begin(testSsid_, testPass_);
      state_ = State::Testing;
      testStart_ = millis();
    }
    pass = "";
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/scan", HTTP_GET, [this]() {
    lastActivity_ = millis();
    if (state_ != State::Testing && !scanning_) {
      startScan();
      scanning_ = true;
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  // Anything else (phones probing for a captive portal): send them to the page.
  server.onNotFound([this]() {
    lastActivity_ = millis();
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302);
  });
}

void WifiSetup::stop() {
  if (!running_) return;
  server.stop();
  hotspotStop();
  memset(testPass_, 0, sizeof(testPass_));
  running_ = false;
  scanning_ = false;
  if (state_ == State::Waiting || state_ == State::Testing) state_ = State::Off;
  Log.printf("[%7lu] wifi setup: closed\n", millis());
}

bool WifiSetup::phoneConnected() const { return running_ && hotspotStations() > 0; }

void WifiSetup::finishTest(State result) {
  WiFi.disconnect(false);  // station side only; the setup network stays
  memset(testPass_, 0, sizeof(testPass_));
  state_ = result;
  Log.printf("[%7lu] wifi setup: \"%s\" %s\n", millis(), testSsid_,
                result == State::Success ? "connected, saved" : "failed");
}

void WifiSetup::update(uint32_t now) {
  if (!running_) return;
  hotspotUpdate(now);
  server.handleClient();
  if (scanning_ && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    collectScan();
    scanning_ = false;
  }
  if (hotspotStations() > 0) lastActivity_ = now;  // a joined phone keeps the setup open

  if (state_ == State::Testing) {
    wl_status_t st = WiFi.status();
    // Signed: testStart_ comes from millis() in a web handler and can be a
    // little later than `now` (taken at the start of loop()).
    int32_t t = (int32_t)(now - testStart_);
    if (st == WL_CONNECTED) {
      bool saved = credsSave(testSsid_, testPass_);
      if (saved) credsRememberIp();
      if (saved && !settings.wifiOn) {
        settings.wifiOn = true;
        saveSettings();
      }
      finishTest(saved ? State::Success : State::SaveFailed);
      successAt_ = now;
    } else if (t > (int32_t)TEST_MIN_MS && st == WL_NO_SSID_AVAIL) {
      finishTest(State::NotFound);
    } else if ((t > (int32_t)TEST_MIN_MS && st == WL_CONNECT_FAILED) || t > (int32_t)TEST_TIMEOUT_MS) {
      finishTest(State::NoConnect);
    }
  }
  // Signed for the same reason (lastActivity_ is also set from millis()).
  if (state_ == State::Success && (int32_t)(now - successAt_) > (int32_t)CLOSE_AFTER_OK_MS) {
    stop();
  } else if (state_ != State::Testing && (int32_t)(now - lastActivity_) > (int32_t)SETUP_IDLE_MS) {
    stop();
    if (state_ != State::Success) state_ = State::TimedOut;
  }
}
