#include "api/Common.h"
#include "flasher.h"

  Flasher::Flasher(int pin, long on, long off)
  {
    ledPin = pin;
    pinMode(ledPin, OUTPUT);

    onTime = on;
    offTime = off;

    ledState = PinStatus::LOW;
    previousMillis = 0;
  }

  void Flasher::Update()
  {
    unsigned long currentMillis = millis();

    if ((ledState == PinStatus::HIGH) && (currentMillis - previousMillis >= onTime))
    {
      ledState = PinStatus::LOW;
      previousMillis = currentMillis;
      digitalWrite(ledPin, ledState);
    }
    else if ((ledState == PinStatus::LOW) && (currentMillis - previousMillis >= offTime)) {
      ledState = PinStatus::LOW;
      previousMillis = currentMillis;
      digitalWrite(ledPin, ledState);    
    }
  }

