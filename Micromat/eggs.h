// Easter eggs. Hidden way in: with the door open, hold Rotate for at least
// EGG_HOLD_MIN_MS and release it before EGG_HOLD_MAX_MS (held longer: nothing
// happens). That opens the easter-egg menu (App, MenuId::Eggs) and "egg mode";
// closing the door in egg mode restarts the board.
//
// Two kinds of egg:
// - Lasting: a choice kept in flash here, so it survives the restart. Kept in
//   its own NVS namespace, not in the settings blob, so adding eggs never
//   changes the settings layout. An egg that changes something the normal
//   settings also control is turned off when that setting is changed (the
//   end melody: by saving any sound setting).
// - Momentary: runs only while in egg mode (e.g. the bouncing authors) and is
//   forgotten at the restart.
#pragma once
#include <Arduino.h>

struct EggSettings {
  bool melody = false;  // end-of-cooking signal plays a melody instead of three beeps
};

extern EggSettings eggs;
void loadEggs();
void saveEggs();
void eggsSoundSettingChanged();  // a standard sound setting was saved: the melody egg is turned off
