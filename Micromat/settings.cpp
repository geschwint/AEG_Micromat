#include "settings.h"
#include "debug_log.h"
#include <Preferences.h>
#include "wifi_creds.h"
#include "config.h"

Settings settings;

static const char *const NAMESPACE = "micromat";
static const char *const SETTINGS_KEY = "s3";

// Favourites before "s3" held the power in percent (100/80/60/40/20/0).
struct FavouritePct {
  uint32_t seconds;
  uint8_t percent;
  bool rotate;
};

static void carryFavourites(Settings &s, const FavouritePct (&old)[NUM_FAVOURITES]) {
  for (uint8_t i = 0; i < NUM_FAVOURITES; i++) {
    s.favourites[i].seconds = old[i].seconds;
    s.favourites[i].power = nearestPowerW((int32_t)old[i].percent * MAX_POWER_W / 100);
    s.favourites[i].rotate = old[i].rotate;
  }
}

// Layout of "s2" (power in percent, no debug log). Kept in flash after the
// carry-over, so firmware rolled back to an older version still finds it.
static const char *const OLD_KEY_S2 = "s2";
struct SettingsV2 {
  uint8_t brightness, dimLevel, dimDelay, volume;
  bool keyClicks;
  Language language;
  bool autoTime;
  bool wifiOn;
  FavouritePct favourites[NUM_FAVOURITES];
  uint16_t defrostGrams;
};

static bool migrateV2(Preferences &prefs, Settings &s) {
  SettingsV2 v;
  if (prefs.getBytesLength(OLD_KEY_S2) != sizeof(v) || prefs.getBytes(OLD_KEY_S2, &v, sizeof(v)) != sizeof(v)) {
    return false;
  }
  s.brightness = v.brightness;
  s.dimLevel = v.dimLevel;
  s.dimDelay = v.dimDelay;
  s.volume = v.volume;
  s.keyClicks = v.keyClicks;
  s.language = v.language;
  s.autoTime = v.autoTime;
  s.wifiOn = v.wifiOn;
  carryFavourites(s, v.favourites);
  s.defrostGrams = v.defrostGrams;
  return true;
}

// Layout of "s1" (Wi-Fi network in the blob, password in plain text). Read
// once to carry the settings over; the network moves to wifi_creds.
static const char *const OLD_KEY_S1 = "s1";
struct SettingsV1 {
  uint8_t brightness, dimLevel, dimDelay, volume;
  bool keyClicks;
  Language language;
  bool autoTime;
  char wifiSsid[33];
  char wifiPass[65];
  FavouritePct favourites[NUM_FAVOURITES];
  uint16_t defrostGrams;
};

static bool migrateV1(Preferences &prefs, Settings &s) {
  SettingsV1 v;
  if (prefs.getBytesLength(OLD_KEY_S1) != sizeof(v) || prefs.getBytes(OLD_KEY_S1, &v, sizeof(v)) != sizeof(v)) {
    return false;
  }
  s.brightness = v.brightness;
  s.dimLevel = v.dimLevel;
  s.dimDelay = v.dimDelay;
  s.volume = v.volume;
  s.keyClicks = v.keyClicks;
  s.language = v.language;
  s.autoTime = v.autoTime;
  s.wifiOn = v.autoTime;  // Wi-Fi was only used for auto time
  carryFavourites(s, v.favourites);
  s.defrostGrams = v.defrostGrams;
  v.wifiSsid[sizeof(v.wifiSsid) - 1] = v.wifiPass[sizeof(v.wifiPass) - 1] = '\0';
  if (v.wifiSsid[0] && !credsSave(v.wifiSsid, v.wifiPass)) {
    Log.println("settings: old Wi-Fi network dropped (password can't be encrypted yet)");
  }
  memset(&v, 0, sizeof(v));
  return true;
}

// Guard against stale or damaged data: out-of-range indexes fall back to the default.
static void validate(Settings &s) {
  Settings d;
  if (s.brightness >= NUM_BRIGHTNESS) s.brightness = d.brightness;
  if (s.dimLevel >= NUM_DIM_LEVEL) s.dimLevel = d.dimLevel;
  if (s.dimDelay >= NUM_DIM_DELAY) s.dimDelay = d.dimDelay;
  if (s.volume >= NUM_VOLUME) s.volume = d.volume;
  if (s.language >= Language::Count) s.language = d.language;
  for (Favourite &f : s.favourites) f.power = nearestPowerW(f.power);
}

void loadSettings() {
  Preferences prefs;
  Settings s;
  if (!prefs.begin(NAMESPACE, false)) {
    Log.println("settings: NVS not available, using defaults");
    return;
  }
  if (prefs.getBytesLength(SETTINGS_KEY) == sizeof(Settings) && prefs.getBytes(SETTINGS_KEY, &s, sizeof(s)) == sizeof(s)) {
    validate(s);
    settings = s;
    Log.println("settings: loaded");
  } else if (migrateV2(prefs, s) || migrateV1(prefs, s)) {
    validate(s);
    settings = s;
    prefs.putBytes(SETTINGS_KEY, &settings, sizeof(settings));
    Log.println("settings: carried over from the old layout");
  } else {
    Log.println("settings: none saved, using defaults");
  }
  if (prefs.isKey(OLD_KEY_S1)) prefs.remove(OLD_KEY_S1);  // the old blob may hold a plain-text password
  prefs.end();
}

void saveSettings() {
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, false)) {
    Log.println("settings: SAVE FAILED");
    return;
  }
  prefs.putBytes(SETTINGS_KEY, &settings, sizeof(settings));
  prefs.end();
  Log.printf("[%7lu] settings saved\n", millis());
}
