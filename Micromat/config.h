// Pin map, hardware options and timing constants (see HARDWARE.md / PLAN.md).
#pragma once
#include <Arduino.h>

// --- Outputs ----------------------------------------------------------------
constexpr uint8_t PIN_MAGNETRON    = 17;  // K1: original power relay via transistor (onboard pull-down)
// Bench: a single relay card stands in for K1. Set true if that card switches on LOW.
constexpr bool MAGNETRON_ACTIVE_LOW = false;
constexpr uint8_t PIN_FAN          = 18;  // K2: VMA400 channel
constexpr uint8_t PIN_TURNTABLE    = 8;   // K3: VMA400 channel
constexpr uint8_t PIN_LAMP         = 9;   // K4: VMA400 channel
constexpr uint8_t PIN_SPARE        = 10;  // K5: VMA400 channel, unused, held OFF
constexpr uint8_t PIN_LED_ROTATE   = 43;  // panel LED "rotation active"
constexpr uint8_t PIN_LED_MAXPOWER = 44;  // panel LED "max power"
constexpr uint8_t PIN_BUZZER       = 15;  // via NPN + flyback diode

// --- Inputs -----------------------------------------------------------------
constexpr uint8_t PIN_DOOR   = 14;  // pull-up; level when closed = DOOR_CLOSED_LEVEL
constexpr uint8_t PIN_LADDER = 4;   // ADC1: panel buttons on a resistor ladder (10k pull-up)
constexpr uint8_t PIN_POT    = 2;   // ADC1: power potentiometer
constexpr uint8_t PIN_ENC_A  = 11;
constexpr uint8_t PIN_ENC_B  = 12;
constexpr uint8_t PIN_ENC_SW = 13;  // encoder push, to GND

// Ladder windows in mV, between measured button voltages. Contact resistance
// varies (worn carbon pads / return wire): first readings Stop 287, Quick start
// 809, Program 1574, Rotate 2268; later Stop 375-562, Quick start 890-1017,
// Program 1718-1724, Rotate 2311-2370; none 3149. With the first windows
// (Stop < 550) a Stop at 560 mV was taken as Quick start. A third run: Stop
// 317-614, Quick start 1003-1085 (still rising).
constexpr uint16_t LADDER_STOP_MAX    = 800;
constexpr uint16_t LADDER_QUICK_MAX   = 1350;
constexpr uint16_t LADDER_PROGRAM_MAX = 2000;
constexpr uint16_t LADDER_ROTATE_MAX  = 2700;

constexpr uint32_t INPUT_POLL_MS       = 10;
constexpr uint8_t  INPUT_STABLE_COUNT  = 3;   // identical polls before a button change counts
// Carbon contacts close gradually, so a press sweeps down through the windows
// of higher keys (Rotate -> Program -> Quick start -> Stop). A ladder key acts
// only after its reading has settled this long; the lowest key seen wins.
constexpr uint32_t LADDER_SETTLE_MS    = 60;
// Encoder type: quadrature steps (A/B edges) per click. The bench encoder gives
// one pulse per click = half an A/B cycle = 2 steps (measured); the oven's
// 2-bit quadrature encoder gives 2 pulses per click = a full cycle = 4 steps.
constexpr int8_t   ENC_STEPS_ONE_PULSE  = 2;  // bench encoder
constexpr int8_t   ENC_STEPS_TWO_PULSES = 4;  // the oven's encoder
constexpr int8_t   ENC_STEPS_PER_DETENT = ENC_STEPS_TWO_PULSES;
constexpr int8_t   ENC_DIRECTION        = 1;  // set to -1 if clockwise decreases the time
// Encoder acceleration when setting the time: the turning speed (clicks per
// second, smoothed) gives a multiplier for the steps per click. Slow turning
// is always one step per click.
constexpr uint16_t ENC_ACCEL_CPS[]  = { 8, 14, 22 };  // speed thresholds
constexpr uint8_t  ENC_ACCEL_MULT[] = { 2, 3, 5 };    // steps per click above each threshold
constexpr uint32_t ENC_ACCEL_RESET_MS = 200;          // a pause this long starts over at one step
constexpr uint16_t POT_HYSTERESIS_MV    = 80;

// --- Onboard (Waveshare ESP32-S3-Touch-LCD-2) ---------------------------------
constexpr uint8_t PIN_LCD_SCLK = 39;
constexpr uint8_t PIN_LCD_MOSI = 38;
constexpr uint8_t PIN_LCD_MISO = 40;
constexpr uint8_t PIN_LCD_DC   = 42;
constexpr uint8_t PIN_LCD_CS   = 45;
constexpr uint8_t PIN_LCD_BL   = 1;
constexpr uint8_t PIN_SD_CS    = 41;  // microSD card, on the LCD's SPI bus
constexpr uint8_t PIN_I2C_SDA  = 48;  // touch 0x15, IMU 0x6B, RTC 0x51
constexpr uint8_t PIN_I2C_SCL  = 47;

constexpr int16_t SCREEN_W = 320;  // landscape
constexpr int16_t SCREEN_H = 240;

// --- Hardware present ---------------------------------------------------------
// true: door state comes from the service screen/serial instead of PIN_DOOR.
// Preferred wiring: switch connects PIN_DOOR to GND when closed (DOOR_CLOSED_LEVEL = LOW),
// so an open/broken wire reads as OPEN. Bench wiring is currently HIGH when closed.
constexpr uint8_t DOOR_CLOSED_LEVEL = HIGH;
constexpr bool SIMULATE_DOOR = false;

// --- Cooking ------------------------------------------------------------------
constexpr uint32_t MAX_COOK_SECONDS = 9 * 3600 + 59 * 60 + 59;  // 9:59:59 (one-digit hour)
// Watts, as the original oven's knob (manual), highest first. 0 W = timer
// only: counts down and beeps, no relays at all.
constexpr uint16_t POWER_LEVELS[] = { 750, 700, 600, 500, 450, 350, 250, 150, 80, 0 };
constexpr uint8_t NUM_POWER_LEVELS = sizeof(POWER_LEVELS) / sizeof(POWER_LEVELS[0]);
constexpr uint16_t MAX_POWER_W = POWER_LEVELS[0];
// Magnetron power is on/off only: watts / MAX_POWER_W = share of this period with the magnetron on.
constexpr uint32_t DUTY_PERIOD_MS = 24000;

// The level closest to `watts` (ties: the higher one).
inline uint16_t nearestPowerW(int32_t watts) {
  uint16_t best = POWER_LEVELS[0];
  for (uint16_t w : POWER_LEVELS) {
    if (abs(w - watts) < abs(best - watts)) best = w;
  }
  return best;
}

// --- Safety / timing ----------------------------------------------------------
constexpr uint32_t FAN_LEAD_MS     = 1000;    // fan must run this long before the magnetron may start
// Fan run-on (as the original oven): only after heavy use, i.e. when heating
// ends and at least FAN_RUNON_MIN_HEAT_S of the last FAN_RUNON_WINDOW_S were
// spent heating (e.g. the second bag of popcorn in a row, not the first).
constexpr uint32_t FAN_RUNON_MS         = 300000;  // fan keeps cooling this long
constexpr uint16_t FAN_RUNON_WINDOW_S   = 240;
constexpr uint16_t FAN_RUNON_MIN_HEAT_S = 180;
constexpr uint32_t LAMP_DOOR_MS    = 300000;  // lamp turns off if the door is left open this long
constexpr uint32_t DOOR_DEBOUNCE_MS = 30;

// --- User interface -------------------------------------------------------------
// Brightness, dimmed level and dim delay are user settings (settings.h).
constexpr uint32_t MENU_TIMEOUT_MS  = 30000;   // menu closes after 30 s without use
constexpr uint32_t SERVICE_HOLD_MS  = 5000;    // hold Program this long for the service screen
constexpr uint32_t TOAST_MS         = 2000;    // short on-screen messages
constexpr uint32_t ZERO_TO_CLOCK_MS = 10000;   // timer at 00:00 this long -> clock view
constexpr uint32_t SPLASH_MS = 5000;           // start-up splash (logo, model, firmware); any input ends it
constexpr uint32_t POWER_VIEW_MS    = 1000;    // big watts on the main screen after the knob last moved
constexpr uint32_t END_BIG_MS       = 10000;   // large "End" this long -> clock view (small "End")
constexpr uint32_t PAUSE_BLINK_MS   = 1000;    // paused: the remaining time blinks, this period...
constexpr uint32_t PAUSE_BLINK_ON_MS = 600;    // ...shown this long of it (starts shown; restarts on a change)
constexpr uint32_t TOUCH_POLL_MS    = 30;
constexpr int32_t BIG_DIGIT_PX      = 77;      // big digit font size (Tiny TTF)
// Easter eggs (eggs.h): door open, hold Rotate at least MIN, release before MAX.
constexpr uint32_t EGG_HOLD_MIN_MS  = 10000;
constexpr uint32_t EGG_HOLD_MAX_MS  = 14000;

// --- Debug log on the SD card (debug_log.h) -----------------------------------
constexpr uint32_t LOG_FLUSH_MS    = 5000;             // buffered text goes to the card this often
constexpr size_t   LOG_BUF_BYTES   = 32768;            // RAM buffer (PSRAM)
constexpr size_t   LOG_CHUNK_BYTES = 4096;             // at most this much per loop() pass
constexpr size_t   LOG_FILE_MAX    = 1024UL * 1024;    // then the file rolls over (two are kept)
constexpr uint32_t LOG_SD_HZ       = 20000000;         // SPI clock for the card

// --- Sound --------------------------------------------------------------------
constexpr uint16_t BEEP_FREQ_HZ = 2700;  // buzzer is loudest around 2.5-3 kHz

// --- Time zone ----------------------------------------------------------------
// The RTC holds UTC; this POSIX rule gives Swedish local time with DST.
#define LOCAL_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
