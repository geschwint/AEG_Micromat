#include "eggs.h"
#include <Preferences.h>
#include "debug_log.h"

EggSettings eggs;

static const char *const NAMESPACE = "eggs";
static const char *const KEY_MELODY = "melody";  // one key per egg: adding eggs needs no migration

void loadEggs() {
  Preferences p;
  if (!p.begin(NAMESPACE, true)) return;  // never saved yet: defaults
  eggs.melody = p.getBool(KEY_MELODY, false);
  p.end();
  if (eggs.melody) Log.println("eggs: end melody on");
}

void saveEggs() {
  Preferences p;
  if (!p.begin(NAMESPACE, false)) return;
  p.putBool(KEY_MELODY, eggs.melody);
  p.end();
}

void eggsSoundSettingChanged() {
  if (!eggs.melody) return;
  eggs.melody = false;
  saveEggs();
  Log.printf("[%7lu] eggs: end melody off (a sound setting was changed)\n", millis());
}
