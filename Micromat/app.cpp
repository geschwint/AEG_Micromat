#include "app.h"
#include "debug_log.h"
#include "config.h"
#include "programs.h"
#include "settings.h"
#include "touch_cst816.h"
#include "about.h"
#include "fw_update.h"
#include <esp_arduino_version.h>
#include "wifi_creds.h"
#include "wifi_setup.h"
#include <lvgl.h>  // LV_SYMBOL_OK only

static constexpr uint32_t TURN_TOAST_MS = 8000;

// --- Menu structure --------------------------------------------------------------
// Rows of the fixed menus; each submenu's parent row index must match these.
static const Str ROOT_ROWS[] = { Str::Defrost, Str::Programs, Str::Settings, Str::About };
// Menu -> About: read-only label/value rows (values in aboutValue()).
static const Str ABOUT_ROWS[] = {
  Str::AboutFirmware, Str::AboutDate,   Str::AboutBuild,   Str::AboutOven,      Str::AboutOvenModel,
  Str::AboutType,     Str::AboutENr,    Str::AboutSerial,  Str::AboutBoard,     Str::AboutModel,
  Str::AboutChip,     Str::AboutCpu,    Str::AboutFlash,   Str::AboutPsram,     Str::AboutDisplay,
  Str::AboutTouch,    Str::AboutClockChip, Str::AboutRelays, Str::AboutMagnetron, Str::AboutLvgl,
  Str::AboutCore,
};
static const Str SETTINGS_ROWS[] = { Str::Clock, Str::Brightness, Str::DimLevel, Str::DimDelay, Str::Sound,
                                     Str::Language, Str::Wifi, Str::DebugLog, Str::FirmwareUpdate };
static const Str CLOCK_ROWS[] = { Str::SetTime, Str::AutoTime };
static const Str SOUND_ROWS[] = { Str::KeyClicks, Str::Volume };
static const Str WIFI_ROWS[] = { Str::Wifi, Str::Network, Str::ForgetNetwork };
static const Str DEBUG_LOG_ROWS[] = { Str::Logging, Str::DownloadLogs };
template <size_t N> static constexpr uint8_t count(const Str (&)[N]) { return N; }

struct MenuInfo {
  Str title;
  MenuId parent;
  uint8_t rowInParent;
};
// Indexed by MenuId. Root's parent is unused (back from Root closes the menu).
static const MenuInfo MENUS[] = {
  { Str::Menu, MenuId::Root, 0 },                                  // Root
  { Str::Defrost, MenuId::Root, 0 },                               // Defrost
  { Str::Programs, MenuId::Root, 1 },                              // Programs
  { Str::SaveFavourite, MenuId::Programs, NUM_PRESETS + NUM_FAVOURITES },  // SaveFavourite
  { Str::Settings, MenuId::Root, 2 },                              // Settings
  { Str::Clock, MenuId::Settings, 0 },                             // Clock
  { Str::Sound, MenuId::Settings, 4 },                             // Sound
  { Str::Wifi, MenuId::Settings, 6 },                              // Wifi
  { Str::About, MenuId::Root, 3 },                                 // About
  { Str::DebugLog, MenuId::Settings, 7 },                          // DebugLog
  { Str::Brightness, MenuId::Settings, 1 },                        // Brightness
  { Str::DimLevel, MenuId::Settings, 2 },                          // DimLevel
  { Str::DimDelay, MenuId::Settings, 3 },                          // DimDelay
  { Str::KeyClicks, MenuId::Sound, 0 },                            // KeyClicks
  { Str::Volume, MenuId::Sound, 1 },                               // Volume
  { Str::Language, MenuId::Settings, 5 },                          // Language
  { Str::AutoTime, MenuId::Clock, 1 },                             // AutoTime
  { Str::Wifi, MenuId::Wifi, 0 },                                  // WifiOn
  { Str::Logging, MenuId::DebugLog, 0 },                           // DebugLogOn
};
static_assert(sizeof(MENUS) / sizeof(MENUS[0]) == (size_t)MenuId::DebugLogOn + 1, "MENUS must match MenuId");

static bool isChoice(MenuId id) { return id >= MenuId::Brightness; }

// "2:30", or "1:05:00" from an hour.
static void formatShort(char *buf, size_t len, uint32_t s) {
  if (s >= 3600) snprintf(buf, len, "%lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  else snprintf(buf, len, "%lu:%02lu", s / 60, s % 60);
}

// "2:30  600 W" (or "2:30  Timer" at 0 W).
static void formatSummary(char *buf, size_t len, uint32_t seconds, uint16_t power) {
  char t[12];
  formatShort(t, sizeof(t), seconds);
  if (power == 0) snprintf(buf, len, "%s  %s", t, tr(Str::Timer));
  else snprintf(buf, len, "%s  %u W", t, power);
}

// --- Choice lists --------------------------------------------------------------------
uint8_t App::choiceCount(MenuId id) const {
  switch (id) {
    case MenuId::Brightness: return NUM_BRIGHTNESS;
    case MenuId::DimLevel:   return NUM_DIM_LEVEL;
    case MenuId::DimDelay:   return NUM_DIM_DELAY;
    case MenuId::Volume:     return NUM_VOLUME;
    case MenuId::Language:   return (uint8_t)Language::Count;
    default:                 return 2;  // On / Off
  }
}

uint8_t App::choiceCurrent(MenuId id) const {
  switch (id) {
    case MenuId::Brightness: return settings.brightness;
    case MenuId::DimLevel:   return settings.dimLevel;
    case MenuId::DimDelay:   return settings.dimDelay;
    case MenuId::KeyClicks:  return settings.keyClicks ? 0 : 1;
    case MenuId::Volume:     return settings.volume;
    case MenuId::Language:   return (uint8_t)settings.language;
    case MenuId::AutoTime:   return settings.autoTime ? 0 : 1;
    case MenuId::WifiOn:     return settings.wifiOn ? 0 : 1;
    case MenuId::DebugLogOn: return settings.debugLog ? 0 : 1;
    default:                 return 0;
  }
}

void App::choiceLabel(MenuId id, uint8_t i, char *buf, size_t len) const {
  switch (id) {
    case MenuId::Brightness: snprintf(buf, len, "%u %%", BRIGHTNESS_PCT[i]); break;
    case MenuId::DimLevel:
      if (DIM_LEVEL_PCT[i] == 0) snprintf(buf, len, "%s", tr(Str::Off));
      else snprintf(buf, len, "%u %%", DIM_LEVEL_PCT[i]);
      break;
    case MenuId::DimDelay:
      if (DIM_DELAY_S[i] == 0) snprintf(buf, len, "%s", tr(Str::Never));
      else if (DIM_DELAY_S[i] < 60) snprintf(buf, len, "%u s", DIM_DELAY_S[i]);
      else snprintf(buf, len, "%u min", DIM_DELAY_S[i] / 60);
      break;
    case MenuId::Volume: {
      static const Str NAMES[] = { Str::Low, Str::Medium, Str::High };
      snprintf(buf, len, "%s", tr(NAMES[i]));
      break;
    }
    case MenuId::Language: snprintf(buf, len, "%s", languageName(i)); break;
    default: snprintf(buf, len, "%s", tr(i == 0 ? Str::On : Str::Off)); break;  // KeyClicks, AutoTime, WifiOn, DebugLogOn
  }
}

void App::choiceApply(MenuId id, uint8_t i) {
  switch (id) {
    case MenuId::Brightness: settings.brightness = i; break;
    case MenuId::DimLevel:   settings.dimLevel = i; break;
    case MenuId::DimDelay:   settings.dimDelay = i; break;
    case MenuId::KeyClicks:  settings.keyClicks = i == 0; break;
    case MenuId::Volume:     settings.volume = i; break;
    case MenuId::Language:   settings.language = (Language)i; break;
    case MenuId::AutoTime:
      settings.autoTime = i == 0;
      if (settings.autoTime) {
        if (settings.wifiOn && credsHaveNetwork()) wifi_->syncNow();
        else showToast(Str::NoWifi, 4000);
      }
      break;
    case MenuId::WifiOn:
      settings.wifiOn = i == 0;
      if (!settings.wifiOn) wifi_->cancel();
      else if (credsHaveNetwork()) wifi_->syncNow();
      break;
    case MenuId::DebugLogOn:
      settings.debugLog = i == 0;
      if (!Log.setEnabled(settings.debugLog)) {  // stays on: logging starts when a card is there at start-up
        buzzer_->error();
        showToast(Str::NoSdCard, 4000);
      }
      break;
    default: break;
  }
  saveSettings();
}

// --- About -----------------------------------------------------------------------------
static void aboutValue(Str row, char *buf, size_t len) {
  switch (row) {
    case Str::AboutFirmware:  snprintf(buf, len, "%s", FIRMWARE_VERSION); break;
    case Str::AboutDate:      snprintf(buf, len, "%s", FIRMWARE_DATE); break;
    case Str::AboutBuild:
      snprintf(buf, len, "%s %s", FirmwareUpdate::version(), FirmwareUpdate::runningSlot());
      break;
    case Str::AboutOven:      snprintf(buf, len, "%s", OVEN_MAKER); break;
    case Str::AboutOvenModel: snprintf(buf, len, "%s", OVEN_NAME); break;
    case Str::AboutType:      snprintf(buf, len, "%s", OVEN_TYPE); break;
    case Str::AboutENr:       snprintf(buf, len, "%s", OVEN_E_NR); break;
    case Str::AboutSerial:    snprintf(buf, len, "%s", OVEN_SERIAL); break;
    case Str::AboutBoard:     snprintf(buf, len, "%s", BOARD_MAKER); break;
    case Str::AboutModel:     snprintf(buf, len, "%s", BOARD_MODEL); break;
    case Str::AboutChip: {
      uint16_t rev = ESP.getChipRevision();  // major * 100 + minor
      snprintf(buf, len, "%s v%u.%u", ESP.getChipModel(), rev / 100, rev % 100);
      break;
    }
    case Str::AboutCpu:
      snprintf(buf, len, "%u \xC3\x97 %lu MHz", ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
      break;
    case Str::AboutFlash:     snprintf(buf, len, "%lu MB", (unsigned long)(ESP.getFlashChipSize() >> 20)); break;
    case Str::AboutPsram:     snprintf(buf, len, "%lu MB", (unsigned long)(ESP.getPsramSize() >> 20)); break;
    case Str::AboutDisplay:   snprintf(buf, len, "%s", DISPLAY_INFO); break;
    case Str::AboutTouch:     snprintf(buf, len, "%s", TOUCH_CHIP); break;
    case Str::AboutClockChip: snprintf(buf, len, "%s", RTC_CHIP); break;
    case Str::AboutRelays:    snprintf(buf, len, "%s", RELAY_BOARD); break;
    case Str::AboutMagnetron: snprintf(buf, len, "%s", tr(Str::OriginalRelay)); break;
    case Str::AboutLvgl:
      snprintf(buf, len, "%d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
      break;
    case Str::AboutCore:
      snprintf(buf, len, "%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
      break;
    default: buf[0] = '\0'; break;
  }
}

// --- Menu view -------------------------------------------------------------------------
const char *App::menuTitle() const { return tr(MENUS[(size_t)menuId_].title); }

uint8_t App::menuCount() const {
  switch (menuId_) {
    case MenuId::Root:          return count(ROOT_ROWS);
    case MenuId::Defrost:       return NUM_DEFROST;
    case MenuId::Programs:      return NUM_PRESETS + NUM_FAVOURITES + 1;  // + "Save as favourite"
    case MenuId::SaveFavourite: return NUM_FAVOURITES;
    case MenuId::Settings:      return count(SETTINGS_ROWS);
    case MenuId::Clock:         return count(CLOCK_ROWS);
    case MenuId::Sound:         return count(SOUND_ROWS);
    case MenuId::Wifi:          return count(WIFI_ROWS);
    case MenuId::DebugLog:      return count(DEBUG_LOG_ROWS);
    case MenuId::About:         return count(ABOUT_ROWS);
    default:                    return choiceCount(menuId_);
  }
}

void App::menuRow(uint8_t i, char *label, size_t labelLen, char *value, size_t valueLen) const {
  label[0] = value[0] = '\0';
  auto favourite = [&](uint8_t f) {
    snprintf(label, labelLen, "%s %u", tr(Str::Favourite), f + 1);
    const Favourite &fav = settings.favourites[f];
    if (fav.seconds) formatSummary(value, valueLen, fav.seconds, fav.power);
    else snprintf(value, valueLen, "%s", tr(Str::Empty));
  };
  switch (menuId_) {
    case MenuId::Root: snprintf(label, labelLen, "%s", tr(ROOT_ROWS[i])); break;
    case MenuId::Defrost: snprintf(label, labelLen, "%s", tr(DEFROST_TYPES[i].name)); break;
    case MenuId::Programs:
      if (i < NUM_PRESETS) {
        snprintf(label, labelLen, "%s", tr(PRESETS[i].name));
        formatSummary(value, valueLen, PRESETS[i].seconds, PRESETS[i].power);
      } else if (i < NUM_PRESETS + NUM_FAVOURITES) {
        favourite(i - NUM_PRESETS);
      } else {
        snprintf(label, labelLen, "%s", tr(Str::SaveFavourite));
      }
      break;
    case MenuId::SaveFavourite: favourite(i); break;
    case MenuId::Settings: {
      Str row = SETTINGS_ROWS[i];
      snprintf(label, labelLen, "%s", tr(row));
      MenuId sub = row == Str::Brightness ? MenuId::Brightness
                 : row == Str::DimLevel   ? MenuId::DimLevel
                 : row == Str::DimDelay   ? MenuId::DimDelay
                 : row == Str::Language   ? MenuId::Language
                 : row == Str::Wifi       ? MenuId::WifiOn
                 : row == Str::DebugLog   ? MenuId::DebugLogOn
                                          : MenuId::Root;
      if (sub != MenuId::Root) choiceLabel(sub, choiceCurrent(sub), value, valueLen);
      break;
    }
    case MenuId::Clock:
      snprintf(label, labelLen, "%s", tr(CLOCK_ROWS[i]));
      if (i == 0 && clock_->valid()) strftime(value, valueLen, "%H:%M", &clock_->local());
      else if (i == 1) choiceLabel(MenuId::AutoTime, choiceCurrent(MenuId::AutoTime), value, valueLen);
      break;
    case MenuId::Sound: {
      snprintf(label, labelLen, "%s", tr(SOUND_ROWS[i]));
      MenuId sub = i == 0 ? MenuId::KeyClicks : MenuId::Volume;
      choiceLabel(sub, choiceCurrent(sub), value, valueLen);
      break;
    }
    case MenuId::Wifi:
      snprintf(label, labelLen, "%s", tr(WIFI_ROWS[i]));
      if (i == 0) choiceLabel(MenuId::WifiOn, choiceCurrent(MenuId::WifiOn), value, valueLen);
      else if (i == 1) snprintf(value, valueLen, "%s", credsHaveNetwork() ? credsSsid() : tr(Str::NotSet));
      break;
    case MenuId::DebugLog:
      snprintf(label, labelLen, "%s", tr(DEBUG_LOG_ROWS[i]));
      if (i == 0) choiceLabel(MenuId::DebugLogOn, choiceCurrent(MenuId::DebugLogOn), value, valueLen);
      break;
    case MenuId::About:
      snprintf(label, labelLen, "%s", tr(ABOUT_ROWS[i]));
      aboutValue(ABOUT_ROWS[i], value, valueLen);
      break;
    default:  // choice list: the current value gets a check mark
      choiceLabel(menuId_, i, label, labelLen);
      if (i == choiceCurrent(menuId_)) snprintf(value, valueLen, "%s", LV_SYMBOL_OK);
      break;
  }
}

uint8_t App::menuFirstVisible() const {
  uint8_t n = menuCount();
  if (n <= MENU_ROWS) return 0;
  return constrain((int)menuIndex_ - MENU_ROWS / 2, 0, n - MENU_ROWS);
}

// --- Editor view -----------------------------------------------------------------------
const char *App::editTitle() const {
  return editId_ == EditId::DefrostWeight ? tr(DEFROST_TYPES[defrostType_].name) : tr(Str::SetTime);
}

void App::editValue(char *a, char *b, size_t len) const {
  if (editId_ == EditId::DefrostWeight) {
    snprintf(a, len, "%u g", grams_);
    b[0] = '\0';
  } else {
    snprintf(a, len, "%02u", hour_);
    snprintf(b, len, "%02u", minute_);
  }
}

void App::editInfo(char *buf, size_t len) const {
  if (editId_ == EditId::DefrostWeight) {
    const DefrostType &t = DEFROST_TYPES[defrostType_];
    formatSummary(buf, len, defrostSeconds(t, grams_), t.power);
  } else {
    buf[0] = '\0';
  }
}

// --- Basics -----------------------------------------------------------------------------
void App::begin(Oven &oven, Door &door, Buzzer &buzzer, Clock &clock, WifiTime &wifi) {
  oven_ = &oven;
  door_ = &door;
  buzzer_ = &buzzer;
  clock_ = &clock;
  wifi_ = &wifi;
  lastDoorClosed_ = door.closed();
  lastState_ = oven.state();
  splashStart_ = millis();
  use();
}

void App::use() {
  lastUse_ = millis();
  dimRef_ = lastUse_;
  if (dimmed_) Log.printf("[%7lu] display awake\n", lastUse_);
  dimmed_ = false;
}

void App::showToast(Str s, uint32_t ms) {
  toast_ = s;
  toastUntil_ = millis() + (ms ? ms : TOAST_MS);
}

const char *App::toast() const {
  return (int32_t)(toastUntil_ - millis()) > 0 ? tr(toast_) : nullptr;
}

uint8_t App::backlightPercent() const {
  if (dimmed_) return DIM_LEVEL_PCT[settings.dimLevel];
  if (screen_ == Screen::Menu && menuId_ == MenuId::Brightness) return BRIGHTNESS_PCT[menuIndex_];
  if (screen_ == Screen::Menu && menuId_ == MenuId::DimLevel) return max<uint8_t>(DIM_LEVEL_PCT[menuIndex_], 3);
  return BRIGHTNESS_PCT[settings.brightness];
}

bool App::powerView() const {
  return powerViewOn_;  // cleared in update() after POWER_VIEW_MS
}

bool App::programText(char *buf, size_t len) const {
  switch (program_) {
    case Program::Defrost:
      snprintf(buf, len, "%s: %s %u g", tr(Str::Defrost), tr(DEFROST_TYPES[programIndex_].name), programGrams_);
      return true;
    case Program::Preset: snprintf(buf, len, "%s", tr(PRESETS[programIndex_].name)); return true;
    case Program::Favourite: snprintf(buf, len, "%s %u", tr(Str::Favourite), programIndex_ + 1); return true;
    default: return false;
  }
}

// Each encoder step moves to the next/previous multiple of a step that grows
// with the set time (chosen per step, so a fast spin crosses bands correctly).
static uint32_t encoderStepSeconds(uint32_t t) {
  if (t < 2 * 60) return 5;
  if (t < 5 * 60) return 10;
  if (t < 10 * 60) return 20;
  if (t < 20 * 60) return 30;
  if (t < 3600) return 60;
  return 300;
}

// Acceleration: the smoothed turning speed gives the steps per click (1 when
// turning slowly, up to the last ENC_ACCEL_MULT when spinning). A pause or a
// change of direction starts over at 1.
void App::adjustTime(int32_t clicks, uint32_t now) {
  int8_t dir = clicks > 0 ? 1 : -1;
  uint32_t dt = now - encLastMs_;
  if (dir != encLastDir_ || dt >= ENC_ACCEL_RESET_MS) {
    encSpeed_ = 0;
  } else {
    float inst = abs(clicks) * 1000.0f / max<uint32_t>(dt, INPUT_POLL_MS);
    encSpeed_ = encSpeed_ == 0 ? inst : 0.6f * encSpeed_ + 0.4f * inst;
  }
  encLastMs_ = now;
  encLastDir_ = dir;
  uint8_t mult = 1;
  for (uint8_t i = 0; i < sizeof(ENC_ACCEL_CPS) / sizeof(ENC_ACCEL_CPS[0]); i++) {
    if (encSpeed_ >= ENC_ACCEL_CPS[i]) mult = ENC_ACCEL_MULT[i];
  }
  int32_t steps = clicks * mult;

  uint32_t start = oven_->remainingSeconds();
  uint32_t t = start;
  for (int32_t i = 0; i < abs(steps); i++) {
    if (clicks > 0) {
      uint32_t step = encoderStepSeconds(t);
      t = t - t % step + step;
    } else if (t > 0) {
      uint32_t step = encoderStepSeconds(t - 1);
      t = (t % step) ? t - t % step : t - step;
    }
  }
  if (t > MAX_COOK_SECONDS) t = MAX_COOK_SECONDS;
  Log.printf("[%7lu] ENC %+ld click(s) x%u (%.0f/s): %lu -> %lu s\n", now, (long)clicks, mult, encSpeed_,
                (unsigned long)start, (unsigned long)t);
  if (t != start) oven_->addSeconds((int32_t)t - (int32_t)start);
}

// --- Programs --------------------------------------------------------------------------
// Loads time, power and rotate; a paused cook is cleared first. Starting is
// left to the user (push / Quick start), so the door check and start beep work
// as usual.
void App::loadProgram(Program kind, uint8_t index, uint32_t seconds, uint16_t power, bool rotate) {
  if (oven_->state() == OvenState::Paused) oven_->stop();
  oven_->setSeconds(seconds);
  oven_->setPower(power);
  oven_->setRotate(rotate);
  program_ = kind;
  programIndex_ = index;
  programGrams_ = grams_;
  programSeconds_ = seconds;
  turnBeeped_ = false;
  Log.printf("[%7lu] program loaded: %lu s, %u W\n", millis(), (unsigned long)seconds, power);
  screen_ = Screen::Main;
}

// --- Menu ------------------------------------------------------------------------------
void App::openMenu(MenuId id, uint8_t index) {
  screen_ = Screen::Menu;
  menuId_ = id;
  menuIndex_ = index;
  buzzer_->click();
}

void App::closeMenu() {
  if (screen_ == Screen::Menu || screen_ == Screen::Edit) screen_ = Screen::Main;
}

void App::menuBack() {
  if (menuId_ == MenuId::Root) {
    buzzer_->click();
    closeMenu();
    return;
  }
  const MenuInfo &m = MENUS[(size_t)menuId_];
  openMenu(m.parent, m.rowInParent);
}

void App::menuSelect() {
  uint8_t i = menuIndex_;
  if (isChoice(menuId_)) {
    choiceApply(menuId_, i);
    menuBack();
    return;
  }
  switch (menuId_) {
    case MenuId::Root:
      openMenu(i == 0 ? MenuId::Defrost : i == 1 ? MenuId::Programs : i == 2 ? MenuId::Settings : MenuId::About);
      break;
    case MenuId::Defrost:
      defrostType_ = i;
      grams_ = constrain(settings.defrostGrams, DEFROST_MIN_G, DEFROST_MAX_G);
      openEdit(EditId::DefrostWeight);
      break;
    case MenuId::Programs:
      if (i < NUM_PRESETS) {
        buzzer_->click();
        const Preset &p = PRESETS[i];
        loadProgram(Program::Preset, i, p.seconds, p.power, p.rotate);
      } else if (i < NUM_PRESETS + NUM_FAVOURITES) {
        const Favourite &f = settings.favourites[i - NUM_PRESETS];
        if (!f.seconds) {
          buzzer_->error();
          showToast(Str::Empty);
        } else {
          buzzer_->click();
          loadProgram(Program::Favourite, i - NUM_PRESETS, f.seconds, f.power, f.rotate);
        }
      } else if (oven_->remainingSeconds() == 0) {
        buzzer_->error();
        showToast(Str::SetTimeFirst);
      } else {
        openMenu(MenuId::SaveFavourite);
      }
      break;
    case MenuId::SaveFavourite: {
      Favourite &f = settings.favourites[i];
      f.seconds = oven_->remainingSeconds();
      f.power = oven_->power();
      f.rotate = oven_->rotate();
      saveSettings();
      buzzer_->click();
      showToast(Str::Saved);
      openMenu(MenuId::Programs, NUM_PRESETS + i);
      break;
    }
    case MenuId::Settings: {
      Str row = SETTINGS_ROWS[i];
      if (row == Str::FirmwareUpdate) {
        startUpdate();
        break;
      }
      MenuId sub = row == Str::Clock        ? MenuId::Clock
                 : row == Str::Brightness   ? MenuId::Brightness
                 : row == Str::DimLevel     ? MenuId::DimLevel
                 : row == Str::DimDelay     ? MenuId::DimDelay
                 : row == Str::Sound        ? MenuId::Sound
                 : row == Str::Wifi         ? MenuId::Wifi
                 : row == Str::DebugLog     ? MenuId::DebugLog
                                            : MenuId::Language;
      openMenu(sub, isChoice(sub) ? choiceCurrent(sub) : 0);
      break;
    }
    case MenuId::Clock:
      if (i == 0) openEdit(EditId::ClockTime);
      else openMenu(MenuId::AutoTime, choiceCurrent(MenuId::AutoTime));
      break;
    case MenuId::Sound: {
      MenuId sub = i == 0 ? MenuId::KeyClicks : MenuId::Volume;
      openMenu(sub, choiceCurrent(sub));
      break;
    }
    case MenuId::DebugLog:
      if (i == 0) openMenu(MenuId::DebugLogOn, choiceCurrent(MenuId::DebugLogOn));
      else startUpdate(true);
      break;
    case MenuId::Wifi:
      if (i == 0) {
        openMenu(MenuId::WifiOn, choiceCurrent(MenuId::WifiOn));
      } else if (i == 1) {
        if (credsKeyReady()) {
          startWifiSetup();
        } else {
          buzzer_->error();
          showToast(Str::NoKey, 4000);
        }
      } else if (credsHaveNetwork()) {
        wifi_->cancel();
        credsForget();
        buzzer_->click();
        showToast(Str::Forgotten);
      } else {
        buzzer_->error();
        showToast(Str::NotSet);
      }
      break;
    default: break;
  }
}

// --- Wi-Fi setup screen ----------------------------------------------------------------------
void App::startWifiSetup() {
  wifi_->cancel();  // the setup needs the radio
  wifiSetup.start();
  screen_ = Screen::WifiSetup;
  buzzer_->click();
}

// Back to Settings -> Wi-Fi; a newly saved network gets the clock synced.
void App::leaveWifiSetup() {
  bool saved = wifiSetup.state() == WifiSetup::State::Success;
  wifiSetup.stop();
  openMenu(MenuId::Wifi, 1);
  if (saved) wifi_->syncNow();
}

// --- Firmware update screen ------------------------------------------------------------------
// Only from Idle/Done (a paused cook must be cleared first). While the screen
// is open nothing starts cooking: Quick start and push are not passed on.
void App::startUpdate(bool logs) {
  OvenState st = oven_->state();
  if (st == OvenState::Cooking || st == OvenState::Paused) {
    buzzer_->error();
    showToast(Str::NotWhileCooking);
    return;
  }
  wifi_->cancel();  // the update needs the radio
  firmwareUpdate.start(logs ? FirmwareUpdate::Mode::Logs : FirmwareUpdate::Mode::Firmware);
  screen_ = Screen::Update;
  buzzer_->click();
}

void App::leaveUpdate() {
  firmwareUpdate.stop();
  if (firmwareUpdate.mode() == FirmwareUpdate::Mode::Logs) openMenu(MenuId::DebugLog, 1);
  else openMenu(MenuId::Settings, count(SETTINGS_ROWS) - 1);
}

void App::updateInput(InputEvent e) {
  if (firmwareUpdate.state() == FirmwareUpdate::State::Done) return;  // restarting
  switch (e) {
    case InputEvent::Stop:
    case InputEvent::Program:
    case InputEvent::EncoderPush: leaveUpdate(); break;
    case InputEvent::QuickStart:  buzzer_->error(); break;
    default: break;
  }
}

void App::wifiSetupInput(InputEvent e) {
  switch (e) {
    case InputEvent::Stop:
    case InputEvent::Program:
    case InputEvent::EncoderPush: leaveWifiSetup(); break;
    case InputEvent::QuickStart:
      leaveWifiSetup();
      closeMenu();
      oven_->quickStart();
      break;
    case InputEvent::Rotate: oven_->toggleRotate(); break;
    default: break;
  }
}

// --- Editors -------------------------------------------------------------------------------
void App::openEdit(EditId id) {
  editId_ = id;
  editField_ = 0;
  if (id == EditId::ClockTime) {
    const struct tm &t = clock_->local();
    hour_ = clock_->valid() ? t.tm_hour : 12;
    minute_ = clock_->valid() ? t.tm_min : 0;
  }
  screen_ = Screen::Edit;
  buzzer_->click();
}

void App::editBack() {
  if (editId_ == EditId::ClockTime && editField_ > 0) {
    editField_--;
    buzzer_->click();
  } else if (editId_ == EditId::ClockTime) {
    openMenu(MenuId::Clock, 0);
  } else {
    openMenu(MenuId::Defrost, defrostType_);
  }
}

void App::editInput(InputEvent e, int32_t value) {
  switch (e) {
    case InputEvent::EncoderTurn:
      if (editId_ == EditId::DefrostWeight) {
        grams_ = constrain((int32_t)grams_ + value * DEFROST_STEP_G, (int32_t)DEFROST_MIN_G, (int32_t)DEFROST_MAX_G);
      } else if (editField_ == 0) {
        hour_ = ((int32_t)hour_ + value % 24 + 24) % 24;
      } else {
        minute_ = ((int32_t)minute_ + value % 60 + 60) % 60;
      }
      buzzer_->click();
      break;
    case InputEvent::EncoderPush:
      if (editId_ == EditId::DefrostWeight) {
        const DefrostType &t = DEFROST_TYPES[defrostType_];
        settings.defrostGrams = grams_;
        saveSettings();
        buzzer_->click();
        loadProgram(Program::Defrost, defrostType_, defrostSeconds(t, grams_), t.power, true);
      } else if (editField_ == 0) {
        editField_ = 1;
        buzzer_->click();
      } else {
        bool ok = clock_->setLocalTime(hour_, minute_);
        Log.printf("[%7lu] clock set to %02u:%02u %s\n", millis(), hour_, minute_, ok ? "OK" : "FAILED");
        if (ok) {
          buzzer_->click();
          showToast(Str::Saved);
        } else {
          buzzer_->error();
        }
        openMenu(MenuId::Clock, 0);
      }
      break;
    case InputEvent::Stop:
    case InputEvent::Program:    editBack(); break;
    case InputEvent::QuickStart:
      closeMenu();
      oven_->quickStart();
      break;
    case InputEvent::Rotate:     oven_->toggleRotate(); break;
    default: break;
  }
}

// --- Input handling ----------------------------------------------------------------------
void App::onInput(InputEvent e, int32_t value) {
  if (e == InputEvent::PotPower) {  // the pot is a direct control on every screen
    // Its first report comes at start-up: that mustn't end the splash or show the watts.
    if (potSeen_ && !splash_) {
      use();
      powerViewAt_ = millis();
      powerViewOn_ = true;
    }
    potSeen_ = true;
    potWatts_ = value;
    oven_->setPower(value);
    return;
  }
  powerViewOn_ = false;  // any other input: the big digits show the time again at once
  // Like waking from dimmed: the encoder only ends the splash; buttons end it and act.
  bool wakeOnly = dimmed_ || splash_;
  splash_ = false;
  use();
  if (wakeOnly && (e == InputEvent::EncoderTurn || e == InputEvent::EncoderPush)) return;
  if (e == InputEvent::ProgramHold) {
    Log.printf("[%7lu] service screen\n", millis());
    buzzer_->click();
    if (screen_ == Screen::WifiSetup) wifiSetup.stop();
    if (screen_ == Screen::Update) {
      if (firmwareUpdate.state() == FirmwareUpdate::State::Done) return;  // restarting
      firmwareUpdate.stop();
    }
    screen_ = Screen::Service;
    return;
  }

  switch (screen_) {
    case Screen::Main:    mainInput(e, value); break;
    case Screen::Menu:    menuInput(e, value); break;
    case Screen::Edit:    editInput(e, value); break;
    case Screen::Service: serviceInput(e); break;
    case Screen::WifiSetup: wifiSetupInput(e); break;
    case Screen::Update:    updateInput(e); break;
  }
}

void App::mainInput(InputEvent e, int32_t value) {
  switch (e) {
    case InputEvent::EncoderTurn: adjustTime(value, millis()); break;
    case InputEvent::EncoderPush: oven_->start(); break;
    case InputEvent::Stop:        oven_->stop(); break;
    case InputEvent::QuickStart:  oven_->quickStart(); break;
    case InputEvent::Rotate:      oven_->toggleRotate(); break;
    case InputEvent::Program:
      if (oven_->state() == OvenState::Cooking) buzzer_->error();  // no menu while cooking
      else openMenu(MenuId::Root);
      break;
    default: break;
  }
}

void App::menuInput(InputEvent e, int32_t value) {
  switch (e) {
    case InputEvent::EncoderTurn: {
      int32_t i = constrain((int32_t)menuIndex_ + value, 0, (int32_t)menuCount() - 1);
      if (i != menuIndex_) {
        menuIndex_ = i;
        buzzer_->click();
      }
      break;
    }
    case InputEvent::EncoderPush: menuSelect(); break;
    case InputEvent::Stop:
    case InputEvent::Program:     menuBack(); break;
    case InputEvent::QuickStart:
      closeMenu();
      oven_->quickStart();
      break;
    case InputEvent::Rotate:      oven_->toggleRotate(); break;
    default: break;
  }
}

void App::serviceInput(InputEvent e) {
  switch (e) {
    case InputEvent::Stop:
    case InputEvent::Program:
    case InputEvent::EncoderPush:
      buzzer_->click();
      screen_ = Screen::Main;
      break;
    case InputEvent::QuickStart: oven_->quickStart(); break;
    case InputEvent::Rotate:     oven_->toggleRotate(); break;
    default: break;
  }
}

// Touch (the display is already awake here). Menu: tap a row = select it, tap
// the title = back. Clock editor: tap the hours or minutes to edit that field;
// title = back. Service: tap toggles the simulated door.
void App::onTouch(int16_t x, int16_t y) {
  if (screen_ == Screen::Menu) {
    if (y < MENU_ROW_Y0) {
      menuBack();
      return;
    }
    if (y >= MENU_ROW_Y0 + MENU_ROWS * MENU_ROW_H) return;  // hint area
    uint8_t i = menuFirstVisible() + (y - MENU_ROW_Y0) / MENU_ROW_H;
    if (i >= menuCount()) return;
    Log.printf("[%7lu] touch menu row %u at %d,%d\n", millis(), i, x, y);
    menuIndex_ = i;
    menuSelect();
  } else if (screen_ == Screen::Edit) {
    if (y < MENU_ROW_Y0) editBack();
    else if (editId_ == EditId::ClockTime && editField_ != (x >= SCREEN_W / 2)) {
      editField_ = x >= SCREEN_W / 2;
      buzzer_->click();
    }
  } else if (screen_ == Screen::WifiSetup) {
    if (y < MENU_ROW_Y0) leaveWifiSetup();
  } else if (screen_ == Screen::Update) {
    if (y < MENU_ROW_Y0 && firmwareUpdate.state() != FirmwareUpdate::State::Done) leaveUpdate();
  } else if (screen_ == Screen::Service && door_->simulated()) {
    door_->simulate(!door_->closed());
  }
}

// --- Periodic ------------------------------------------------------------------------------
void App::update(uint32_t now) {
  if (now - lastTouchPoll_ >= TOUCH_POLL_MS) {
    lastTouchPoll_ = now;
    TouchPoint t;
    touchRead(t);
    if (t.pressed && !touchDown_) {
      touchDown_ = true;
      bool wakeOnly = dimmed_ || splash_;  // a touch only ends the splash / wakes
      powerViewOn_ = false;
      splash_ = false;
      use();
      if (!wakeOnly) onTouch(t.x, t.y);
    } else if (!t.pressed) {
      touchDown_ = false;
    }
  }

  // The splash ends after SPLASH_MS (or earlier on any input or change below).
  if (splash_ && now - splashStart_ >= SPLASH_MS) splash_ = false;
  if (powerViewOn_ && (int32_t)(now - powerViewAt_) >= (int32_t)POWER_VIEW_MS) powerViewOn_ = false;

  // Door and oven state changes count as use and bring the main screen back.
  if (door_->closed() != lastDoorClosed_) {
    lastDoorClosed_ = door_->closed();
    splash_ = false;
    use();
    closeMenu();
  }
  if (oven_->state() != lastState_) {
    lastState_ = oven_->state();
    splash_ = false;
    use();
    if (lastState_ == OvenState::Done) closeMenu();
  }

  // Program: "turn the food" halfway through a defrost; forget the program
  // once the cook is finished or cleared, and give power back to the pot
  // (quietly, so the end beeps aren't cut short).
  OvenState st = oven_->state();
  if (program_ == Program::Defrost && !turnBeeped_ && st == OvenState::Cooking &&
      oven_->remainingSeconds() <= programSeconds_ / 2) {
    turnBeeped_ = true;
    buzzer_->beep(2, 150, 150);
    showToast(Str::TurnFood, TURN_TOAST_MS);
    Log.printf("[%7lu] defrost halfway: turn the food\n", now);
  }
  if (program_ != Program::None &&
      (st == OvenState::Done || (st == OvenState::Idle && oven_->remainingSeconds() == 0))) {
    program_ = Program::None;
    if (oven_->power() != potWatts_) oven_->setPower(potWatts_, false);
  }

  uint32_t t = millis();
  if ((screen_ == Screen::Menu || screen_ == Screen::Edit) && t - lastUse_ > MENU_TIMEOUT_MS) {
    Log.printf("[%7lu] menu timeout\n", t);
    closeMenu();
  }
  // The setup screen stays while its network is open (it has its own timeout),
  // then shows the result until the usual menu timeout.
  if (screen_ == Screen::WifiSetup && !wifiSetup.active() && t - lastUse_ > MENU_TIMEOUT_MS) {
    leaveWifiSetup();
    closeMenu();
  }
  if (screen_ == Screen::Update && !firmwareUpdate.active() && t - lastUse_ > MENU_TIMEOUT_MS) {
    leaveUpdate();
    closeMenu();
  }

  // Dim only on the main screen in Idle/Done; otherwise keep restarting the countdown.
  uint32_t dimAfterMs = DIM_DELAY_S[settings.dimDelay] * 1000UL;
  bool mayDim = dimAfterMs > 0 && screen_ == Screen::Main && (st == OvenState::Idle || st == OvenState::Done);
  if (!mayDim) dimRef_ = t;
  if (mayDim && !dimmed_ && t - dimRef_ > dimAfterMs) {
    dimmed_ = true;
    Log.printf("[%7lu] display dimmed\n", t);
  }
}
