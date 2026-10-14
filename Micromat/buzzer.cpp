#include "buzzer.h"
#include "config.h"
#include "settings.h"
#include "eggs.h"

static constexpr uint8_t RES_BITS = 10;
static constexpr uint16_t NOTE_GAP_MS = 25;  // silence after each note (every note is longer)

// Für Elise (Beethoven, public domain), the opening, two octaves up: the
// piezo is loud at 1.7-2.7 kHz and quiet much below. Eighth note = 150 ms.
// (N_ prefix: A6 etc. are Arduino pin names.)
enum : uint16_t { N_C6 = 1047, N_E6 = 1319, N_GS6 = 1661, N_A6 = 1760, N_B6 = 1976, N_C7 = 2093, N_D7 = 2349, N_DS7 = 2489, N_E7 = 2637 };
static const Note FUR_ELISE[] = {
  { N_E7, 150 }, { N_DS7, 150 }, { N_E7, 150 }, { N_DS7, 150 }, { N_E7, 150 }, { N_B6, 150 }, { N_D7, 150 }, { N_C7, 150 },
  { N_A6, 300 }, { N_C6, 150 }, { N_E6, 150 }, { N_A6, 150 },
  { N_B6, 300 }, { N_E6, 150 }, { N_GS6, 150 }, { N_B6, 150 },
  { N_C7, 300 }, { 0, 150 },
  { N_E7, 150 }, { N_DS7, 150 }, { N_E7, 150 }, { N_DS7, 150 }, { N_E7, 150 }, { N_B6, 150 }, { N_D7, 150 }, { N_C7, 150 },
  { N_A6, 450 },
};

void Buzzer::begin() {
  ledcAttach(PIN_BUZZER, BEEP_FREQ_HZ, RES_BITS);
  freq_ = BEEP_FREQ_HZ;
  ledcWrite(PIN_BUZZER, 0);
}

void Buzzer::sound(bool on) {
  on_ = on;
  ledcWrite(PIN_BUZZER, on ? VOLUME_DUTY[settings.volume] : 0);  // up to a 50 % square wave, or silent
}

void Buzzer::setFreq(uint16_t hz) {
  if (hz == freq_) return;
  ledcChangeFrequency(PIN_BUZZER, hz, RES_BITS);
  freq_ = hz;
}

void Buzzer::beep(uint8_t count, uint16_t onMs, uint16_t offMs) {
  notes_ = nullptr;  // a beep cuts a melody short
  sound(false);
  setFreq(BEEP_FREQ_HZ);
  remaining_ = count;
  onMs_ = onMs;
  offMs_ = offMs;
  active_ = count > 0;
  phaseStart_ = millis() - offMs;  // start the first beep on the next update()
}

void Buzzer::melody(const Note *notes, uint8_t count) {
  sound(false);
  remaining_ = 0;
  notes_ = notes;
  noteCount_ = count;
  noteIndex_ = 0;
  phase_ = Phase::Gap;
  active_ = count > 0;
  phaseStart_ = millis() - NOTE_GAP_MS;  // first note on the next update()
}

void Buzzer::update(uint32_t now) {
  if (!active_) return;
  uint32_t elapsed = now - phaseStart_;
  if (notes_) {  // melody: a note (or rest), then a short gap, then the next note
    if (phase_ == Phase::Note) {
      if (elapsed >= (uint32_t)notes_[noteIndex_].ms - NOTE_GAP_MS) {
        sound(false);
        phase_ = Phase::Gap;
        phaseStart_ = now;
        noteIndex_++;
      }
    } else if (elapsed >= NOTE_GAP_MS) {
      if (noteIndex_ >= noteCount_) {
        notes_ = nullptr;
        active_ = false;
        setFreq(BEEP_FREQ_HZ);
        return;
      }
      const Note &n = notes_[noteIndex_];
      if (n.hz) {
        setFreq(n.hz);
        sound(true);
      }
      phase_ = Phase::Note;
      phaseStart_ = now;
    }
    return;
  }
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

void Buzzer::playEndMelody() {
  melody(FUR_ELISE, sizeof(FUR_ELISE) / sizeof(FUR_ELISE[0]));
}

void Buzzer::done() {
  if (eggs.melody) playEndMelody();
  else beep(3, 1000, 150);
}
