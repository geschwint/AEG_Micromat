// The Wi-Fi network (name + password), kept in flash (NVS namespace "wifi").
//
// The password is stored encrypted (AES-256-GCM, the network name as associated
// data). The AES key is never stored: it's derived on demand by the chip's HMAC
// hardware from a secret key in an eFuse block (purpose HMAC_UP). That eFuse
// key is read-protected, so neither software nor a flash dump can see it, and
// a copy of the flash is useless on another chip.
//
// The eFuse key is created once per chip with credsCreateKey(): automatically
// when the owner opens Settings -> Wi-Fi -> Network on a board without one (that
// choice is the approval), or with the serial command `keygen YES`. Burning an
// eFuse is permanent; it uses one of the six key blocks and changes nothing
// else. Without the key, passwords are refused (never stored in plain text).
// Risks, limits and status: HARDWARE.md, "eFuse key for the Wi-Fi password".
// Never burn it from anywhere else, or without the owner's say-so.
// HMAC_LABEL (wifi_creds.cpp) is part of the key derivation: changing it makes
// a stored password unreadable.
//
// The Arduino Wi-Fi library would also save the password, in plain text, in
// its own NVS namespace. credsBegin() turns that off and erases what's there.
#pragma once
#include <Arduino.h>

constexpr size_t WIFI_SSID_MAX = 32;
constexpr size_t WIFI_PASS_MAX = 64;

void credsBegin();           // call once at start, before any Wi-Fi use
bool credsKeyReady();        // eFuse HMAC key present
bool credsCreateKey();       // burns a random eFuse key unless there is one (once per chip; permanent); checks the result
bool credsHaveNetwork();     // a network name is stored
const char *credsSsid();     // "" if none
bool credsPassword(char *buf, size_t len);  // decrypts; false if it can't
// Saves name + password (empty password = open network). False if the
// password can't be encrypted (no key) or is too long.
bool credsSave(const char *ssid, const char *pass);
void credsForget();
