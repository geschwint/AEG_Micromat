// User settings and favourites, kept in flash (NVS, via Preferences) so they
// survive power cuts. Most values are indexes into the choice tables below.
#pragma once
#include <Arduino.h>

constexpr uint8_t BRIGHTNESS_PCT[] = { 20, 40, 60, 80, 100 };
constexpr uint8_t DIM_LEVEL_PCT[] = { 0, 5, 10, 20, 30 };        // 0 = backlight off
constexpr uint16_t DIM_DELAY_S[] = { 30, 60, 120, 300, 600, 0 };  // 0 = never dim
// Buzzer LEDC duty (10 bit; 512 = 50 % square wave, the loudest).
constexpr uint16_t VOLUME_DUTY[] = { 12, 64, 512 };
constexpr uint8_t NUM_BRIGHTNESS = sizeof(BRIGHTNESS_PCT) / sizeof(BRIGHTNESS_PCT[0]);
constexpr uint8_t NUM_DIM_LEVEL = sizeof(DIM_LEVEL_PCT) / sizeof(DIM_LEVEL_PCT[0]);
constexpr uint8_t NUM_DIM_DELAY = sizeof(DIM_DELAY_S) / sizeof(DIM_DELAY_S[0]);
constexpr uint8_t NUM_VOLUME = sizeof(VOLUME_DUTY) / sizeof(VOLUME_DUTY[0]);
constexpr uint8_t NUM_FAVOURITES = 4;

enum class Language : uint8_t { English, Swedish, Count };

struct Favourite {
  uint32_t seconds = 0;  // 0 = empty slot
  uint8_t power = 100;
  bool rotate = true;
};

// Stored as one blob: bump SETTINGS_KEY in settings.cpp when the layout changes
// (old data is then ignored and the defaults below apply).
struct Settings {
  uint8_t brightness = 4;  // index into BRIGHTNESS_PCT
  uint8_t dimLevel = 2;    // index into DIM_LEVEL_PCT
  uint8_t dimDelay = 2;    // index into DIM_DELAY_S
  uint8_t volume = 2;      // index into VOLUME_DUTY
  bool keyClicks = true;   // click on key presses (end and error beeps always sound)
  Language language = Language::English;
  bool autoTime = false;   // set the clock over Wi-Fi (NTP)
  bool wifiOn = false;     // master switch: Wi-Fi is never started when off (network: wifi_creds)
  Favourite favourites[NUM_FAVOURITES];
  uint16_t defrostGrams = 500;  // last weight used, the next defrost starts from it
};

extern Settings settings;
void loadSettings();
void saveSettings();  // writes to flash, takes a few ms; call only after a user change
