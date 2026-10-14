#include "outputs.h"
#include "debug_log.h"
#include "config.h"
#include <driver/gpio.h>

static const uint8_t PINS[OUT_COUNT] = {
  PIN_MAGNETRON, PIN_FAN, PIN_TURNTABLE, PIN_LAMP, PIN_LED_ROTATE, PIN_LED_MAXPOWER
};

const char *Outputs::name(Output o) {
  static const char *NAMES[OUT_COUNT] = { "magnetron", "fan", "turntable", "lamp", "led-rotate", "led-maxpower" };
  return NAMES[o];
}

// Pin level for an output state (only the magnetron may be wired active-low).
static uint8_t level(Output o, bool on) {
  bool activeLow = (o == OUT_MAGNETRON) && MAGNETRON_ACTIVE_LOW;
  return (on != activeLow) ? HIGH : LOW;
}

void Outputs::begin() {
  // Set every output's OFF level before making it an output, so nothing glitches on.
  // gpio_set_level, not digitalWrite: the core ignores digitalWrite before pinMode, and a
  // software restart (ESP.restart, watchdog, crash) keeps the old output level, so a relay
  // that was on came back on (2026-10-14: the lamp after the easter-egg restart).
  for (uint8_t i = 0; i < OUT_COUNT; i++) {
    gpio_set_level((gpio_num_t)PINS[i], level((Output)i, false));
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], level((Output)i, false));
  }
  gpio_set_level((gpio_num_t)PIN_SPARE, LOW);
  pinMode(PIN_SPARE, OUTPUT);
  digitalWrite(PIN_SPARE, LOW);
}

void Outputs::write(Output o, bool on) {
  if (actual_[o] == on) return;
  actual_[o] = on;
  digitalWrite(PINS[o], level(o, on));
  if (o <= OUT_LAMP) Log.printf("[%7lu] OUT %-9s %s\n", millis(), name(o), on ? "ON" : "off");
}

void Outputs::update(uint32_t now, bool doorClosed) {
  bool fanOn = requested_[OUT_FAN];
  if (fanOn && !actual_[OUT_FAN]) fanOnSince_ = now;
  bool fanReady = fanOn && actual_[OUT_FAN] && (now - fanOnSince_ >= FAN_LEAD_MS);

  bool magnetronOn = requested_[OUT_MAGNETRON] && doorClosed && fanReady;

  // Magnetron first: when the fan turns off in this pass, the magnetron is already off.
  write(OUT_MAGNETRON, magnetronOn);
  write(OUT_FAN, fanOn);
  write(OUT_TURNTABLE, requested_[OUT_TURNTABLE]);
  write(OUT_LAMP, requested_[OUT_LAMP]);
  write(OUT_LED_ROTATE, requested_[OUT_LED_ROTATE]);
  write(OUT_LED_MAXPOWER, requested_[OUT_LED_MAXPOWER]);
}
