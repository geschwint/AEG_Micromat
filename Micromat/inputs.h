// Physical panel inputs: resistor-ladder buttons, rotary encoder (+ push) and
// power pot. update() polls every INPUT_POLL_MS and reports *events* to a
// handler; what an event means depends on the screen and is decided in App.
// The encoder is decoded in a pin-change interrupt.
#pragma once
#include <Arduino.h>

enum class PanelKey : uint8_t { None, Stop, QuickStart, Program, Rotate };

enum class InputEvent : uint8_t {
  Stop,         // on press
  QuickStart,   // on press
  Rotate,       // on press
  Program,      // on release, if released before SERVICE_HOLD_MS
  ProgramHold,  // Program held for SERVICE_HOLD_MS
  EncoderTurn,  // value = clicks, positive = clockwise
  EncoderPush,  // on press
  PotPower,     // value = power in watts; sent when the pot moves into another band (and once at start)
};

using InputHandler = void (*)(InputEvent event, int32_t value);

class Inputs {
public:
  void begin(InputHandler handler);
  void update(uint32_t now);
  void printRaw() const;  // diagnostics for the serial console
  static const char *keyName(PanelKey k);

  // Raw state for the service screen
  uint16_t ladderMv() const { return ladderMv_; }
  uint16_t potMv() const { return potMv_; }
  PanelKey key() const { return keyStable_; }
  bool pushPressed() const { return swStable_; }
  int32_t encoderSteps() const;

private:
  PanelKey decodeLadder(uint16_t mv) const;
  uint8_t potLevel(uint16_t mv) const;
  void emit(InputEvent e, int32_t value = 0);
  void pressKey(PanelKey k, uint32_t now, bool corrected);

  InputHandler handler_ = nullptr;
  uint32_t lastPoll_ = 0;

  uint16_t ladderMv_ = 0;
  PanelKey keyStable_ = PanelKey::None, keyCandidate_ = PanelKey::None;
  uint8_t keyCount_ = 0;
  PanelKey pending_ = PanelKey::None;  // lowest key seen while settling, not yet acted on
  PanelKey pressKey_ = PanelKey::None; // key acted on for the current press
  uint32_t pendingSince_ = 0;
  uint16_t pressMinMv_ = 0;            // lowest reading during the press (diagnostics)
  uint32_t programDownAt_ = 0;
  bool holdFired_ = false;

  bool swStable_ = false, swCandidate_ = false;  // true = pressed
  uint8_t swCount_ = 0;

  int32_t encUsed_ = 0;  // quadrature steps already turned into clicks

  uint16_t potMv_ = 0;
  int8_t potLevelIdx_ = -1;  // power level index the pot currently selects
  uint16_t potLevelMv_ = 0;  // pot reading when that level was last reported
};
