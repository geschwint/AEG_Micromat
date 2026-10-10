// AEG Micromat 143 replacement controller — main sketch.
// Waveshare ESP32-S3-Touch-LCD-2. See PLAN.md and HARDWARE.md.
//
// Everything is non-blocking: each module has an update(now) that checks the
// time and does a little work, and loop() calls them all on every pass.
// No delay() anywhere in the control path. A loop watchdog restarts the board
// (all outputs OFF) if loop() ever stalls.
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "config.h"
#include "outputs.h"
#include "buzzer.h"
#include "door.h"
#include "oven.h"
#include "clock.h"
#include "inputs.h"
#include "app.h"
#include "settings.h"
#include "wifi_creds.h"
#include "wifi_setup.h"
#include "wifi_time.h"
#include "ui.h"
#include "console.h"
#include "perf.h"
#include "touch_cst816.h"

Arduino_DataBus *lcdBus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_MOSI, PIN_LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(lcdBus, -1 /* no reset pin */, 1 /* landscape */, true /* IPS */, 240, 320);

Outputs outputs;
Buzzer buzzer;
Door door;
Oven oven;
Clock rtcClock;
Inputs inputs;
App app;
WifiTime wifiTime;
LoopStats loopStats;

// Input events go to App, which decides what they mean on the current screen.
static void onInputEvent(InputEvent event, int32_t value) {
  app.onInput(event, value);
}

void setup() {
  outputs.begin();  // first: every output OFF

  Serial.begin(115200);
  // With no reader on the PC, USB serial writes would otherwise wait up to
  // 100 ms each for buffer space, stalling loop(). Drop the log text instead.
  Serial.setTxTimeoutMs(0);
  Serial.println("Micromat controller starting");

  loadSettings();  // before anything that uses them (buzzer volume, UI language, ...)
  credsBegin();    // Wi-Fi network store; also stops the Wi-Fi driver keeping a plain-text copy
  buzzer.begin();
  door.begin();

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);

  bool touchOk = touchBegin(Wire, PIN_I2C_SDA, PIN_I2C_SCL, SCREEN_W, SCREEN_H);  // also starts Wire
  rtcClock.begin();
  Serial.printf("touch %s, RTC %s, door %s\n", touchOk ? "ok" : "FAILED", rtcClock.valid() ? "ok" : "not valid",
                door.simulated() ? "simulated" : "sensor");

  oven.begin(outputs, buzzer);
  wifiTime.begin(rtcClock);
  app.begin(oven, door, buzzer, rtcClock, wifiTime);
  inputs.begin(onInputEvent);
  uiBegin(gfx, app, oven, door, rtcClock, outputs, inputs, wifiTime);  // also sets up the backlight PWM

  enableLoopWDT();
  Serial.println("ready (send any unknown text for help)");
}

void loop() {
  static bool lastDoorClosed = door.closed();
  uint32_t startUs = micros();
  uint32_t now = millis();

  door.update(now);
  if (door.closed() != lastDoorClosed) {
    lastDoorClosed = door.closed();
    oven.doorChanged(lastDoorClosed, now);
  }

  inputs.update(now);                  // panel buttons, encoder, pot -> events -> App
  app.update(now);                     // touch, dimming, menu timeout
  oven.update(now, door.closed());     // decides what it wants on
  if (app.dimmed()) {                  // panel LEDs sleep with the display
    outputs.request(OUT_LED_ROTATE, false);
    outputs.request(OUT_LED_MAXPOWER, false);
  }
  outputs.update(now, door.closed());  // applies interlocks, drives pins
  buzzer.update(now);
  rtcClock.update(now);
  wifiSetup.update(now);               // Wi-Fi setup network + web page, while that screen is open
  wifiTime.update(now);                // clock over Wi-Fi (NTP), when enabled
  consoleUpdate(oven, door, rtcClock, outputs, inputs, wifiTime);
  uiUpdate();
  loopStats.record(micros() - startUs);
}
