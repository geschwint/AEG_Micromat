#include "door.h"
#include "config.h"

bool Door::simulated() const { return SIMULATE_DOOR; }

void Door::begin() {
  if (SIMULATE_DOOR) {
    closed_ = true;  // simulated door starts closed
    return;
  }
  pinMode(PIN_DOOR, INPUT_PULLUP);
  lastRaw_ = digitalRead(PIN_DOOR) == DOOR_CLOSED_LEVEL;
  closed_ = lastRaw_;
  rawSince_ = millis();
}

void Door::simulate(bool closed) {
  if (SIMULATE_DOOR) closed_ = closed;
}

void Door::update(uint32_t now) {
  if (SIMULATE_DOOR) return;
  bool raw = digitalRead(PIN_DOOR) == DOOR_CLOSED_LEVEL;
  if (raw != lastRaw_) {
    lastRaw_ = raw;
    rawSince_ = now;
  } else if (raw != closed_ && now - rawSince_ >= DOOR_DEBOUNCE_MS) {
    closed_ = raw;
  }
}
