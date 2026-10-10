#include "touch_cst816.h"

#define TOUCH_ADDR        0x15
#define REG_GESTURE       0x01
#define REG_DIS_AUTOSLEEP 0xFE

#define NATIVE_W 240  // panel is natively portrait 240x320
#define NATIVE_H 320

// The first samples after touch-down are often stale (raw 64,0 / 64,1025 ...),
// so a touch only counts as pressed after SETTLE_SAMPLES.
#define SETTLE_SAMPLES 2

#define CAL_INSET 20  // corner targets used for calibration, inset from edges

// Average raw readings at the four corner targets (TL, TR, BL, BR), measured
// with Test_Touch. Corrects the panel's non-uniformity near the bottom-left.
static const uint16_t DEFAULT_CAL_RAW[4][2] = { { 207, 21 }, { 198, 305 }, { 58, 20 }, { 30, 304 } };

static TwoWire *bus = nullptr;
static int16_t scrW = 320, scrH = 240;
// Bilinear correction: x,y = c0 + c1*u + c2*v + c3*u*v
static float calX[4] = { 0, 1, 0, 0 };
static float calY[4] = { 0, 0, 1, 0 };

static bool readRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
  bus->beginTransmission(TOUCH_ADDR);
  bus->write(reg);
  if (bus->endTransmission(false) != 0) return false;
  if (bus->requestFrom((uint8_t)TOUCH_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = bus->read();
  return true;
}

static bool writeReg(uint8_t reg, uint8_t val) {
  bus->beginTransmission(TOUCH_ADDR);
  bus->write(reg);
  bus->write(val);
  return bus->endTransmission() == 0;
}

// Landscape rotation: u = rawY runs left->right, v = (NATIVE_W-1) - rawX top->bottom.
static void rawToUV(uint16_t rawX, uint16_t rawY, float &u, float &v) {
  u = rawY;
  v = (NATIVE_W - 1) - rawX;
}

// Solve the 4x4 system A*c = b by Gaussian elimination with partial pivoting.
static bool solve4(float A[4][4], float b[4], float c[4]) {
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

static bool fitCalibration(const uint16_t raw[4][2]) {
  const int sx[4] = { CAL_INSET, scrW - 1 - CAL_INSET, CAL_INSET, scrW - 1 - CAL_INSET };
  const int sy[4] = { CAL_INSET, CAL_INSET, scrH - 1 - CAL_INSET, scrH - 1 - CAL_INSET };
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
  return true;
}

bool touchBegin(TwoWire &wire, int sda, int scl, int16_t screenW, int16_t screenH) {
  bus = &wire;
  scrW = screenW;
  scrH = screenH;
  bus->begin(sda, scl, 400000);
  fitCalibration(DEFAULT_CAL_RAW);
  // Keep the controller awake so polling never gets NACKs.
  return writeReg(REG_DIS_AUTOSLEEP, 0x01);
}

bool touchRead(TouchPoint &t) {
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
  if (t.pressed) {
    float u, v;
    rawToUV(t.rawX, t.rawY, u, v);
    t.x = constrain((int)lroundf(calX[0] + calX[1] * u + calX[2] * v + calX[3] * u * v), 0, scrW - 1);
    t.y = constrain((int)lroundf(calY[0] + calY[1] * u + calY[2] * v + calY[3] * u * v), 0, scrH - 1);
  }
  return true;
}
