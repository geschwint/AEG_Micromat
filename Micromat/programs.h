// Built-in cooking programs: defrost by food type + weight, and fixed presets.
// Values are starting points for an ~800 W oven; tune them here.
#pragma once
#include <Arduino.h>
#include "ui_text.h"

// Defrost: time = weight x seconds per 100 g, at a low power so the outside
// doesn't cook before the inside thaws. A beep halfway asks to turn the food.
struct DefrostType {
  Str name;
  uint8_t power;
  uint16_t secPer100g;
};
constexpr DefrostType DEFROST_TYPES[] = {
  { Str::Meat, 40, 90 },
  { Str::Poultry, 40, 100 },
  { Str::Fish, 40, 75 },
  { Str::Bread, 40, 30 },
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
  uint8_t power;
  bool rotate;
};
constexpr Preset PRESETS[] = {
  { Str::ReheatPlate, 150, 80, true },
  { Str::Beverage, 80, 100, true },
  { Str::Popcorn, 180, 100, true },
  { Str::SoftenButter, 30, 20, true },
  { Str::MeltChocolate, 120, 40, true },
};
constexpr uint8_t NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);
