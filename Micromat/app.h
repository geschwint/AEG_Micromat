// Application controller: decides what each input event means on the current
// screen (main / menu / editor / service), and handles dimming, waking, the
// menu timeout, touch, and the program (defrost/preset/favourite) loaded.
//
// Dimming: after the Settings "dim after" time without use, in Idle/Done on
// the main screen. "Use" = any input event, a touch, the door opening/closing,
// or an oven state change. While dimmed, encoder turn/push and touch only wake
// the display; panel buttons and the pot wake it *and* act.
//
// Menus are lists: encoder browses, push or a tap on a row selects, Stop or a
// tap on the title goes back. Choice lists (e.g. brightness) mark the current
// value with a check; picking one saves it and goes back. Editors (defrost
// weight, clock time) take a value with the encoder; push confirms.
// A program from the menu is loaded (time, power, rotate) and the main screen
// returns; push or Quick start then starts it, as for a manual setting.
// Settings -> Wi-Fi -> Network opens the Wi-Fi setup screen (wifi_setup.h);
// Settings -> Firmware update the update screen (fw_update.h; only when idle,
// and nothing can start cooking while it's open); Settings -> Debug log ->
// Download the same screen for the log files on the SD card.
#pragma once
#include <Arduino.h>
#include "inputs.h"
#include "oven.h"
#include "door.h"
#include "buzzer.h"
#include "clock.h"
#include "wifi_time.h"
#include "ui_text.h"
#include "config.h"

enum class Screen : uint8_t { Main, Menu, Edit, Service, WifiSetup, Update };
// Choice lists come last (isChoice() relies on that order).
enum class MenuId : uint8_t {
  Root, Defrost, Programs, SaveFavourite, Settings, Clock, Sound, Wifi, About, DebugLog,
  Brightness, DimLevel, DimDelay, KeyClicks, Volume, Language, AutoTime, WifiOn, DebugLogOn
};
enum class EditId : uint8_t { DefrostWeight, ClockTime };

// Menu row geometry, shared by the UI (drawing) and App (touch hit-testing).
// Rows are 42 px tall because touch is only accurate to about +/-15 px.
// A tap above the rows (the title) goes back.
constexpr uint8_t MENU_ROWS = 4;
constexpr int16_t MENU_ROW_Y0 = 44;
constexpr int16_t MENU_ROW_H = 42;

class App {
public:
  void begin(Oven &oven, Door &door, Buzzer &buzzer, Clock &clock, WifiTime &wifi);
  void onInput(InputEvent event, int32_t value);
  void update(uint32_t now);

  // View for the UI
  Screen screen() const { return screen_; }
  bool dimmed() const { return dimmed_; }
  bool splash() const { return splash_; }  // start-up splash (logo, model, firmware) still showing
  bool powerView() const;  // the knob just moved: main screen shows the watts big
  uint8_t backlightPercent() const;  // includes the live preview while choosing brightness
  const char *toast() const;         // short message, or nullptr
  enum class Program : uint8_t { None, Defrost, Preset, Favourite };
  Program program() const { return program_; }  // loaded program (icon on the main screen)
  bool programText(char *buf, size_t len) const;  // loaded program, e.g. "Meat 500 g"

  const char *menuTitle() const;
  uint8_t menuCount() const;
  void menuRow(uint8_t i, char *label, size_t labelLen, char *value, size_t valueLen) const;
  uint8_t menuIndex() const { return menuIndex_; }
  uint8_t menuFirstVisible() const;  // scroll window keeping the selection visible

  EditId editId() const { return editId_; }
  const char *editTitle() const;
  // Big value as up to two fields (b empty for one field); `field` = the one being edited.
  void editValue(char *a, char *b, size_t len) const;
  uint8_t editField() const { return editField_; }
  void editInfo(char *buf, size_t len) const;

private:
  void use();
  void showToast(Str s, uint32_t ms = 0);
  void adjustTime(int32_t clicks, uint32_t now);
  void openMenu(MenuId id, uint8_t index = 0);
  void closeMenu();
  void menuBack();
  void menuSelect();
  void openEdit(EditId id);
  void editBack();
  void loadProgram(Program kind, uint8_t index, uint32_t seconds, uint16_t power, bool rotate);
  uint8_t choiceCount(MenuId id) const;
  uint8_t choiceCurrent(MenuId id) const;
  void choiceLabel(MenuId id, uint8_t i, char *buf, size_t len) const;
  void choiceApply(MenuId id, uint8_t i);
  void mainInput(InputEvent e, int32_t value);
  void menuInput(InputEvent e, int32_t value);
  void editInput(InputEvent e, int32_t value);
  void serviceInput(InputEvent e);
  void startWifiSetup();
  void leaveWifiSetup();
  void wifiSetupInput(InputEvent e);
  void startUpdate(bool logs = false);  // logs: Settings -> Debug log -> Download
  void leaveUpdate();
  void updateInput(InputEvent e);
  void onTouch(int16_t x, int16_t y);

  Oven *oven_ = nullptr;
  Door *door_ = nullptr;
  Buzzer *buzzer_ = nullptr;
  Clock *clock_ = nullptr;
  WifiTime *wifi_ = nullptr;

  Screen screen_ = Screen::Main;
  MenuId menuId_ = MenuId::Root;
  uint8_t menuIndex_ = 0;

  EditId editId_ = EditId::DefrostWeight;
  uint8_t editField_ = 0;
  uint8_t defrostType_ = 0;
  uint16_t grams_ = 500;
  uint8_t hour_ = 0, minute_ = 0;

  Program program_ = Program::None;
  uint8_t programIndex_ = 0;
  uint16_t programGrams_ = 0;
  uint32_t programSeconds_ = 0;  // as loaded (halfway point for "turn the food")
  bool turnBeeped_ = false;
  uint16_t potWatts_ = MAX_POWER_W;  // power the pot selects (restored when a program ends)
  bool potSeen_ = false;          // the pot's start-up report has come
  uint32_t powerViewAt_ = 0;      // last knob change (big watts for POWER_VIEW_MS)
  bool powerViewOn_ = false;

  // Encoder acceleration (setting the time on the main screen)
  uint32_t encLastMs_ = 0;
  int8_t encLastDir_ = 0;
  float encSpeed_ = 0;            // clicks per second, smoothed

  bool dimmed_ = false;
  bool splash_ = true;
  uint32_t splashStart_ = 0;
  uint32_t lastUse_ = 0;  // last user activity (menu timeout)
  uint32_t dimRef_ = 0;   // dim countdown start (also reset while dimming isn't allowed)

  Str toast_ = Str::Saved;
  uint32_t toastUntil_ = 0;

  bool lastDoorClosed_ = true;
  OvenState lastState_ = OvenState::Idle;
  uint32_t lastTouchPoll_ = 0;
  bool touchDown_ = false;
};
