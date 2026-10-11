#include "ui.h"
#include <lvgl.h>
#include "config.h"
#include "font_montserrat_ttf.h"
#include "perf.h"
#include "ui_text.h"
#include "wifi_setup.h"
#include "about.h"
#include "fw_update.h"
#include "logo_aeg.h"
#include "hotspot.h"

static Arduino_GFX *gfx_;
static App *app_;
static Oven *oven_;
static Door *door_;
static Clock *clock_;
static WifiTime *wifi_;
static Outputs *outputs_;
static Inputs *inputs_;

static uint16_t drawBuf[SCREEN_W * 40];  // partial render buffer, 40 lines

// Colours
static constexpr uint32_t C_BG = 0x101418, C_DIM_TEXT = 0x9EB3C2, C_WHITE = 0xFFFFFF, C_GREY = 0x607D8B,
                          C_ORANGE = 0xFFA000, C_YELLOW = 0xFFE082, C_GREEN = 0x66BB6A, C_AMBER = 0xFFC107,
                          C_RED = 0xE53935, C_PANEL = 0x37474F, C_BAR_BG = 0x263238, C_ICE = 0x81D4FA;

// Views
static lv_obj_t *mainView, *dimView, *menuView, *editView, *serviceView, *splashView;
// Main view
static lv_obj_t *clockLabel, *timeLabel, *subLabel, *toastLabel, *powerBar, *powerLabel, *magOn, *magOff;
static lv_obj_t *progDefrost, *progPreset, *progFavourite;  // program icons
// Dimmed view
static lv_obj_t *dimClock;
// Menu view (row geometry from app.h, shared with touch hit-testing)
static lv_obj_t *menuTitle, *menuHighlight, *menuRows[MENU_ROWS], *menuValues[MENU_ROWS], *menuHint;
// Editor view: up to two big fields (e.g. hours, minutes)
static lv_obj_t *editTitle, *editA, *editSep, *editB, *editInfo, *editHint;
// Wi-Fi setup view: QR code to join the setup network, its name/password, status
static lv_obj_t *wifiView, *wifiTitle, *wifiQr, *wifiCaps[3], *wifiVals[3], *wifiStatus, *wifiHint;
static constexpr int32_t QR_PX = 112;
// Service view
static constexpr uint8_t SERVICE_LINES = 10;
static lv_obj_t *serviceTitle, *serviceLines[SERVICE_LINES], *serviceHint;

// Fonts: Montserrat via Tiny TTF (built-in LVGL fonts are ASCII only, Swedish
// needs å/ä/ö). Each falls back to the built-in font of that size for symbols
// such as LV_SYMBOL_OK. Glyphs are cached after first use.
static lv_font_t *font14, *font20, *font28, *bigFont;

// --- LVGL glue ------------------------------------------------------------------
static uint32_t tickCb() { return millis(); }

static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  gfx_->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px, lv_area_get_width(area), lv_area_get_height(area));
  lv_display_flush_ready(disp);
}

// --- Change-only setters (see ui.h) -------------------------------------------------
static void setText(lv_obj_t *label, const char *text) {
  if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static void setColor(lv_obj_t *obj, uint32_t hex) {
  lv_color_t c = lv_color_hex(hex);
  if (!lv_color_eq(lv_obj_get_style_text_color(obj, LV_PART_MAIN), c)) lv_obj_set_style_text_color(obj, c, 0);
}

static void setHidden(lv_obj_t *obj, bool hidden) {
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) != hidden) {
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

static void setY(lv_obj_t *obj, int32_t y) {
  if (lv_obj_get_y(obj) != y) lv_obj_set_y(obj, y);
}

// --- Builders -----------------------------------------------------------------------
// A transparent, borderless, non-scrolling container.
static lv_obj_t *makeBox(lv_obj_t *parent, int32_t w, int32_t h) {
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_radius(b, 0, 0);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
  return b;
}

static lv_obj_t *makeView(lv_obj_t *scr) {
  lv_obj_t *v = makeBox(scr, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(v, 0, 0);
  lv_obj_add_flag(v, LV_OBJ_FLAG_HIDDEN);
  return v;
}

static lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *font, uint32_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, "");
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}

static lv_font_t *makeFont(int32_t px, const lv_font_t *fallback) {
  lv_font_t *f = lv_tiny_ttf_create_data(MONTSERRAT_TTF, sizeof(MONTSERRAT_TTF), px);
  f->fallback = fallback;
  return f;
}

// Big digits at BIG_DIGIT_PX (LVGL's built-in fonts stop at 48 px). Scaling a
// 48 px label with transform_scale instead took up to 100 ms per redraw.
static lv_obj_t *makeBigDigits(lv_obj_t *parent) {
  return makeLabel(parent, bigFont, C_WHITE);
}

// Magnetron icon (heat symbol): three wavy arrows rising from a tray, drawn
// with lines in a 34x30 box (about the size of the old icon). The "off" version is grey with a red slash.
// lv_line keeps a pointer to its points, so they are static and shared by
// both versions.
static constexpr int32_t ICON_W = 34, ICON_H = 30;
static constexpr uint8_t WAVE_PTS = 17;      // one point per pixel row
static constexpr int32_t WAVE_TOP = 8;       // arrowhead base
static constexpr int32_t WAVE_X[] = { 9, 17, 25 };
static lv_point_precise_t wavePts[3][WAVE_PTS];
static lv_point_precise_t headPts[3][4];

static void makeLine(lv_obj_t *parent, const lv_point_precise_t *pts, uint32_t n, int32_t width, uint32_t color) {
  lv_obj_t *l = lv_line_create(parent);
  lv_line_set_points(l, pts, n);
  lv_obj_set_style_line_width(l, width, 0);
  lv_obj_set_style_line_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_line_rounded(l, true, 0);
}

static lv_obj_t *makeMagIcon(lv_obj_t *parent, bool on) {
  static const lv_point_precise_t TRAY[] = { { 2, 20 }, { 2, 28 }, { 32, 28 }, { 32, 20 } };
  static const lv_point_precise_t SLASH[] = { { 2, 29 }, { 32, 1 } };
  static bool built = false;
  if (!built) {
    built = true;
    for (uint8_t w = 0; w < 3; w++) {
      int32_t x0 = WAVE_X[w];
      for (uint8_t i = 0; i < WAVE_PTS; i++) {  // about 1.5 periods, 2 px each side
        wavePts[w][i].x = x0 + (int32_t)lroundf(2.0f * sinf(2 * PI * i / 11.0f));
        wavePts[w][i].y = WAVE_TOP + i;
      }
      // Arrowhead: a small closed triangle; a 3 px line almost fills it.
      headPts[w][0] = { x0 - 3, WAVE_TOP };
      headPts[w][1] = { x0, WAVE_TOP - 5 };
      headPts[w][2] = { x0 + 3, WAVE_TOP };
      headPts[w][3] = { x0 - 3, WAVE_TOP };
    }
  }
  const uint32_t color = on ? C_ORANGE : C_GREY;

  lv_obj_t *box = makeBox(parent, ICON_W, ICON_H);
  lv_obj_align(box, LV_ALIGN_TOP_RIGHT, -6, 2);
  for (uint8_t w = 0; w < 3; w++) {
    makeLine(box, wavePts[w], WAVE_PTS, 3, color);
    makeLine(box, headPts[w], 4, 3, color);
  }
  makeLine(box, TRAY, 4, 3, color);
  if (!on) makeLine(box, SLASH, 2, 3, C_RED);
  return box;
}

// Program icons, left of the magnetron icon while a program from the menu is
// loaded: snowflake = defrost, cloche = preset (shared by all presets for now),
// star = favourite. Same size and line style as the magnetron icon.
static constexpr int32_t PROG_ICON_X = -6 - ICON_W - 8;  // from the right edge
static lv_point_precise_t flakeArms[3][2], flakeTips[6][3], starPts[11], clochePts[13];

static lv_point_precise_t polar(float cx, float cy, float r, float a) {
  return { (int32_t)lroundf(cx + r * cosf(a)), (int32_t)lroundf(cy - r * sinf(a)) };
}

static lv_obj_t *makeProgIcon(lv_obj_t *parent) {
  lv_obj_t *box = makeBox(parent, ICON_W, ICON_H);
  lv_obj_align(box, LV_ALIGN_TOP_RIGHT, PROG_ICON_X, 2);
  lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
  return box;
}

static lv_obj_t *makeDefrostIcon(lv_obj_t *parent) {
  const float cx = 17, cy = 15;
  for (uint8_t i = 0; i < 3; i++) {  // three lines through the centre
    float a = PI / 2 + i * PI / 3;
    flakeArms[i][0] = polar(cx, cy, 13, a);
    flakeArms[i][1] = polar(cx, cy, 13, a + PI);
  }
  for (uint8_t i = 0; i < 6; i++) {  // a "V" on each arm, opening outwards
    float a = PI / 2 + i * PI / 3;
    flakeTips[i][0] = polar(cx, cy, 11, a - 0.5f);
    flakeTips[i][1] = polar(cx, cy, 7, a);
    flakeTips[i][2] = polar(cx, cy, 11, a + 0.5f);
  }
  lv_obj_t *box = makeProgIcon(parent);
  for (uint8_t i = 0; i < 3; i++) makeLine(box, flakeArms[i], 2, 2, C_ICE);
  for (uint8_t i = 0; i < 6; i++) makeLine(box, flakeTips[i], 3, 2, C_ICE);
  return box;
}

static lv_obj_t *makeFavouriteIcon(lv_obj_t *parent) {
  for (uint8_t i = 0; i < 11; i++) {  // 5-point star outline, closed
    starPts[i] = polar(17, 16, (i % 2) ? 6 : 14, PI / 2 + i * PI / 5);
  }
  lv_obj_t *box = makeProgIcon(parent);
  makeLine(box, starPts, 11, 3, C_AMBER);
  return box;
}

static lv_obj_t *makePresetIcon(lv_obj_t *parent) {
  static const lv_point_precise_t BASE[] = { { 2, 26 }, { 32, 26 } };
  static const lv_point_precise_t KNOB[] = { { 17, 7 }, { 17, 9 } };
  for (uint8_t i = 0; i < 13; i++) clochePts[i] = polar(17, 24, 13, PI - i * PI / 12);  // dome
  lv_obj_t *box = makeProgIcon(parent);
  makeLine(box, clochePts, 13, 3, C_DIM_TEXT);
  makeLine(box, BASE, 2, 3, C_DIM_TEXT);
  makeLine(box, KNOB, 2, 4, C_DIM_TEXT);
  return box;
}

static void buildMainView(lv_obj_t *scr) {
  mainView = makeView(scr);
  clockLabel = makeLabel(mainView, font20, C_DIM_TEXT);
  lv_obj_align(clockLabel, LV_ALIGN_TOP_LEFT, 8, 4);
  magOn = makeMagIcon(mainView, true);
  magOff = makeMagIcon(mainView, false);
  progDefrost = makeDefrostIcon(mainView);
  progPreset = makePresetIcon(mainView);
  progFavourite = makeFavouriteIcon(mainView);

  timeLabel = makeBigDigits(mainView);
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, 0, -22);
  subLabel = makeLabel(mainView, font20, C_YELLOW);
  lv_obj_align(subLabel, LV_ALIGN_CENTER, 0, 32);
  toastLabel = makeLabel(mainView, font20, C_DIM_TEXT);
  lv_obj_align(toastLabel, LV_ALIGN_CENTER, 0, 60);

  powerBar = lv_bar_create(mainView);
  lv_obj_set_size(powerBar, 288, 28);
  lv_obj_align(powerBar, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_bar_set_range(powerBar, 0, 100);
  lv_obj_set_style_bg_color(powerBar, lv_color_hex(C_BAR_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(powerBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(powerBar, lv_color_hex(C_ORANGE), LV_PART_INDICATOR);
  lv_obj_set_style_radius(powerBar, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(powerBar, 6, LV_PART_INDICATOR);
  powerLabel = makeLabel(powerBar, font20, C_WHITE);
  lv_obj_center(powerLabel);
}

// Start-up splash: the AEG logo (white background, like the brand), the oven
// model and the firmware version/date. Static: built once, never refreshed.
static void buildSplashView(lv_obj_t *scr) {
  splashView = makeView(scr);
  lv_obj_set_style_bg_color(splashView, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(splashView, LV_OPA_COVER, 0);
  lv_obj_t *logo = lv_image_create(splashView);
  lv_image_set_src(logo, &LOGO_AEG);
  lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 26);
  lv_obj_t *model = makeLabel(splashView, font28, C_BAR_BG);
  lv_label_set_text(model, OVEN_MODEL);
  lv_obj_align(model, LV_ALIGN_TOP_MID, 0, 26 + LOGO_AEG.header.h + 16);
  lv_obj_t *fw = makeLabel(splashView, font20, C_GREY);
  char buf[48];
  snprintf(buf, sizeof(buf), "%s %s  \xC2\xB7  %s", tr(Str::AboutFirmware), FIRMWARE_VERSION, FIRMWARE_DATE);
  lv_label_set_text(fw, buf);
  lv_obj_align(fw, LV_ALIGN_TOP_MID, 0, 26 + LOGO_AEG.header.h + 16 + 42);
}

static void buildDimView(lv_obj_t *scr) {
  dimView = makeView(scr);
  dimClock = makeBigDigits(dimView);
  setColor(dimClock, C_GREY);
  lv_obj_center(dimClock);
}

static void buildMenuView(lv_obj_t *scr) {
  menuView = makeView(scr);
  menuTitle = makeLabel(menuView, font28, C_ORANGE);
  lv_obj_align(menuTitle, LV_ALIGN_TOP_LEFT, 12, 6);

  menuHighlight = makeBox(menuView, 296, MENU_ROW_H - 4);
  lv_obj_set_pos(menuHighlight, 12, MENU_ROW_Y0 + 2);
  lv_obj_set_style_radius(menuHighlight, 6, 0);
  lv_obj_set_style_bg_opa(menuHighlight, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(menuHighlight, lv_color_hex(C_PANEL), 0);

  // Each row: label on the left, value (current setting, summary or check) on the right.
  for (uint8_t i = 0; i < MENU_ROWS; i++) {
    int32_t y = MENU_ROW_Y0 + i * MENU_ROW_H + (MENU_ROW_H - 22) / 2;
    menuRows[i] = makeLabel(menuView, font20, C_WHITE);
    lv_obj_set_pos(menuRows[i], 24, y);
    menuValues[i] = makeLabel(menuView, font20, C_YELLOW);
    lv_obj_align(menuValues[i], LV_ALIGN_TOP_RIGHT, -24, y);
  }
  menuHint = makeLabel(menuView, font14, C_GREY);
  lv_obj_align(menuHint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void buildEditView(lv_obj_t *scr) {
  editView = makeView(scr);
  editTitle = makeLabel(editView, font28, C_ORANGE);
  lv_obj_align(editTitle, LV_ALIGN_TOP_LEFT, 12, 6);

  // Fields in a centred row, so one field ("500 g") or two ("07" ":" "45") stay centred.
  lv_obj_t *row = makeBox(editView, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, -10);
  editA = makeBigDigits(row);
  editSep = makeBigDigits(row);
  lv_label_set_text(editSep, ":");
  editB = makeBigDigits(row);

  editInfo = makeLabel(editView, font20, C_DIM_TEXT);
  lv_obj_align(editInfo, LV_ALIGN_CENTER, 0, 52);
  editHint = makeLabel(editView, font14, C_GREY);
  lv_obj_align(editHint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void buildWifiView(lv_obj_t *scr) {
  wifiView = makeView(scr);
  wifiTitle = makeLabel(wifiView, font28, C_ORANGE);
  lv_obj_align(wifiTitle, LV_ALIGN_TOP_LEFT, 12, 6);
  wifiQr = lv_qrcode_create(wifiView);
  lv_qrcode_set_size(wifiQr, QR_PX);
  lv_qrcode_set_dark_color(wifiQr, lv_color_black());
  lv_qrcode_set_light_color(wifiQr, lv_color_white());
  lv_obj_set_style_border_color(wifiQr, lv_color_white(), 0);  // quiet zone, so phones can read it
  lv_obj_set_style_border_width(wifiQr, 4, 0);
  lv_obj_set_pos(wifiQr, 14, 46);
  // Name / password / address of the setup network, right of the code.
  for (uint8_t i = 0; i < 3; i++) {
    wifiCaps[i] = makeLabel(wifiView, font14, C_GREY);
    lv_obj_set_pos(wifiCaps[i], 146, 44 + i * 40);
    wifiVals[i] = makeLabel(wifiView, font20, C_WHITE);
    lv_obj_set_pos(wifiVals[i], 146, 60 + i * 40);
  }
  wifiStatus = makeLabel(wifiView, font20, C_YELLOW);
  lv_obj_set_width(wifiStatus, SCREEN_W - 28);
  lv_label_set_long_mode(wifiStatus, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(wifiStatus, 14, 168);
  wifiHint = makeLabel(wifiView, font14, C_GREY);
  lv_obj_align(wifiHint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void buildServiceView(lv_obj_t *scr) {
  serviceView = makeView(scr);
  serviceTitle = makeLabel(serviceView, font20, C_ORANGE);
  lv_obj_align(serviceTitle, LV_ALIGN_TOP_LEFT, 10, 4);
  for (uint8_t i = 0; i < SERVICE_LINES; i++) {
    serviceLines[i] = makeLabel(serviceView, font14, C_DIM_TEXT);
    lv_obj_set_pos(serviceLines[i], 10, 30 + i * 18);  // 10 lines of 14 px text above the hint
  }
  serviceHint = makeLabel(serviceView, font14, C_GREY);
  lv_obj_align(serviceHint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

// --- Refresh ------------------------------------------------------------------------
static void formatClock(char *buf, size_t len) {
  if (clock_->valid()) strftime(buf, len, "%H:%M", &clock_->local());
  else snprintf(buf, len, "--:--");
}

static void formatDuration(char *buf, size_t len, uint32_t secs) {
  if (secs >= 3600) snprintf(buf, len, "%lu:%02lu:%02lu", secs / 3600, (secs / 60) % 60, secs % 60);
  else snprintf(buf, len, "%02lu:%02lu", secs / 60, secs % 60);
}

static void refreshMain() {
  char buf[48];
  bool magnetronOn = outputs_->isOn(OUT_MAGNETRON);
  setHidden(magOn, !magnetronOn);
  setHidden(magOff, magnetronOn);
  App::Program prog = app_->program();
  setHidden(progDefrost, prog != App::Program::Defrost);
  setHidden(progPreset, prog != App::Program::Preset);
  setHidden(progFavourite, prog != App::Program::Favourite);

  OvenState st = oven_->state();
  uint32_t secs = oven_->remainingSeconds();
  // Clock view: with nothing set (Idle, 0 s) or finished (Done), the current time
  // is shown large in the centre; otherwise the clock moves to the top left and
  // the big digits show the timer. The switch back waits: "00:00" stays for
  // ZERO_TO_CLOCK_MS, and a large "End" for END_BIG_MS.
  enum Rest : uint8_t { Busy, Zero, Finished };
  static Rest rest = Zero;
  static uint32_t restSince = 0;
  static bool clockView = true;  // boot straight into the clock
  Rest r = st == OvenState::Done ? Finished : (st == OvenState::Idle && secs == 0) ? Zero : Busy;
  uint32_t now = millis();
  if (r != rest) {
    if (rest == Finished && r == Zero) clockView = true;  // End acknowledged (Stop/door)
    else clockView = false;
    rest = r;
    restSince = now;
  }
  if (!clockView && r != Busy && now - restSince >= (r == Zero ? ZERO_TO_CLOCK_MS : END_BIG_MS)) clockView = true;

  bool showClockBig = clockView && door_->closed();
  if (showClockBig) setText(clockLabel, "");
  else {
    formatClock(buf, sizeof(buf));
    setText(clockLabel, buf);
  }

  uint32_t color;
  if (!door_->closed()) {
    snprintf(buf, sizeof(buf), "%s", tr(Str::Open));
    color = C_AMBER;
  } else if (clockView) {
    formatClock(buf, sizeof(buf));
    color = C_WHITE;
  } else if (st == OvenState::Done) {
    snprintf(buf, sizeof(buf), "%s", tr(Str::End));
    color = C_GREEN;
  } else {
    formatDuration(buf, sizeof(buf), secs);
    if (st == OvenState::Cooking) color = C_ORANGE;
    else if (st == OvenState::Paused) color = C_YELLOW;
    else color = C_WHITE;
  }
  setText(timeLabel, buf);
  setColor(timeLabel, color);

  // Small caption under the big digits: "Paused"; "End" (green) under the clock
  // until the finished cook is acknowledged (Stop, door, or a new time); or the
  // program loaded from the menu, e.g. "Defrost: Meat 500 g".
  char program[48];
  const char *sub = "";
  uint32_t subColor = C_YELLOW;
  if (st == OvenState::Paused) {
    sub = tr(Str::Paused);
  } else if (st == OvenState::Done && showClockBig) {
    sub = tr(Str::End);
    subColor = C_GREEN;
  } else if (!clockView && door_->closed() && app_->programText(program, sizeof(program))) {
    sub = program;
    subColor = C_DIM_TEXT;
  }
  setText(subLabel, sub);
  setColor(subLabel, subColor);
  const char *toast = app_->toast();
  setText(toastLabel, toast ? toast : "");

  int32_t power = oven_->power();
  if (lv_bar_get_value(powerBar) != power) lv_bar_set_value(powerBar, power, LV_ANIM_OFF);
  if (oven_->timerOnly()) snprintf(buf, sizeof(buf), "%s", tr(Str::Timer));
  else snprintf(buf, sizeof(buf), "%ld %%", (long)power);
  setText(powerLabel, buf);
}

static void refreshDim() {
  char buf[16];
  formatClock(buf, sizeof(buf));
  setText(dimClock, buf);
}

// Title with a back arrow (tapping the title goes back).
static void setTitle(lv_obj_t *label, const char *title) {
  char buf[48];
  snprintf(buf, sizeof(buf), LV_SYMBOL_LEFT " %s", title);
  setText(label, buf);
}

// The hint line also shows short messages (toasts) while they last.
static void setHint(lv_obj_t *label, Str hint) {
  const char *toast = app_->toast();
  setText(label, toast ? toast : tr(hint));
  setColor(label, toast ? C_YELLOW : C_GREY);
}

static void refreshMenu() {
  setTitle(menuTitle, app_->menuTitle());
  uint8_t count = app_->menuCount(), sel = app_->menuIndex();
  uint8_t first = app_->menuFirstVisible();
  char label[40], value[24];
  for (uint8_t r = 0; r < MENU_ROWS; r++) {
    uint8_t i = first + r;
    if (i < count) app_->menuRow(i, label, sizeof(label), value, sizeof(value));
    else label[0] = value[0] = '\0';
    setText(menuRows[r], label);
    setColor(menuRows[r], i == sel ? C_WHITE : C_DIM_TEXT);
    setText(menuValues[r], value);
  }
  setY(menuHighlight, MENU_ROW_Y0 + (sel - first) * MENU_ROW_H + 2);
  setHint(menuHint, Str::MenuHint);
}

static void refreshEdit() {
  setTitle(editTitle, app_->editTitle());
  char a[16], b[16], info[40];
  app_->editValue(a, b, sizeof(a));
  bool two = b[0] != '\0';
  setText(editA, a);
  setText(editB, b);
  setHidden(editSep, !two);
  setHidden(editB, !two);
  // With two fields, the one being edited is orange.
  setColor(editA, two && app_->editField() == 0 ? C_ORANGE : C_WHITE);
  setColor(editB, app_->editField() == 1 ? C_ORANGE : C_WHITE);
  app_->editInfo(info, sizeof(info));
  setText(editInfo, info);
  setHint(editHint, app_->editId() == EditId::ClockTime ? Str::ClockHint : Str::EditHint);
}

// Wi-Fi setup and firmware update share this view. While the hotspot is open:
// QR code + its name/password/address, and what's happening; afterwards only
// the result.
static void refreshHotspotView(bool update) {
  setTitle(wifiTitle, tr(update ? Str::FirmwareUpdate : Str::WifiSetup));
  bool open = hotspotActive();
  static char qrShown[64] = "";
  if (open && strcmp(qrShown, hotspotQr()) != 0) {
    strlcpy(qrShown, hotspotQr(), sizeof(qrShown));
    lv_qrcode_update(wifiQr, qrShown, strlen(qrShown));
  }
  if (!open) qrShown[0] = '\0';
  setHidden(wifiQr, !open);
  static const Str CAPS[] = { Str::Network, Str::Password, Str::Address };
  const char *vals[] = { hotspotName(), hotspotPass(), HOTSPOT_IP };
  for (uint8_t i = 0; i < 3; i++) {
    setText(wifiCaps[i], open ? tr(CAPS[i]) : "");
    setText(wifiVals[i], open ? vals[i] : "");
  }

  char buf[128];
  const char *text = "";
  uint32_t color = C_YELLOW;
  const lv_font_t *font = font20;
  if (update) {
    switch (firmwareUpdate.state()) {
      case FirmwareUpdate::State::Waiting:
        snprintf(buf, sizeof(buf), "%s", tr(Str::UpdateSignIn));
        if (firmwareUpdate.homeIp()[0]) {
          size_t n = strlen(buf);
          buf[n++] = '\n';
          snprintf(buf + n, sizeof(buf) - n, tr(Str::UpdateHome), firmwareUpdate.homeIp());
        }
        text = buf;
        font = font14;  // two or three lines
        break;
      case FirmwareUpdate::State::Receiving:
        snprintf(buf, sizeof(buf), tr(Str::UpdateReceiving), firmwareUpdate.progress());
        text = buf;
        break;
      case FirmwareUpdate::State::Done: text = tr(Str::UpdateDone); color = C_GREEN; break;
      case FirmwareUpdate::State::Failed:
        snprintf(buf, sizeof(buf), tr(Str::UpdateFailed), firmwareUpdate.error());
        text = buf;
        color = C_RED;
        break;
      case FirmwareUpdate::State::TimedOut: text = tr(Str::UpdateClosed); color = C_GREY; break;
      default: break;
    }
  } else {
    switch (wifiSetup.state()) {
      case WifiSetup::State::Waiting:
        text = tr(wifiSetup.phoneConnected() ? Str::SetupPhone : Str::SetupScan);
        break;
      case WifiSetup::State::Testing:
        snprintf(buf, sizeof(buf), tr(Str::SetupTesting), wifiSetup.testSsid());
        text = buf;
        break;
      case WifiSetup::State::Success:    text = tr(Str::SetupOk); color = C_GREEN; break;
      case WifiSetup::State::NotFound:   text = tr(Str::SetupNotFound); color = C_RED; break;
      case WifiSetup::State::NoConnect:  text = tr(Str::SetupNoConnect); color = C_RED; break;
      case WifiSetup::State::SaveFailed: text = tr(Str::SetupSaveFailed); color = C_RED; break;
      case WifiSetup::State::TimedOut:   text = tr(Str::SetupTimeout); color = C_GREY; break;
      default: break;
    }
  }
  if (lv_obj_get_style_text_font(wifiStatus, LV_PART_MAIN) != font) lv_obj_set_style_text_font(wifiStatus, font, 0);
  setText(wifiStatus, text);
  setColor(wifiStatus, color);
  if (update && open && !app_->toast()) {  // the running version, next to the hint
    snprintf(buf, sizeof(buf), "%s   %s", tr(Str::SetupHint), FirmwareUpdate::version());
    setText(wifiHint, buf);
    setColor(wifiHint, C_GREY);
  } else {
    setHint(wifiHint, open ? Str::SetupHint : Str::SetupDoneHint);
  }
}

static void refreshService() {
  char buf[96];
  auto onOff = [](Output o) { return outputs_->isOn(o) ? "ON " : "off"; };
  snprintf(buf, sizeof(buf), "Out: MAG %s  FAN %s  TT %s  LAMP %s", onOff(OUT_MAGNETRON), onOff(OUT_FAN),
           onOff(OUT_TURNTABLE), onOff(OUT_LAMP));
  setText(serviceLines[0], buf);
  snprintf(buf, sizeof(buf), "Door: %s%s", door_->closed() ? "closed" : "OPEN",
           door_->simulated() ? "  (simulated: tap to toggle)" : "  (sensor GPIO14)");
  setText(serviceLines[1], buf);
  snprintf(buf, sizeof(buf), "Ladder: %u mV  key %s", inputs_->ladderMv(), Inputs::keyName(inputs_->key()));
  setText(serviceLines[2], buf);
  snprintf(buf, sizeof(buf), "Pot: %u mV   Encoder: %ld steps, push %s", inputs_->potMv(),
           (long)inputs_->encoderSteps(), inputs_->pushPressed() ? "down" : "up");
  setText(serviceLines[3], buf);
  snprintf(buf, sizeof(buf), "Oven: %s  %lu s  %u %%  rotate %s", Oven::stateName(oven_->state()),
           (unsigned long)oven_->remainingSeconds(), oven_->power(), oven_->rotate() ? "on" : "off");
  setText(serviceLines[4], buf);
  if (clock_->valid()) strftime(buf, sizeof(buf), "RTC: %Y-%m-%d %H:%M:%S %Z", &clock_->local());
  else snprintf(buf, sizeof(buf), "RTC: not valid");
  setText(serviceLines[5], buf);
  snprintf(buf, sizeof(buf), "Loop max: %lu us   Heap free: %lu", (unsigned long)loopStats.maxUs(),
           (unsigned long)ESP.getFreeHeap());
  setText(serviceLines[6], buf);
  snprintf(buf, sizeof(buf), "Fan run-on: %s   Heat last 4 min: %u s", oven_->fanRunningOn() ? "yes" : "no",
           oven_->recentHeatSeconds());
  setText(serviceLines[7], buf);
  snprintf(buf, sizeof(buf), "Wi-Fi: %s", wifi_->status());
  setText(serviceLines[8], buf);
  snprintf(buf, sizeof(buf), "Firmware: %s  build %s (%s)", FIRMWARE_VERSION, FirmwareUpdate::version(),
           FirmwareUpdate::runningSlot());
  setText(serviceLines[9], buf);
  setText(serviceTitle, tr(Str::Service));
  setText(serviceHint, tr(Str::ServiceHint));
}

static void refresh(lv_timer_t *) {
  // Backlight: brightness setting, the dimmed level, or a live preview while
  // choosing either in the menu.
  static int16_t backlightPct = -1;
  uint8_t pct = app_->backlightPercent();
  if (backlightPct != pct) {
    backlightPct = pct;
    ledcWrite(PIN_LCD_BL, (uint32_t)pct * 255 / 100);
  }

  Screen s = app_->screen();
  if (app_->splash()) {  // over everything else until it ends
    setHidden(splashView, false);
    for (lv_obj_t *v : { mainView, dimView, menuView, editView, serviceView, wifiView }) setHidden(v, true);
    return;
  }
  setHidden(splashView, true);
  bool dim = s == Screen::Main && app_->dimmed();
  setHidden(mainView, !(s == Screen::Main && !dim));
  setHidden(dimView, !dim);
  setHidden(menuView, s != Screen::Menu);
  setHidden(editView, s != Screen::Edit);
  setHidden(serviceView, s != Screen::Service);
  setHidden(wifiView, s != Screen::WifiSetup && s != Screen::Update);

  if (dim) refreshDim();
  else if (s == Screen::Main) refreshMain();
  else if (s == Screen::Menu) refreshMenu();
  else if (s == Screen::Edit) refreshEdit();
  else if (s == Screen::WifiSetup) refreshHotspotView(false);
  else if (s == Screen::Update) refreshHotspotView(true);
  else refreshService();
}

void uiBegin(Arduino_GFX *gfx, App &app, Oven &oven, Door &door, Clock &clock, Outputs &outputs, Inputs &inputs,
             WifiTime &wifi) {
  gfx_ = gfx;
  app_ = &app;
  oven_ = &oven;
  door_ = &door;
  clock_ = &clock;
  wifi_ = &wifi;
  outputs_ = &outputs;
  inputs_ = &inputs;

  ledcAttach(PIN_LCD_BL, 5000, 8);
  ledcWrite(PIN_LCD_BL, 0);  // refresh() sets the brightness from Settings

  lv_init();
  lv_tick_set_cb(tickCb);
  lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, drawBuf, nullptr, sizeof(drawBuf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  font14 = makeFont(14, &lv_font_montserrat_14);
  font20 = makeFont(20, &lv_font_montserrat_20);
  font28 = makeFont(28, &lv_font_montserrat_28);
  bigFont = makeFont(BIG_DIGIT_PX, &lv_font_montserrat_48);

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  buildMainView(scr);
  buildDimView(scr);
  buildSplashView(scr);
  buildMenuView(scr);
  buildEditView(scr);
  buildServiceView(scr);
  buildWifiView(scr);

  lv_timer_create(refresh, 33, nullptr);
  refresh(nullptr);
}

void uiUpdate() {
  lv_timer_handler();
}
