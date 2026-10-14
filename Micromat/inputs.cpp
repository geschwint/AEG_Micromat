#include "inputs.h"
#include "debug_log.h"
#include "config.h"

// --- Encoder (interrupt) -------------------------------------------------------
// A transition table turns each valid AB change into -1/+1; bounce and invalid
// jumps give 0.
static volatile int32_t encSteps = 0;
static volatile uint8_t encState = 0;

static void IRAM_ATTR onEncoder() {
  static const int8_t TABLE[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
  uint8_t ab = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  encState = ((encState << 2) | ab) & 0x0F;
  encSteps += TABLE[encState];
}

const char *Inputs::keyName(PanelKey k) {
  switch (k) {
    case PanelKey::None:       return "none";
    case PanelKey::Stop:       return "Stop";
    case PanelKey::QuickStart: return "Quick start";
    case PanelKey::Program:    return "Program";
    case PanelKey::Rotate:     return "Rotate";
  }
  return "?";
}

void Inputs::begin(InputHandler handler) {
  handler_ = handler;
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  encState = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(PIN_ENC_A, onEncoder, CHANGE);
  attachInterrupt(PIN_ENC_B, onEncoder, CHANGE);
  analogReadResolution(12);
}

void Inputs::emit(InputEvent e, int32_t value) {
  if (handler_) handler_(e, value);
}

int32_t Inputs::encoderSteps() const {
  noInterrupts();
  int32_t steps = encSteps;
  interrupts();
  return steps;
}

PanelKey Inputs::decodeLadder(uint16_t mv) const {
  if (mv < LADDER_STOP_MAX) return PanelKey::Stop;
  if (mv < LADDER_QUICK_MAX) return PanelKey::QuickStart;
  if (mv < LADDER_PROGRAM_MAX) return PanelKey::Program;
  if (mv < LADDER_ROTATE_MAX) return PanelKey::Rotate;
  return PanelKey::None;
}

// Act on a ladder key press (Program only starts its hold timer; it acts on release).
void Inputs::pressKey(PanelKey k, uint32_t now, bool corrected) {
  Log.printf("[%7lu] KEY %s (%u mV)%s\n", now, keyName(k), pressMinMv_, corrected ? " - corrected" : "");
  pressKey_ = k;
  switch (k) {
    case PanelKey::Stop:       emit(InputEvent::Stop); break;
    case PanelKey::QuickStart: emit(InputEvent::QuickStart); break;
    case PanelKey::Rotate:
      rotateDownAt_ = now;
      emit(InputEvent::Rotate);
      break;
    case PanelKey::Program:
      programDownAt_ = now;
      holdFired_ = false;
      break;
    default: break;
  }
}

// Pot travel split into equal bands, lowest band = lowest power.
uint8_t Inputs::potLevel(uint16_t mv) const {
  const uint16_t full = 3150;
  uint8_t band = (uint32_t)min(mv, (uint16_t)(full - 1)) * NUM_POWER_LEVELS / full;  // 0 = left
  return NUM_POWER_LEVELS - 1 - band;  // POWER_LEVELS[0] is the highest, so invert
}

void Inputs::update(uint32_t now) {
  if (now - lastPoll_ < INPUT_POLL_MS) return;
  lastPoll_ = now;

  // Ladder buttons. Stop/Quick start/Rotate act on press; Program acts on
  // release, unless it was held long enough to become ProgramHold.
  // Contact resistance only ever raises the voltage, so the real key is the
  // lowest one seen during a press (PanelKey order = rising voltage). A press
  // acts once it has settled for LADDER_SETTLE_MS (Stop at once: nothing is
  // lower). Should a lower key still show up later in the same press, it acts
  // too, so Stop always gets through.
  ladderMv_ = analogReadMilliVolts(PIN_LADDER);
  PanelKey k = decodeLadder(ladderMv_);
  if (k != PanelKey::None && (pressMinMv_ == 0 || ladderMv_ < pressMinMv_)) pressMinMv_ = ladderMv_;
  if (k == keyCandidate_) {
    if (keyCount_ < INPUT_STABLE_COUNT) keyCount_++;
  } else {
    keyCandidate_ = k;
    keyCount_ = 1;
  }
  if (keyCount_ == INPUT_STABLE_COUNT && keyCandidate_ != keyStable_) {
    keyStable_ = keyCandidate_;
    if (keyStable_ == PanelKey::None) {  // released
      if (pressKey_ == PanelKey::None && pending_ != PanelKey::None) pressKey(pending_, now, false);  // short tap
      if (pressKey_ == PanelKey::Program && !holdFired_) emit(InputEvent::Program);
      if (pressKey_ == PanelKey::Rotate) {
        uint32_t held = now - rotateDownAt_;
        if (held >= EGG_HOLD_MIN_MS) Log.printf("[%7lu] KEY Rotate held %lu ms\n", now, (unsigned long)held);
        if (held >= EGG_HOLD_MIN_MS && held < EGG_HOLD_MAX_MS) emit(InputEvent::RotateLong);
      }
      Log.printf("[%7lu] KEY released (lowest %u mV)\n", now, pressMinMv_);
      pending_ = pressKey_ = PanelKey::None;
      pressMinMv_ = 0;
    } else if (pressKey_ == PanelKey::None) {  // settling: keep the lowest
      if (pending_ == PanelKey::None) pendingSince_ = now;
      if (pending_ == PanelKey::None || keyStable_ < pending_) pending_ = keyStable_;
    } else if (keyStable_ < pressKey_) {  // a lower key after acting: that's the real one
      pressKey(keyStable_, now, true);
    }
  }
  if (pressKey_ == PanelKey::None && pending_ != PanelKey::None &&
      (pending_ == PanelKey::Stop || now - pendingSince_ >= LADDER_SETTLE_MS)) {
    pressKey(pending_, now, false);
  }
  if (pressKey_ == PanelKey::Program && !holdFired_ && now - programDownAt_ >= SERVICE_HOLD_MS) {
    holdFired_ = true;
    emit(InputEvent::ProgramHold);
  }

  // Encoder push
  bool sw = digitalRead(PIN_ENC_SW) == LOW;
  if (sw == swCandidate_) {
    if (swCount_ < INPUT_STABLE_COUNT) swCount_++;
  } else {
    swCandidate_ = sw;
    swCount_ = 1;
  }
  if (swCount_ == INPUT_STABLE_COUNT && swCandidate_ != swStable_) {
    swStable_ = swCandidate_;
    if (swStable_) {
      Log.printf("[%7lu] KEY encoder push\n", now);
      emit(InputEvent::EncoderPush);
    }
  }

  // Encoder rotation: whole clicks only; leftover steps carry over.
  int32_t clicks = (encoderSteps() - encUsed_) / ENC_STEPS_PER_DETENT;
  if (clicks != 0) {
    encUsed_ += clicks * ENC_STEPS_PER_DETENT;
    emit(InputEvent::EncoderTurn, clicks * ENC_DIRECTION);
  }

  // Pot: report only when it moves into another band (with hysteresis), so the
  // power isn't overridden while the knob is still.
  potMv_ = analogReadMilliVolts(PIN_POT);
  uint8_t lvl = potLevel(potMv_);
  bool first = potLevelIdx_ < 0;
  if (first || (lvl != potLevelIdx_ && abs((int)potMv_ - (int)potLevelMv_) > POT_HYSTERESIS_MV)) {
    potLevelIdx_ = lvl;
    potLevelMv_ = potMv_;
    if (!first) Log.printf("[%7lu] POT %u mV -> power %u W\n", now, potMv_, POWER_LEVELS[lvl]);
    emit(InputEvent::PotPower, POWER_LEVELS[lvl]);
  }
}

void Inputs::printRaw() const {
  Log.printf("          inputs: ladder %u mV (%s), pot %u mV, encoder %ld steps, push %s\n", ladderMv_,
                keyName(decodeLadder(ladderMv_)), potMv_, (long)encoderSteps(),
                digitalRead(PIN_ENC_SW) == LOW ? "pressed" : "released");
}
