// Minimal CST816D touch driver for Waveshare ESP32-S3-Touch-LCD-2 (from Test_Touch).
// Maps raw touch to landscape screen coordinates (GFX rotation 1).
#pragma once
#include <Arduino.h>
#include <Wire.h>

struct TouchPoint {
  bool pressed;
  uint8_t gesture;
  uint16_t rawX, rawY;  // panel-native (portrait) coordinates
  int16_t x, y;         // screen coordinates after mapping
};

// Initialise I2C and the controller; screenW/H are the landscape screen size.
bool touchBegin(TwoWire &wire, int sda, int scl, int16_t screenW, int16_t screenH);
// Poll once (call every ~20 ms). Returns false if the controller did not answer.
bool touchRead(TouchPoint &t);
