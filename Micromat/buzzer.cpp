#include "buzzer.h"
#include "config.h"
#include "settings.h"

static constexpr uint8_t RES_BITS = 10;

void Buzzer::begin() {
  ledcAttach(PIN_BUZZER, BEEP_FREQ_HZ, RES_BITS);
  ledcWrite(PIN_BUZZER, 0);
}

void Buzzer::sound(bool on) {
  on_ = on;
  ledcWrite(PIN_BUZZER, on ? VOLUME_DUTY[settings.volume] : 0);  // up to a 50 % square wave, or silent
}

void Buzzer::beep(uint8_t count, uint16_t onMs, uint16_t offMs) {
  remaining_ = count;
  onMs_ = onMs;
  offMs_ = offMs;
  active_ = count > 0;
  sound(false);
  phaseStart_ = millis() - offMs;  // start the first beep on the next update()
}

void Buzzer::update(uint32_t now) {
  if (!active_) return;
  uint32_t elapsed = now - phaseStart_;
  if (on_) {
    if (elapsed >= onMs_) {
      sound(false);
      phaseStart_ = now;
      if (remaining_ == 0) active_ = false;
    }
  } else if (elapsed >= offMs_) {
    if (remaining_ == 0) {
      active_ = false;
      return;
    }
    remaining_--;
    sound(true);
    phaseStart_ = now;
  }
}

void Buzzer::click() {
  if (settings.keyClicks) beep(1, 30, 0);
}
