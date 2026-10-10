#include "hotspot.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_random.h>

static DNSServer dns;
static bool active_ = false;
static char name_[20] = "";
static char pass_[12] = "";
static char qr_[64] = "";
static uint8_t stations_ = 0;
static uint32_t lastPoll_ = 0;

void hotspotStart() {
  if (active_) return;
  WiFi.mode(WIFI_AP_STA);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);  // from eFuse; WiFi.macAddress() can still be zeros here
  snprintf(name_, sizeof(name_), "Micromat-%02X%02X", mac[4], mac[5]);
  static const char CHARS[] = "abcdefghjkmnpqrstuvwxyz23456789";  // no 0/o, 1/l/i
  for (uint8_t i = 0; i < 8; i++) pass_[i] = CHARS[esp_random() % (sizeof(CHARS) - 1)];
  pass_[8] = '\0';
  snprintf(qr_, sizeof(qr_), "WIFI:T:WPA;S:%s;P:%s;;", name_, pass_);
  // Log joins and leaves (with the reason), to diagnose devices that can't connect.
  static bool events = false;
  if (!events) {
    events = true;
    WiFi.onEvent([](arduino_event_id_t e, arduino_event_info_t info) {
      if (e == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        const uint8_t *m = info.wifi_ap_staconnected.mac;
        Serial.printf("[%7lu] hotspot: device %02x:%02x:%02x:%02x:%02x:%02x joined\n", millis(), m[0], m[1], m[2],
                      m[3], m[4], m[5]);
      } else if (e == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        Serial.printf("[%7lu] hotspot: device left (reason %u)\n", millis(), info.wifi_ap_stadisconnected.reason);
      } else if (e == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
        Serial.printf("[%7lu] hotspot: device got %s\n", millis(),
                      IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
      }
    });
  }
  bool ok = WiFi.softAP(name_, pass_);
  dns.start(53, "*", WiFi.softAPIP());
  stations_ = 0;
  active_ = true;
  Serial.printf("[%7lu] hotspot \"%s\" %s, %s, MAC %s, channel %d\n", millis(), name_, ok ? "open" : "FAILED TO START",
                WiFi.softAPIP().toString().c_str(), WiFi.softAPmacAddress().c_str(), WiFi.channel());
}

void hotspotStop() {
  if (!active_) return;
  dns.stop();
  WiFi.scanDelete();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  memset(pass_, 0, sizeof(pass_));
  memset(qr_, 0, sizeof(qr_));
  active_ = false;
  stations_ = 0;
  Serial.printf("[%7lu] hotspot closed, Wi-Fi off\n", millis());
}

void hotspotUpdate(uint32_t now) {
  if (!active_) return;
  dns.processNextRequest();
  if (now - lastPoll_ >= 1000) {
    lastPoll_ = now;
    stations_ = WiFi.softAPgetStationNum();
  }
}

bool hotspotActive() { return active_; }
const char *hotspotName() { return name_; }
const char *hotspotPass() { return pass_; }
const char *hotspotQr() { return qr_; }
uint8_t hotspotStations() { return active_ ? stations_ : 0; }

String htmlStart(const char *heading, bool refresh) {
  String p = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'>");
  if (refresh) p += F("<meta http-equiv='refresh' content='2'>");
  p += F("<title>Micromat</title><style>"
         "body{font-family:sans-serif;background:#101418;color:#e0e6ea;margin:0;padding:16px;max-width:480px}"
         "h1{color:#ffa000;font-size:1.4em}label.n{display:block;padding:10px;margin:6px 0;background:#263238;"
         "border-radius:6px}input[type=text],input[type=password],input[type=file]{width:100%;box-sizing:border-box;"
         "padding:10px;font-size:1em;margin:6px 0 12px}button{background:#ffa000;border:0;border-radius:6px;"
         "padding:12px 20px;font-size:1em}a{color:#9eb3c2}.err{color:#ff7961}.ok{color:#66bb6a}"
         ".r{float:right;color:#9eb3c2}.dim{color:#9eb3c2}</style></head><body><h1>");
  htmlEscape(p, heading);
  p += "</h1>";
  return p;
}

void htmlEscape(String &out, const char *s) {
  for (; *s; s++) {
    switch (*s) {
      case '&':  out += "&amp;"; break;
      case '<':  out += "&lt;"; break;
      case '>':  out += "&gt;"; break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default:   out += *s;
    }
  }
}
