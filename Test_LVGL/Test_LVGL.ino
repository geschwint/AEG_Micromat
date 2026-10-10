// LVGL 9 + touch test for Waveshare ESP32-S3-Touch-LCD-2.
// Mock microwave panel: big time display, power/status line, and six touch
// buttons (96x56 px). Start runs a simulated countdown — no outputs are driven.
// Needs lv_conf.h in the Arduino libraries folder (next to lvgl/).
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "touch_cst816.h"

#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1
#define LCD_BL   1

#define I2C_SDA  48
#define I2C_SCL  47

#define SCREEN_W 320  // landscape
#define SCREEN_H 240

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 1 /* landscape */, true /* IPS */, 240, 320);

// Partial render buffer: 40 lines.
static uint16_t drawBuf[SCREEN_W * 40];

// --- Simulated oven state ---------------------------------------------------
enum OvenState { IDLE, RUNNING, PAUSED, DONE };
static const char *STATE_NAMES[] = { "Ready", "Cooking", "Paused", "Done!" };
static const uint8_t POWER_LEVELS[] = { 100, 80, 60, 40, 20 };
#define MAX_SECONDS (99 * 60 + 59)

static OvenState state = IDLE;
static int seconds = 0;
static uint8_t powerIdx = 0;

static lv_obj_t *timeLabel;
static lv_obj_t *statusLabel;

enum ButtonId { BTN_MINUS30, BTN_PLUS30, BTN_POWER, BTN_CLEAR, BTN_START, BTN_STOP };

// --- LVGL glue --------------------------------------------------------------
static uint32_t tickCb() { return millis(); }

static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px, w, h);
  lv_display_flush_ready(disp);
}

static void touchReadCb(lv_indev_t *indev, lv_indev_data_t *data) {
  TouchPoint t;
  touchRead(t);
  if (t.pressed) {
    data->point.x = t.x;
    data->point.y = t.y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// --- UI ---------------------------------------------------------------------
static void updateDisplay() {
  lv_label_set_text_fmt(timeLabel, "%02d:%02d", seconds / 60, seconds % 60);
  lv_label_set_text_fmt(statusLabel, "Power %d%%   %s", POWER_LEVELS[powerIdx], STATE_NAMES[state]);
  lv_obj_set_style_text_color(timeLabel,
                              state == RUNNING ? lv_color_hex(0xFFA000) : lv_color_white(), 0);
}

static void setState(OvenState s) {
  if (s == state) return;
  state = s;
  Serial.printf("(sim) state -> %s, magnetron %s\n", STATE_NAMES[s], s == RUNNING ? "ON (duty cycled)" : "OFF");
}

static void onButton(lv_event_t *e) {
  ButtonId id = (ButtonId)(intptr_t)lv_event_get_user_data(e);
  switch (id) {
    case BTN_MINUS30: seconds = max(0, seconds - 30); break;
    case BTN_PLUS30:  seconds = min(MAX_SECONDS, seconds + 30); if (state == DONE) setState(IDLE); break;
    case BTN_POWER:   powerIdx = (powerIdx + 1) % sizeof(POWER_LEVELS); break;
    case BTN_CLEAR:   if (state != RUNNING) { seconds = 0; setState(IDLE); } break;
    case BTN_START:   if (seconds > 0) setState(RUNNING); break;
    case BTN_STOP:    if (state == RUNNING) setState(PAUSED); else { seconds = 0; setState(IDLE); } break;
  }
  Serial.printf("button %d -> %02d:%02d power %d%%\n", id, seconds / 60, seconds % 60, POWER_LEVELS[powerIdx]);
  updateDisplay();
}

static void countdownTimer(lv_timer_t *) {
  if (state != RUNNING) return;
  if (seconds > 0) seconds--;
  if (seconds == 0) setState(DONE);
  updateDisplay();
}

static lv_obj_t *makeButton(lv_obj_t *parent, const char *text, ButtonId id, int x, int y, uint32_t color) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, 96, 56);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
  lv_obj_add_event_cb(btn, onButton, LV_EVENT_CLICKED, (void *)(intptr_t)id);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  return btn;
}

static void buildUi() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  timeLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
  lv_obj_align(timeLabel, LV_ALIGN_TOP_MID, 0, 14);

  statusLabel = lv_label_create(scr);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x9EB3C2), 0);
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 76);

  // Two rows of three 96x56 buttons with 8 px gaps: x = 8, 112, 216; y = 116, 180.
  const uint32_t grey = 0x37474F, green = 0x2E7D32, red = 0xC62828;
  makeButton(scr, "-30s",  BTN_MINUS30, 8,   116, grey);
  makeButton(scr, "+30s",  BTN_PLUS30,  112, 116, grey);
  makeButton(scr, "Power", BTN_POWER,   216, 116, grey);
  makeButton(scr, "Clear", BTN_CLEAR,   8,   180, grey);
  makeButton(scr, "Start", BTN_START,   112, 180, green);
  makeButton(scr, "Stop",  BTN_STOP,    216, 180, red);

  updateDisplay();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Test_LVGL starting");

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  Serial.printf("touch init: %s\n", touchBegin(Wire, I2C_SDA, I2C_SCL, SCREEN_W, SCREEN_H) ? "ok" : "failed");

  lv_init();
  lv_tick_set_cb(tickCb);

  lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, drawBuf, nullptr, sizeof(drawBuf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchReadCb);

  buildUi();
  lv_timer_create(countdownTimer, 1000, nullptr);
  Serial.printf("LVGL %d.%d.%d ready\n", lv_version_major(), lv_version_minor(), lv_version_patch());
}

void loop() {
  lv_timer_handler();
  delay(5);
}
