// Input diagnostics: resistor-ladder buttons (GPIO4), pot (GPIO2), encoder
// A/B/push (GPIO11/12/13). Prints a line whenever something changes, with
// calibrated millivolts for the analog inputs, so the ladder windows can be set
// from real readings.
#include <Arduino.h>

#define PIN_LADDER 4
#define PIN_POT    2
#define PIN_ENC_A  11
#define PIN_ENC_B  12
#define PIN_ENC_SW 13

// Quadrature decoding in the pin-change interrupt: a transition table turns
// each valid AB change into -1/+1 and ignores bounce/invalid jumps.
volatile int32_t encSteps = 0;  // quarter-steps (4 per detent on most encoders)
volatile uint8_t encState = 0;

void IRAM_ATTR onEncoder() {
  static const int8_t TABLE[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
  uint8_t ab = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  encState = ((encState << 2) | ab) & 0x0F;
  encSteps += TABLE[encState];
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Test_Inputs: press ladder buttons, turn/push encoder, turn pot");

  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  encState = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(PIN_ENC_A, onEncoder, CHANGE);
  attachInterrupt(PIN_ENC_B, onEncoder, CHANGE);

  analogReadResolution(12);  // default attenuation (11 dB) covers ~0-3.1 V
}

void loop() {
  static uint32_t lastPoll = 0, lastIdle = 0;
  static int ladderStable = -1, ladderCandidate = -1, ladderCount = 0;
  static int potReported = -1000;
  static int32_t encReported = 0;
  static int swReported = HIGH;

  uint32_t now = millis();
  if (now - lastPoll < 10) return;  // 100 Hz polling
  lastPoll = now;

  // Ladder: report a value once 3 consecutive readings agree within 40 mV.
  int ladder = analogReadMilliVolts(PIN_LADDER);
  if (ladderCandidate >= 0 && abs(ladder - ladderCandidate) <= 40) {
    if (++ladderCount == 3 && abs(ladder - ladderStable) > 40) {
      ladderStable = ladder;
      Serial.printf("[%7lu] LADDER %4d mV\n", now, ladder);
    }
  } else {
    ladderCandidate = ladder;
    ladderCount = 1;
  }

  // Pot: report changes over 40 mV (avoids noise spam).
  int pot = analogReadMilliVolts(PIN_POT);
  if (abs(pot - potReported) > 40) {
    potReported = pot;
    Serial.printf("[%7lu] POT    %4d mV\n", now, pot);
  }

  noInterrupts();
  int32_t steps = encSteps;
  interrupts();
  if (steps != encReported) {
    Serial.printf("[%7lu] ENC    %+ld quarter-steps (total %ld)\n", now, (long)(steps - encReported), (long)steps);
    encReported = steps;
  }

  int sw = digitalRead(PIN_ENC_SW);
  if (sw != swReported) {
    swReported = sw;
    Serial.printf("[%7lu] ENC-SW %s\n", now, sw == LOW ? "pressed" : "released");
  }

  if (now - lastIdle > 5000) {
    lastIdle = now;
    Serial.printf("[%7lu] idle: ladder %d mV, pot %d mV, A=%d B=%d SW=%d\n", now, ladder, pot,
                  digitalRead(PIN_ENC_A), digitalRead(PIN_ENC_B), sw);
  }
}
