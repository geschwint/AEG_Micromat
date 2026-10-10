// The oven's own temporary Wi-Fi network ("Micromat-XXXX"), used by the Wi-Fi
// setup and firmware update screens (one at a time). Each start() picks a new
// random password; qrText() is the "WIFI:..." text a phone camera joins with.
// A DNS server answers every name with our address, so phones open the page by
// themselves (captive portal). The radio runs as AP + station, so the screens
// can also scan for or join the home network.
//
// Also the small HTML helpers both web pages share.
#pragma once
#include <Arduino.h>

constexpr const char *HOTSPOT_IP = "192.168.4.1";  // softAP default

void hotspotStart();
void hotspotStop();               // radio off
void hotspotUpdate(uint32_t now); // DNS + joined-phone count; call while active
bool hotspotActive();
const char *hotspotName();
const char *hotspotPass();        // also used as the firmware update password
const char *hotspotQr();
uint8_t hotspotStations();        // devices joined (polled each second)

// HTML: page head with the shared style and an <h1>; optional auto-refresh (2 s).
String htmlStart(const char *heading, bool refresh = false);
void htmlEscape(String &out, const char *s);
