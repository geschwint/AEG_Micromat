#include "oven.h"
#include "debug_log.h"
#include "config.h"

uint16_t Oven::power() const { return POWER_LEVELS[powerIdx_]; }

const char *Oven::stateName(OvenState s) {
  switch (s) {
    case OvenState::Idle:    return "Idle";
    case OvenState::Cooking: return "Cooking";
    case OvenState::Paused:  return "Paused";
    case OvenState::Done:    return "Done";
  }
  return "?";
}

void Oven::begin(Outputs &outputs, Buzzer &buzzer) {
  out_ = &outputs;
  buzzer_ = &buzzer;
  lastUpdate_ = millis();
}

void Oven::setState(OvenState s) {
  if (s == state_) return;
  uint32_t now = millis();
  if (s == OvenState::Cooking) cookStart_ = now;
  Log.printf("[%7lu] STATE %s -> %s  (%lu s left, %u W)\n", now, stateName(state_), stateName(s),
                (unsigned long)remainingSeconds(), power());
  state_ = s;
}

bool Oven::fanRunningOn() const {
  return runOn_ && (int32_t)(runOnUntil_ - millis()) > 0;
}

// Power control: the magnetron is on for watts / MAX_POWER_W of each DUTY_PERIOD_MS. The
// cycle starts after the fan lead time, so the first on-period isn't shortened.
bool Oven::magnetronWanted(uint32_t now) const {
  if (state_ != OvenState::Cooking || timerOnly()) return false;
  uint32_t since = now - cookStart_;
  if (since < FAN_LEAD_MS) return false;
  if (power() >= MAX_POWER_W) return true;
  uint32_t phase = (since - FAN_LEAD_MS) % DUTY_PERIOD_MS;
  return phase < DUTY_PERIOD_MS * power() / MAX_POWER_W;
}

void Oven::update(uint32_t now, bool doorClosed) {
  doorClosed_ = doorClosed;
  uint32_t elapsed = now - lastUpdate_;
  lastUpdate_ = now;

  if (state_ == OvenState::Cooking) {
    remainingMs_ = remainingMs_ > elapsed ? remainingMs_ - elapsed : 0;
    if (remainingMs_ == 0) {
      setState(OvenState::Done);
      buzzer_->done();
    }
  }
  // "Heating" = cooking with power > 0. When it ends for any reason (pause,
  // done, clear, or power turned to 0 W) the fan keeps cooling for FAN_RUNON_MS,
  // but only after heavy use (see oven.h).
  bool heating = state_ == OvenState::Cooking && !timerOnly();
  recordHeat(now, heating);
  if (heating) {
    runOn_ = false;
  } else if (heating_) {
    runOn_ = heatCount_ >= FAN_RUNON_MIN_HEAT_S;
    runOnUntil_ = now + FAN_RUNON_MS;
    Log.printf("[%7lu] heated %u s of the last %u s: %s\n", now, heatCount_, FAN_RUNON_WINDOW_S,
                  runOn_ ? "fan run-on" : "no fan run-on");
  }
  heating_ = heating;
  if (runOn_ && (int32_t)(runOnUntil_ - now) <= 0) {
    runOn_ = false;
    Log.printf("[%7lu] fan run-on finished\n", now);
  }

  bool doorLamp = !doorClosed_ && (now - doorOpenedAt_ < LAMP_DOOR_MS);
  out_->request(OUT_MAGNETRON, magnetronWanted(now));
  out_->request(OUT_FAN, heating || runOn_);
  out_->request(OUT_TURNTABLE, heating && rotate_);
  out_->request(OUT_LAMP, heating || doorLamp);
  out_->request(OUT_LED_ROTATE, rotate_);
  out_->request(OUT_LED_MAXPOWER, power() >= MAX_POWER_W);
}

// One bit per second in a ring buffer covering FAN_RUNON_WINDOW_S; heatCount_
// keeps the number of heating seconds in it. A second counts as heating if it
// was heating when that second was recorded.
void Oven::recordHeat(uint32_t now, bool heating) {
  uint32_t sec = now / 1000;
  if (heatSec_ == 0) heatSec_ = sec;  // first call
  uint32_t steps = min<uint32_t>(sec - heatSec_, FAN_RUNON_WINDOW_S);  // seconds to record (normally 0 or 1)
  heatSec_ = sec;
  for (uint32_t i = 0; i < steps; i++) {
    uint16_t idx = (sec - i) % FAN_RUNON_WINDOW_S;
    uint8_t &byte = heatHist_[idx / 8];
    uint8_t bit = 1 << (idx % 8);
    if (byte & bit) heatCount_--;
    if (heating) {
      byte |= bit;
      heatCount_++;
    } else {
      byte &= ~bit;
    }
  }
}

void Oven::addSeconds(int32_t seconds) {
  int32_t s = (int32_t)remainingSeconds() + seconds;
  setSeconds(s < 0 ? 0 : (uint32_t)s);
}

void Oven::setSeconds(uint32_t seconds) {
  if (seconds > MAX_COOK_SECONDS) seconds = MAX_COOK_SECONDS;
  // While cooking, keep the running sub-second part so the display doesn't jump.
  if (state_ == OvenState::Cooking) {
    int32_t delta = (int32_t)seconds - (int32_t)remainingSeconds();
    int64_t ms = (int64_t)remainingMs_ + (int64_t)delta * 1000;
    remainingMs_ = ms < 0 ? 0 : (uint32_t)ms;
  } else {
    remainingMs_ = seconds * 1000;
  }
  if (state_ == OvenState::Done) setState(OvenState::Idle);
  buzzer_->click();
}

void Oven::nextPower() {
  powerIdx_ = (powerIdx_ + 1) % NUM_POWER_LEVELS;
  Log.printf("[%7lu] power %u W\n", millis(), power());
  buzzer_->click();
}

void Oven::setPower(uint16_t watts, bool click) {
  // Pick the closest available level.
  uint8_t best = 0;
  for (uint8_t i = 1; i < NUM_POWER_LEVELS; i++) {
    if (abs((int)POWER_LEVELS[i] - watts) < abs((int)POWER_LEVELS[best] - watts)) best = i;
  }
  powerIdx_ = best;
  Log.printf("[%7lu] power %u W\n", millis(), power());
  if (click) buzzer_->click();
}

void Oven::start() {
  if (state_ == OvenState::Cooking) return;
  if (!doorClosed_ || remainingMs_ == 0) {
    Log.printf("[%7lu] start refused: %s\n", millis(), !doorClosed_ ? "door open" : "no time set");
    buzzer_->error();
    return;
  }
  buzzer_->click();
  setState(OvenState::Cooking);
}

void Oven::stop() {
  buzzer_->click();
  if (state_ == OvenState::Cooking) {
    setState(OvenState::Paused);
  } else {
    remainingMs_ = 0;
    setState(OvenState::Idle);
  }
}

// Paused, or Idle with a time set: start as set (same as encoder push).
// Cooking: +30 s. Nothing set (Idle 0 s / Done): 30 s. Power is never changed
// here: it follows the pot (or a program loaded from the menu).
void Oven::quickStart() {
  if (state_ == OvenState::Paused || (state_ == OvenState::Idle && remainingMs_ > 0)) {
    start();
    return;
  }
  addSeconds(30);
  if (state_ != OvenState::Cooking) start();
}

void Oven::toggleRotate() {
  rotate_ = !rotate_;
  Log.printf("[%7lu] rotate %s\n", millis(), rotate_ ? "on" : "off");
  buzzer_->click();
}

// `now` must be the same timestamp the following update() gets, so the lamp
// timeout (now - doorOpenedAt_) never goes negative.
void Oven::doorChanged(bool closed, uint32_t now) {
  doorClosed_ = closed;
  Log.printf("[%7lu] door %s\n", now, closed ? "closed" : "OPEN");
  if (!closed) {
    doorOpenedAt_ = now;
    if (state_ == OvenState::Cooking) setState(OvenState::Paused);
    else if (state_ == OvenState::Done) {
      remainingMs_ = 0;
      setState(OvenState::Idle);
    }
  }
}

void Oven::setRotate(bool on) {
  if (rotate_ != on) toggleRotate();
}
