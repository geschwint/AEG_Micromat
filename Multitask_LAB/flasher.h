#include "api/Common.h"
#include "api/Compat.h"

#ifndef FLASHER_H
#define FLASHER_H

class Flasher {
  // Class member variables
  // These are initialized at startup
  int ledPin;    // What pin the led is connected to
  long onTime;   // Milliseconds of on-time
  long offTime;  // Milliseconds of on-time

  // These maintain the currect state
  PinStatus ledState;
  unsigned long previousMillis;  // Will store the last time LED was updated

  public:
  Flasher(int pin, long on, long off);
  void Update();

  private:
};
#endif
