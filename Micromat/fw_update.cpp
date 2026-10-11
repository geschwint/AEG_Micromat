#include "fw_update.h"
#include "about.h"
#include <ArduinoOTA.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_random.h>
#include "hotspot.h"
#include "settings.h"
#include "ui_text.h"
#include "wifi_creds.h"

FirmwareUpdate firmwareUpdate;

static constexpr uint32_t IDLE_CLOSE_MS = 10UL * 60 * 1000;  // no requests, no joined device: close
static constexpr uint32_t RESTART_DELAY_MS = 3000;            // let the page and screen show "done"
static constexpr uint32_t HOOK_EVERY_MS = 200;
static constexpr uint32_t HEALTHY_AFTER_MS = 30000;
static const char *const HEADING = "Micromat firmware";

static WebServer server(80);
static bool authOk = false;  // the current upload was signed in

// The build fingerprint: start of the image's SHA-256, different for every
// build (__DATE__/__TIME__ would only change when this file is recompiled).
const char *FirmwareUpdate::version() {
  static char v[12] = "";
  if (!v[0]) esp_app_get_elf_sha256(v, 9);
  return v;
}

const char *FirmwareUpdate::runningSlot() {
  const esp_partition_t *p = esp_ota_get_running_partition();
  return p ? p->label : "?";
}

// --- Transfer (shared by the web upload and ArduinoOTA) -------------------------------------
void FirmwareUpdate::transferStart(size_t total) {
  disableLoopWDT();  // the transfer blocks loop(); the progress hook keeps the rest going
  total_ = total;
  progress_ = 0;
  error_[0] = '\0';
  state_ = State::Receiving;
  lastHook_ = 0;
  Serial.printf("[%7lu] update: receiving %u bytes\n", millis(), (unsigned)total);
}

void FirmwareUpdate::transferProgress(size_t done) {
  if (total_) progress_ = min<size_t>(done * 100 / total_, 100);
  uint32_t now = millis();
  if (hook_ && now - lastHook_ >= HOOK_EVERY_MS) {
    lastHook_ = now;
    hook_();
  }
}

void FirmwareUpdate::transferDone() {
  state_ = State::Done;
  progress_ = 100;
  restartAt_ = millis() + RESTART_DELAY_MS;
  Serial.printf("[%7lu] update: installed, restarting in %lu s\n", millis(), RESTART_DELAY_MS / 1000);
}

void FirmwareUpdate::transferFailed(const char *why) {
  if (Update.isRunning()) Update.abort();
  strlcpy(error_, why, sizeof(error_));
  state_ = State::Failed;
  enableLoopWDT();
  lastActivity_ = millis();
  Serial.printf("[%7lu] update FAILED: %s\n", millis(), why);
}

// --- Web page ---------------------------------------------------------------------------------
void FirmwareUpdate::registerPages() {
  static bool done = false;
  if (done) return;
  done = true;

  // Sign-in is a plain form on the page (not HTTP Basic auth: the small
  // "captive portal" browsers phones open by themselves can't show that
  // dialog and stay blank). A correct password gives a one-time token in the
  // upload form's address.
  auto loginPage = [](bool wrong) {
    String p = htmlStart(HEADING);
    p += "<p class='dim'>";
    p += tr(Str::PageVersion);
    p += ": ";
    p += FIRMWARE_VERSION;
    p += ", build ";
    p += version();
    p += " (";
    p += runningSlot();
    p += ")</p>";
    if (wrong) {
      p += "<p class='err'>";
      p += tr(Str::PageWrongPassword);
      p += "</p>";
    }
    p += "<form method='post' action='/login'><p>";
    p += tr(Str::Password);
    p += "<input type='password' name='pw' autocapitalize='none' autocorrect='off'></p><button type='submit'>";
    p += tr(Str::PageSignIn);
    p += "</button></form></body></html>";
    server.send(200, "text/html", p);
  };

  server.on("/", HTTP_GET, [this, loginPage]() {
    lastActivity_ = millis();
    loginPage(false);
  });

  server.on("/login", HTTP_POST, [this, loginPage]() {
    lastActivity_ = millis();
    if (server.arg("pw") != hotspotPass()) {
      Serial.printf("[%7lu] update: wrong password on the page\n", millis());
      return loginPage(true);
    }
    snprintf(token_, sizeof(token_), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    String p = htmlStart(HEADING);
    p += "<form method='post' action='/update?t=";
    p += token_;
    p += "' enctype='multipart/form-data'><p>";
    p += tr(Str::PageFile);
    p += "<input type='file' name='firmware' accept='.bin,application/octet-stream'></p><button type='submit'>";
    p += tr(Str::PageInstall);
    p += "</button></form><p class='dim'>";
    p += tr(Str::PageUpdateNote);
    p += "</p><p class='dim'>";
    p += tr(Str::PageBrowserHint);
    p += "</p></body></html>";
    server.send(200, "text/html", p);
  });

  // Runs after the upload: report the result.
  server.on(
    "/update", HTTP_POST,
    [this]() {
      lastActivity_ = millis();
      String p = htmlStart(HEADING);
      if (!authOk) {
        p += "<p class='err'>";
        p += tr(Str::PageWrongPassword);
        p += "</p><p><a href='/'>";
        p += tr(Str::PageTryAgain);
        p += "</a>";
      } else if (state_ == State::Done) {
        p += "<p class='ok'>";
        p += tr(Str::PageUpdateOk);
      } else {
        p += "<p class='err'>";
        p += tr(Str::PageUpdateFailed);
        p += " ";
        htmlEscape(p, error_[0] ? error_ : "?");
        p += "</p><p><a href='/'>";
        p += tr(Str::PageTryAgain);
        p += "</a>";
      }
      p += "</p></body></html>";
      server.send(200, "text/html", p);
    },
    // Runs for each chunk of the uploaded file. The token is in the address,
    // which is parsed before the body.
    [this]() {
      HTTPUpload &u = server.upload();
      if (u.status == UPLOAD_FILE_START) {
        authOk = token_[0] && server.arg("t") == token_;
        if (!authOk) {
          Serial.printf("[%7lu] update: upload without a valid sign-in, ignored\n", millis());
          return;
        }
        token_[0] = '\0';  // one upload per sign-in
        transferStart(server.clientContentLength());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) transferFailed(Update.errorString());
      } else if (!authOk || state_ != State::Receiving) {
        return;
      } else if (u.status == UPLOAD_FILE_WRITE) {
        if (Update.write(u.buf, u.currentSize) != u.currentSize) transferFailed(Update.errorString());
        else transferProgress(Update.progress());
      } else if (u.status == UPLOAD_FILE_END) {
        if (Update.end(true)) transferDone();  // checks the image, then makes it the boot slot
        else transferFailed(Update.errorString());
      } else if (u.status == UPLOAD_FILE_ABORTED) {
        transferFailed("upload aborted");
      }
    });

  server.onNotFound([this]() {  // captive-portal probes etc.: to the page
    lastActivity_ = millis();
    server.sendHeader("Location", "http://" + server.client().localIP().toString() + "/");
    server.send(302);
  });
}

// --- Control ------------------------------------------------------------------------------------
void FirmwareUpdate::start() {
  if (running_) return;
  hotspotStart();
  // Also join the home network if one is set up, so the PC can send from there.
  homeIp_[0] = '\0';
  joiningHome_ = false;
  char pass[WIFI_PASS_MAX + 1];
  if (settings.wifiOn && credsHaveNetwork() && credsPassword(pass, sizeof(pass))) {
    WiFi.begin(credsSsid(), pass);
    joiningHome_ = true;
  }
  memset(pass, 0, sizeof(pass));

  registerPages();
  server.begin();

  ArduinoOTA.setHostname("micromat");
  ArduinoOTA.setPassword(hotspotPass());
  ArduinoOTA.setRebootOnSuccess(false);  // restart from update(), after showing "done"
  static bool callbacks = false;
  if (!callbacks) {
    callbacks = true;
    ArduinoOTA.onStart([this]() { transferStart(Update.size()); });
    ArduinoOTA.onProgress([this](unsigned int done, unsigned int total) {
      total_ = total;
      transferProgress(done);
    });
    ArduinoOTA.onEnd([this]() { transferDone(); });
    ArduinoOTA.onError([this](ota_error_t e) {
      transferFailed(e == OTA_AUTH_ERROR      ? "wrong password"
                     : e == OTA_BEGIN_ERROR   ? "begin failed"
                     : e == OTA_CONNECT_ERROR ? "connect failed"
                     : e == OTA_RECEIVE_ERROR ? "receive failed"
                                              : "end failed");
    });
  }
  ArduinoOTA.begin();
  otaStarted_ = true;

  running_ = true;
  token_[0] = '\0';
  state_ = State::Waiting;
  progress_ = 0;
  error_[0] = '\0';
  lastActivity_ = millis();
  Serial.printf("[%7lu] update: open (running %s, %s)\n", lastActivity_, version(), runningSlot());
}

void FirmwareUpdate::stop() {
  if (!running_) return;
  if (otaStarted_) ArduinoOTA.end();
  otaStarted_ = false;
  server.stop();
  hotspotStop();
  running_ = false;
  token_[0] = '\0';
  homeIp_[0] = '\0';
  if (state_ == State::Waiting || state_ == State::Receiving) state_ = State::Off;
  enableLoopWDT();
  Serial.printf("[%7lu] update: closed\n", millis());
}

void FirmwareUpdate::update(uint32_t now) {
  if (!running_) return;
  hotspotUpdate(now);
  server.handleClient();  // may run a whole upload (blocking, with the progress hook)
  ArduinoOTA.handle();    // likewise
  if (joiningHome_ && WiFi.status() == WL_CONNECTED) {
    joiningHome_ = false;
    strlcpy(homeIp_, WiFi.localIP().toString().c_str(), sizeof(homeIp_));
    Serial.printf("[%7lu] update: also on the home network at %s\n", now, homeIp_);
  }
  if (hotspotStations() > 0) lastActivity_ = now;

  if (state_ == State::Done && (int32_t)(now - restartAt_) >= 0) {
    Serial.println("update: restarting into the new firmware");
    Serial.flush();
    ESP.restart();  // relays drop out with the reset (all outputs default off)
  }
  // Signed: lastActivity_ is set from millis() in start() and the web handlers,
  // so it can be a little later than `now` (taken at the start of loop()).
  if (state_ != State::Done && state_ != State::Receiving &&
      (int32_t)(now - lastActivity_) > (int32_t)IDLE_CLOSE_MS) {
    stop();
    state_ = State::TimedOut;
  }
}

// Rollback: the first run of a new firmware is on trial; confirm it once it
// has run normally for a while (otherwise a reset goes back to the old one).
void FirmwareUpdate::confirmIfHealthy(uint32_t now) {
  static bool checked = false;
  if (checked || now < HEALTHY_AFTER_MS) return;
  checked = true;
  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
      st == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    Serial.printf("[%7lu] update: new firmware confirmed (%s)\n", now, esp_err_to_name(err));
  }
}

// Keep the new firmware "on trial" until confirmIfHealthy() (instead of the
// core confirming it at boot).
extern "C" bool verifyRollbackLater() { return true; }
