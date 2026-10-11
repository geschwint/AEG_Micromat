// Built-in cooking programs: defrost by food type + weight, and fixed presets.
// Power in watts (one of POWER_LEVELS). Values are starting points; tune them here.
#pragma once
#include <Arduino.h>
#include "ui_text.h"

// Defrost: time = weight x seconds per 100 g, at a low power so the outside
// doesn't cook before the inside thaws. A beep halfway asks to turn the food.
// (Times were set for 300 W and scaled up for 250 W, the original oven's step.)
struct DefrostType {
  Str name;
  uint16_t power;  // W
  uint16_t secPer100g;
};
constexpr DefrostType DEFROST_TYPES[] = {
  { Str::Meat, 250, 110 },
  { Str::Poultry, 250, 120 },
  { Str::Fish, 250, 90 },
  { Str::Bread, 250, 35 },
};
constexpr uint8_t NUM_DEFROST = sizeof(DEFROST_TYPES) / sizeof(DEFROST_TYPES[0]);
constexpr uint16_t DEFROST_MIN_G = 100, DEFROST_MAX_G = 2000, DEFROST_STEP_G = 100;

// Defrost time in seconds, rounded to 10 s.
inline uint32_t defrostSeconds(const DefrostType &t, uint16_t grams) {
  return ((uint32_t)grams * t.secPer100g / 100 + 5) / 10 * 10;
}

struct Preset {
  Str name;
  uint32_t seconds;
  uint16_t power;  // W
  bool rotate;
};
constexpr Preset PRESETS[] = {
  { Str::ReheatPlate, 150, 600, true },
  { Str::Beverage, 80, 750, true },
  { Str::Popcorn, 180, 750, true },
  { Str::SoftenButter, 30, 150, true },
  { Str::MeltChocolate, 120, 250, true },
};
constexpr uint8_t NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);
