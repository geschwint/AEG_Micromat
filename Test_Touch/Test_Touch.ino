// Touch bring-up test for Waveshare ESP32-S3-Touch-LCD-2 (CST816D capacitive touch, I2C)
// Minimal CST816 driver: poll registers 0x01..0x06 (gesture, finger count, X, Y).
// Maps raw touch to landscape screen coordinates (rotation 1) with a bilinear
// correction fitted to averaged corner readings (DEFAULT_CAL_RAW).
// Draws a dot where touched; long press clears, double tap runs a manual
// 4-corner calibration and prints the coefficients.
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1
#define LCD_BL   1

#define I2C_SDA  48
#define I2C_SCL  47
#define TOUCH_ADDR 0x15

// CST816 registers
#define REG_GESTURE       0x01
#define REG_CHIP_ID       0xA7
#define REG_FW_VERSION    0xA9
#define REG_DIS_AUTOSLEEP 0xFE

#define GESTURE_DOUBLE_TAP 0x0B
#define GESTURE_LONG_PRESS 0x0C

#define NATIVE_W 240  // panel is natively portrait 240x320
#define NATIVE_H 320

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 1 /* landscape */, true /* IPS */, NATIVE_W, NATIVE_H);

struct TouchPoint {
  bool pressed;
  uint8_t gesture;
  uint16_t rawX, rawY;  // panel-native (portrait) coordinates
  int16_t x, y;         // screen coordinates after mapping
};

// Rotation for landscape (found by testing): u = rawY runs left->right,
// v = (NATIVE_W-1) - rawX runs top->bottom. Calibration then corrects the
// remaining (non-uniform) error with x,y = c0 + c1*u + c2*v + c3*u*v.
// Defaults are the uncorrected 1:1 mapping.
float calX[4] = { 0, 1, 0, 0 };
float calY[4] = { 0, 0, 1, 0 };

bool readRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // NACK: chip asleep or absent
  if (Wire.requestFrom((uint8_t)TOUCH_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

void rawToUV(uint16_t rawX, uint16_t rawY, float &u, float &v) {
  u = rawY;
  v = (NATIVE_W - 1) - rawX;
}

int16_t evalBilinear(const float c[4], float u, float v) {
  return (int16_t)lroundf(c[0] + c[1] * u + c[2] * v + c[3] * u * v);
}

void mapToScreen(TouchPoint &t) {
  float u, v;
  rawToUV(t.rawX, t.rawY, u, v);
  t.x = constrain(evalBilinear(calX, u, v), 0, gfx->width() - 1);
  t.y = constrain(evalBilinear(calY, u, v), 0, gfx->height() - 1);
}

// The first samples after touch-down are often stale (seen as raw 64,0 / 64,1 /
// 64,1025 / 64,2048), so a touch only counts as pressed after SETTLE_SAMPLES.
#define SETTLE_SAMPLES 2

bool readTouch(TouchPoint &t) {
  static uint8_t pressedCount = 0;
  uint8_t d[6];
  if (!readRegs(REG_GESTURE, d, sizeof(d))) {
    t.pressed = false;
    t.gesture = 0;
    pressedCount = 0;
    return false;
  }
  t.gesture = d[0];
  t.rawX = ((d[2] & 0x0F) << 8) | d[3];
  t.rawY = ((d[4] & 0x0F) << 8) | d[5];
  bool finger = (d[1] & 0x0F) > 0;
  if (!finger) {
    pressedCount = 0;
  } else if (pressedCount < 255) {
    pressedCount++;
  }
  t.pressed = finger && pressedCount > SETTLE_SAMPLES && t.rawX < NATIVE_W && t.rawY < NATIVE_H;
  if (t.pressed) mapToScreen(t);
  return true;
}

// Solve the 4x4 system A*c = b by Gaussian elimination with partial pivoting.
bool solve4(float A[4][4], float b[4], float c[4]) {
  for (int col = 0; col < 4; col++) {
    int piv = col;
    for (int r = col + 1; r < 4; r++)
      if (fabsf(A[r][col]) > fabsf(A[piv][col])) piv = r;
    if (fabsf(A[piv][col]) < 1e-6f) return false;
    for (int k = 0; k < 4; k++) { float t = A[col][k]; A[col][k] = A[piv][k]; A[piv][k] = t; }
    float t = b[col]; b[col] = b[piv]; b[piv] = t;
    for (int r = col + 1; r < 4; r++) {
      float f = A[r][col] / A[col][col];
      for (int k = col; k < 4; k++) A[r][k] -= f * A[col][k];
      b[r] -= f * b[col];
    }
  }
  for (int r = 3; r >= 0; r--) {
    float s = b[r];
    for (int k = r + 1; k < 4; k++) s -= A[r][k] * c[k];
    c[r] = s / A[r][r];
  }
  return true;
}

// Wait for a full touch (press then release) and return the average raw point.
// Taps with fewer than 3 settled samples are ignored (too short to trust).
void waitTap(uint16_t &rx, uint16_t &ry) {
  TouchPoint t;
  uint32_t sumX = 0, sumY = 0, n = 0;
  while (true) {
    delay(20);
    readTouch(t);
    if (t.pressed) {
      sumX += t.rawX;
      sumY += t.rawY;
      n++;
    } else if (n > 0) {
      if (n >= 3) {
        rx = sumX / n;
        ry = sumY / n;
        return;
      }
      sumX = sumY = n = 0;  // too short: wait for another tap
    }
  }
}

void drawTarget(int x, int y, uint16_t color) {
  gfx->drawCircle(x, y, 10, color);
  gfx->drawFastHLine(x - 10, y, 20, color);
  gfx->drawFastVLine(x, y - 10, 20, color);
}

#define CAL_INSET 20  // corner targets are this far in from the screen edges

// Default calibration: average raw readings of 10-20 taps per corner target
// (TL, TR, BL, BR) collected during bring-up. Single taps scatter +/-20 px near
// the edges, so an averaged default is more accurate than a one-shot calibration.
const uint16_t DEFAULT_CAL_RAW[4][2] = { { 207, 21 }, { 198, 305 }, { 58, 20 }, { 30, 304 } };

// Fit the bilinear correction from raw readings at the four corner targets.
bool fitCalibration(const uint16_t raw[4][2]) {
  const int W = gfx->width(), H = gfx->height();
  const int sx[4] = { CAL_INSET, W - 1 - CAL_INSET, CAL_INSET, W - 1 - CAL_INSET };
  const int sy[4] = { CAL_INSET, CAL_INSET, H - 1 - CAL_INSET, H - 1 - CAL_INSET };
  float A[4][4], Ay[4][4], bx[4], by[4], cx[4], cy[4];
  for (int i = 0; i < 4; i++) {
    float u, v;
    rawToUV(raw[i][0], raw[i][1], u, v);
    A[i][0] = 1; A[i][1] = u; A[i][2] = v; A[i][3] = u * v;
    bx[i] = sx[i];
    by[i] = sy[i];
  }
  memcpy(Ay, A, sizeof(A));
  if (!solve4(A, bx, cx) || !solve4(Ay, by, cy)) return false;
  memcpy(calX, cx, sizeof(cx));
  memcpy(calY, cy, sizeof(cy));
  Serial.printf("cal calX = { %.4f, %.6f, %.6f, %.8f }\n", calX[0], calX[1], calX[2], calX[3]);
  Serial.printf("cal calY = { %.4f, %.6f, %.6f, %.8f }\n", calY[0], calY[1], calY[2], calY[3]);
  return true;
}

// Show the four corner targets one at a time and fit the bilinear correction.
void calibrate() {
  const int m = CAL_INSET;
  const int W = gfx->width(), H = gfx->height();
  const int sx[4] = { m, W - 1 - m, m, W - 1 - m };
  const int sy[4] = { m, m, H - 1 - m, H - 1 - m };
  const char *names[4] = { "top-left", "top-right", "bottom-left", "bottom-right" };
  uint16_t raw[4][2];

  for (int i = 0; i < 4; i++) {
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(60, 100);
    gfx->printf("Tap %s", names[i]);
    drawTarget(sx[i], sy[i], RGB565_YELLOW);

    uint16_t rx, ry;
    float u, v;
    while (true) {
      waitTap(rx, ry);
      rawToUV(rx, ry, u, v);
      // Sanity check: the tap must be in the right quarter of the panel.
      bool left = u < NATIVE_H / 2, top = v < NATIVE_W / 2;
      bool wantLeft = (i == 0 || i == 2), wantTop = (i < 2);
      if (left == wantLeft && top == wantTop) break;
      Serial.printf("cal %-12s rejected raw %3u,%3u (wrong area), tap again\n", names[i], rx, ry);
      gfx->setCursor(60, 130);
      gfx->setTextColor(RGB565_RED);
      gfx->print("Missed - tap again");
    }
    Serial.printf("cal %-12s screen %3d,%3d  raw %3u,%3u  uv %3.0f,%3.0f\n", names[i], sx[i], sy[i], rx, ry, u, v);
    raw[i][0] = rx;
    raw[i][1] = ry;
    delay(300);
  }

  if (!fitCalibration(raw)) {
    Serial.println("cal FAILED (taps too close together); retrying");
    calibrate();
  }
}

const char *gestureName(uint8_t g) {
  switch (g) {
    case 0x00: return "none";
    case 0x01: return "swipe up";
    case 0x02: return "swipe down";
    case 0x03: return "swipe left";
    case 0x04: return "swipe right";
    case 0x05: return "tap";
    case 0x0B: return "double tap";
    case 0x0C: return "long press";
    default:   return "?";
  }
}

void i2cScan() {
  Serial.println("I2C scan:");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf("  found 0x%02X\n", a);
  }
}

void drawTargets() {
  const int m = 20;
  const int w = gfx->width(), h = gfx->height();
  drawTarget(m, m, RGB565_YELLOW);
  drawTarget(w - 1 - m, m, RGB565_YELLOW);
  drawTarget(m, h - 1 - m, RGB565_YELLOW);
  drawTarget(w - 1 - m, h - 1 - m, RGB565_YELLOW);
  drawTarget(w / 2, h / 2, RGB565_YELLOW);
}

void clearScreen() {
  gfx->fillScreen(RGB565_BLACK);
  drawTargets();
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(40, 40);
  gfx->print("Long press: clear");
  gfx->setCursor(40, 60);
  gfx->print("Double tap: recal");
}

void showStatus(const TouchPoint &t) {
  gfx->fillRect(40, 160, 240, 40, RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(40, 160);
  gfx->printf("raw %3u,%3u", t.rawX, t.rawY);
  gfx->setCursor(40, 180);
  gfx->printf("scr %3d,%3d", t.x, t.y);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Test_Touch starting");

  gfx->begin();
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  i2cScan();

  uint8_t id = 0, fw = 0;
  if (readRegs(REG_CHIP_ID, &id, 1) && readRegs(REG_FW_VERSION, &fw, 1)) {
    Serial.printf("Touch chip ID 0x%02X, FW 0x%02X\n", id, fw);
  } else {
    Serial.println("Touch chip not responding (may be asleep until touched)");
  }
  // Keep the controller awake so polling never gets NACKs.
  Serial.printf("Disable auto-sleep: %s\n", writeReg(REG_DIS_AUTOSLEEP, 0x01) ? "ok" : "failed");

  fitCalibration(DEFAULT_CAL_RAW);
  clearScreen();
}

void loop() {
  static bool wasPressed = false;
  static uint8_t lastGesture = 0;
  static unsigned long lastPoll = 0;

  if (millis() - lastPoll < 20) return;  // poll at ~50 Hz
  lastPoll = millis();

  TouchPoint t;
  readTouch(t);

  if (t.pressed) {
    gfx->fillCircle(t.x, t.y, 3, RGB565_RED);
    showStatus(t);
    if (!wasPressed) {
      Serial.printf("down  raw %3u,%3u  scr %3d,%3d\n", t.rawX, t.rawY, t.x, t.y);
    }
  } else if (wasPressed) {
    Serial.println("up");
  }

  if (t.gesture != lastGesture && t.gesture != 0) {
    Serial.printf("gesture: %s (0x%02X)\n", gestureName(t.gesture), t.gesture);
    if (t.gesture == GESTURE_LONG_PRESS) clearScreen();
    if (t.gesture == GESTURE_DOUBLE_TAP) {
      calibrate();
      clearScreen();
    }
  }
  lastGesture = t.gesture;
  wasPressed = t.pressed;
}
