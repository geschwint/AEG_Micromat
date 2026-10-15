// Non-blocking buzzer: start a beep pattern or a melody and it plays in the
// background while update() is called from loop(). Volume comes from Settings,
// except for the done signal (beeps or melody), which is always at full volume.
#pragma once
#include <Arduino.h>

struct Note {
  uint16_t hz;  // 0 = rest
  uint16_t ms;
};

class Buzzer {
public:
  void begin();
  void beep(uint8_t count, uint16_t onMs = 200, uint16_t offMs = 150);
  void melody(const Note *notes, uint8_t count);  // `notes` must stay valid (static)
  void click();                             // key feedback (off if Settings: key clicks off)
  void error() { beep(1, 600, 0); }         // action refused
  void done();                              // cooking finished: three 1 s beeps, or the melody egg (full volume)
  void playEndMelody();                     // the melody egg's tune (also as a preview; full volume)
  void update(uint32_t now);

private:
  void sound(bool on);
  void setFreq(uint16_t hz);

  uint8_t remaining_ = 0;  // beeps still to start
  bool on_ = false;
  bool active_ = false;
  uint16_t onMs_ = 0, offMs_ = 0;
  uint32_t phaseStart_ = 0;
  // Melody: one note after another, each followed by a short gap so repeated notes are heard.
  const Note *notes_ = nullptr;
  uint8_t noteCount_ = 0, noteIndex_ = 0;
  enum class Phase : uint8_t { Note, Gap } phase_ = Phase::Gap;
  uint16_t freq_ = 0;
  bool full_ = false;  // the done signal: full volume whatever Settings says
};
