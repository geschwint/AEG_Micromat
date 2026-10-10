# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Replacement controller for an AEG Micromat 143 microwave oven: an MCU board with a display drives relays (magnetron, fan, turntable, lamp) and reads an encoder, buttons and a pot. See `PLAN.md` for decisions, safety principles, architecture and phases, and `HARDWARE.md` for free pins, the proposed pin map and the parts list. Keep both updated as decisions are made; new sketches should use the pin map from `HARDWARE.md`.

Each subfolder is a separate sketch (folder name must match its `.ino` file, per Arduino convention). `Multitask_LAB/` is non-blocking LED flashing used as a base for running several timed tasks at once.

Safety rule for all code: door interlocks stay in hardware; software never runs the magnetron without the fan, and all relay outputs must default OFF (at boot, on reset, on door open).

## Target board

The final MCU is not chosen yet; the leading candidate is the Waveshare ESP32-S3-Touch-LCD-2 (details in `PLAN.md`). `Test_*` sketches target it.

### Waveshare ESP32-S3-Touch-LCD-2

- Usually on `COM5`. esp32 core pinned to **3.3.11** (stable index URL); display via "GFX Library for Arduino" 1.6.9.
- FQBN (16 MB flash, octal PSRAM, USB CDC on boot so Serial works over USB):
  `esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB`
- Compile/upload: `arduino-cli compile --fqbn <FQBN> Test_Display` then `arduino-cli upload -p COM5 --fqbn <FQBN> Test_Display`
- Firmware update over Wi-Fi (`Micromat` only; Settings → Firmware update on the oven shows the password):
  - Build the image: `arduino-cli compile --fqbn <FQBN> --export-binaries Micromat` → `Micromat/build/esp32.esp32.esp32s3/Micromat.ino.bin` (git-ignored).
  - Browser: join `Micromat-XXXX` (QR code), open http://192.168.4.1 (or the home-network address shown), enter the password, upload the `.bin`. **Verified 2026-10-10 from an Android phone** (10 s transfer, restart into the other slot, confirmed after 30 s). A phone's captive-portal mini-browser may not open a file picker; then use the normal browser (and turn off mobile data if it can't reach the page).
  - Network upload (espota, untested so far): `arduino-cli upload -p 192.168.4.1 --protocol network --fqbn <FQBN> --upload-field password=<password> Micromat`.
- If the port vanishes right after connecting (firmware without USB CDC), enter download mode: hold BOOT, tap RESET, release BOOT.
- Onboard pin usage (from Waveshare's demo package) — do not reuse for relays/inputs:
  LCD SPI SCLK 39, MOSI 38, MISO 40, DC 42, CS 45, backlight 1 (no LCD reset); I2C SDA 48 / SCL 47 (CST816D touch + QMI8658 IMU; also on the header); SD card CS 41 (shares LCD SPI); battery ADC 5; touch INT 46; IMU INT1 3; BOOT 0. Free header pins are listed in `HARDWARE.md`.
- Factory firmware and Waveshare examples: `https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-2/ESP32-S3-Touch-LCD-2-Demo.zip` (`Firmware/ESP32-S3-Touch-LCD-2-factory.bin`).
- UI uses **LVGL 9.5.0** (not Waveshare's 8.4 examples). Its config is `Documents\Arduino\libraries\lv_conf.h` (shared by all sketches), made from `lv_conf_template.h` with these changes: enabled, C-library malloc/string/sprintf (uses the heap/PSRAM instead of a 64 KB pool), Montserrat 20/28/48 fonts on, default font 20, Tiny TTF on (used by `Micromat` for all fonts), QR code on (Wi-Fi setup). `Test_LVGL` shows the glue: `lv_tick_set_cb(millis)`, a partial 40-line buffer flushed via `gfx->draw16bitRGBBitmap`, and a pointer input device reading `touch_cst816`.
- Touch (CST816D, I2C 0x15, chip ID 0xB6): Waveshare's `bsp_cst816` library folder in the demo zip is empty, so we have our own minimal driver, packaged as `Test_LVGL/touch_cst816.{h,cpp}` — reuse that (`Test_Touch` has the calibration UI). Key facts:
  - Poll 6 bytes from reg 0x01 (gesture, fingers, XH, XL, YH, YL; 12-bit coords). Write 0x01 to reg 0xFE to disable auto-sleep, otherwise it NACKs when idle. No INT/RST pins are used.
  - The first samples after touch-down are stale (e.g. raw 64,0 / 64,1025): ignore the first 2 pressed samples.
  - Landscape (GFX rotation 1): u = rawY (left→right), v = 239 − rawX (top→bottom), then a bilinear correction fitted to averaged corner taps (`DEFAULT_CAL_RAW`). The panel is non-uniform near the bottom-left. Accuracy ≈ ±15 px at the edges and ±8 px in the centre, so touch targets should be ≥ 50 px.
- Other I2C devices: 0x6B = QMI8658 IMU; 0x51 = PCF85063A RTC (Soldered 333051, on the header I2C); 0x7E responds too (unidentified).
- RTC: `Test_RTC` has a minimal PCF85063A driver (BCD time regs 0x04–0x0A, bit 7 of 0x04 = oscillator-stopped flag, STOP bit 0x20 in reg 0x00). **The RTC stores UTC**; local time comes from the POSIX TZ rule `CET-1CEST,M3.5.0,M10.5.0/3` (handles DST). To set it from the PC, run `Test_RTC/set_rtc_time.ps1` while `Test_RTC` is running. The chip's first tick after a set comes ~0.508 s later, and the script compensates for that.

### Arduino UNO R4 WiFi

`Multitask_LAB` currently targets:

- Arduino UNO R4 WiFi: FQBN `arduino:renesas_uno:unor4wifi`, usually on `COM4` (confirm with `arduino-cli board list`; ports change when boards are swapped).
- The code uses ArduinoCore-API headers (`api/Common.h`, `api/Compat.h`) and the `PinStatus` enum, so it only builds on ArduinoCore-API-based cores like the R4's. It does **not** compile for the classic AVR Uno (`arduino:avr:uno`).

## Commands

Run from this directory; pass the sketch folder as the path.

- Compile: `arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi Multitask_LAB`
- Upload: `arduino-cli upload -p COM4 --fqbn arduino:renesas_uno:unor4wifi Multitask_LAB`
- Read serial non-interactively (`arduino-cli monitor` never exits), e.g. 5 s at 115200 baud:
  `$p = New-Object System.IO.Ports.SerialPort 'COM4',115200; $p.DtrEnable = $true; $p.Open(); Start-Sleep 5; $p.ReadExisting(); $p.Close()`

There are no tests; verify by compiling, uploading, and checking the hardware or serial output.

## Architecture

### `Micromat/`: the controller (main sketch)

Non-blocking throughout: **no `delay()` in the control path.** Each module has `update(now)`, and `loop()` calls them in order: door → oven → outputs → buzzer → clock → console → UI (`lv_timer_handler`). A loop watchdog (`enableLoopWDT()`, 5 s) restarts the board, with all outputs OFF, if `loop()` stalls.

- `config.h`: pin map (from `HARDWARE.md`), hardware-present flags (`SIMULATE_DOOR`), timing constants (24 s duty period, 1 s fan lead, fan run-on 5 min after heavy use (≥ 3 of the last 4 min heating), 5 min door lamp timeout), TZ rule.
- `oven.*`: the state machine (Idle / Cooking / Paused / Done). Owns cook time, power level and rotate; every `update()` turns state into output **requests**. Power % = magnetron on-share of each 24 s period, with the cycle starting after the fan lead. Levels 100/80/60/40/20/**0 %**; 0 % = timer only (countdown + done beeps, every relay off). **Power follows the pot**: Stop/Quick start never change it; a program from the menu sets its own power, and `App` restores the pot's level when that program ends. Quick start: with a time set or paused it starts as set; while cooking +30 s; with nothing set 30 s. When "heating" (cooking with power > 0) ends, including power turned to 0 % mid-cook, the fan runs on for 5 min **only after heavy use**: at least 3 of the last 4 min spent heating (1 bit per second ring buffer, `recentHeatSeconds()`, shown on the service screen). As the original oven: one bag of popcorn, no run-on; the second in a row, run-on.
- `outputs.*`: the only code that writes relay/LED pins (the panel LEDs are requested off in `loop()` while the display is dimmed). Enforces interlocks regardless of requests: magnetron only with door closed and fan on for ≥ `FAN_LEAD_MS`; magnetron written before fan; all LOW first in `begin()`.
- `door.*`: debounced door input on GPIO14 (pull-up). `DOOR_CLOSED_LEVEL` sets the level read when closed: preferred is LOW (switch to GND when closed, so a broken wire reads open); the bench wiring is currently HIGH when closed. `SIMULATE_DOOR = true` swaps it for a simulated door (service-screen tap / serial `open`/`close`); it's `false` now that a bench door switch is fitted.
- `buzzer.*`: non-blocking beep patterns (LEDC, 2700 Hz): `click()` (skipped if key clicks are off), `error()`, `done()`. Volume = LEDC duty from Settings.
- `clock.*`: PCF85063A RTC (UTC) → local time. Accepts the same `T YYYY-MM-DD HH:MM:SS` command as `Test_RTC`, so `set_rtc_time.ps1` works with this sketch too. `setLocalTime(h, m)` (menu) and `setUtcEpoch()` (NTP).
- `settings.*`: user settings + 4 favourites in NVS (`Preferences`, one blob under key `s2`). **When `Settings` changes layout, bump the key and carry the old blob over** (as `migrateV1()` does from `s1`), so favourites aren't lost. Global `settings`; `saveSettings()` only after a user change. `wifiOn` is the Wi-Fi master switch (default off).
- `wifi_creds.*`: the Wi-Fi network, NVS namespace `wifi`. The password is **encrypted** (AES-256-GCM, network name as associated data); the AES key is derived by the chip's HMAC hardware from a read-protected **eFuse key** (purpose HMAC_UP), so it's never in flash or firmware. The eFuse key is burned once per chip with the serial command `keygen YES` (permanent; uses one of six key blocks). Without it passwords are refused, never stored in plain text. **The key has not been created (the user's decision); never run `keygen YES` or any `espefuse burn_*` without explicit approval.** Risks, limits and status are in `HARDWARE.md` ("eFuse key for the Wi-Fi password"). `credsBegin()` sets `WiFi.persistent(false)` and erases the Wi-Fi driver's own plain-text copy (`nvs.net80211`). Never call `WiFi.begin()` before `credsBegin()`.
- `hotspot.*`: the oven's temporary network `Micromat-XXXX` (AP+STA, new random password each start, `WIFI:` QR text, captive DNS answering every name), shared by Wi-Fi setup and firmware update (one at a time); also the shared HTML helpers. Each screen has its own `WebServer` on port 80, only one running.
- `fw_update.*`: Settings → Firmware update, only when the oven is Idle/Done; nothing can start cooking while the screen is open. Opens the hotspot, also joins the home network if one is set up, and accepts a new image (the `.bin` from `arduino-cli compile --export-binaries`) via a browser upload page (a password form, not HTTP Basic auth, which captive-portal mini-browsers can't show; a correct password gives a one-time upload token in the form address) or ArduinoOTA/espota (port 3232). **The password is the hotspot's, random each time and shown on screen.** The transfer blocks `loop()` for 10–40 s: the loop watchdog is paused and `pumpDuringUpdate()` (in the `.ino`) keeps door/oven/outputs/buzzer/UI running. The image goes into the other OTA slot (`app0`/`app1`, partition scheme `app3M_fat9M_16MB`); the board restarts 3 s after a good check. **Rollback:** `verifyRollbackLater()` returns true, so a new image boots "on trial"; `confirmIfHealthy()` marks it valid after 30 s, and a crash or hang before that makes the next reset go back to the old one. Closes after 10 min without activity. The firmware version is a build fingerprint, the first 8 hex digits of the image's ELF SHA-256 (boot log, service screen, update screen and page). `__DATE__`/`__TIME__` were misleading: they only change when that one file is recompiled.
- `wifi_setup.*`: Settings → Wi-Fi → Network. Opens the hotspot and a captive-portal page at 192.168.4.1 listing the networks in range; the chosen one is tested (AP+STA) and saved only if it connects. Closes on leaving the screen, 20 s after success, or 5 min without activity. Web handlers are registered once. Global `wifiSetup`; `WifiTime` waits while it's active.
- `programs.h`: defrost types (power, seconds per 100 g; 100–2000 g) and fixed presets. Values are starting points to tune.
- `wifi_time.*`: auto clock over Wi-Fi/NTP when Wi-Fi is on, Settings → Clock → Auto time is on and a network is set. Syncs at start and daily, writes the RTC, then turns Wi-Fi off; retries after 10 min on failure.
- `ui.*`: renders six views (main, dimmed, menu, editor, service, and a hotspot view with a QR code shared by Wi-Fi setup and firmware update, per the GUI requirements in `PLAN.md`), hidden/shown as `App` reports. Refreshes every 33 ms. Icons (magnetron on/off; program icon for defrost/preset/favourite left of it) are drawn with `lv_line`s in 34×30 px boxes. **All fonts are Montserrat Medium via LVGL Tiny TTF** (`LV_USE_TINY_TTF 1` in `lv_conf.h`; the TTF is embedded as `font_montserrat_ttf.h`, ~240 KB flash) at 14/20/28 px and `BIG_DIGIT_PX` (77 px), because the built-in fonts are ASCII only (Swedish needs å/ä/ö) and stop at 48 px. Each falls back to the built-in font of its size for `LV_SYMBOL_*` icons. A time change redraws in 20–34 ms; the earlier `transform_scale` on a 48 px label took up to 100 ms. Backlight PWM on GPIO1, level from `App::backlightPercent()` (setting, dimmed level, or live preview in the menu). **Only call LVGL setters when the value really changes** (`setText`/`setColor` helpers compare first): any setter invalidates the object even with the same value, and the redraw blocks `loop()` for 13–80 ms. Doing that unconditionally caused visible input lag (84 ms stalls 10×/s); now idle passes take < 2 ms.
- `perf.h`: loop timing statistics; console `perf` prints the slowest pass and the count over 20 ms since the last call, then resets. Use it to check any lag.
- `Serial.setTxTimeoutMs(0)`: with no reader on the PC, USB serial writes would otherwise wait up to 100 ms each, so log text is dropped instead.
- `inputs.*`: panel inputs, polled every 10 ms; reports **events** (`InputEvent`) to a handler and never acts on the oven itself. Ladder keys (GPIO4; windows in `config.h`): the carbon contacts close gradually, so a press sweeps down through higher keys' windows; a key acts after settling `LADDER_SETTLE_MS` (60 ms; Stop at once), the **lowest key seen wins**, and a lower key appearing later in the same press acts too (logged "corrected"), so Stop always gets through. Stop/Quick start/Rotate fire on press; **Program fires on release**, or `ProgramHold` after 5 s held. Encoder (interrupt-decoded, 2 steps per click) → `EncoderTurn` with clicks; push → `EncoderPush`. Pot → `PotPower` with a percent, only when it moves into another of the 6 bands (full left = 0 % timer), plus once at start.
- `app.*`: maps events to actions per screen (`Screen::Main/Menu/Edit/Service/WifiSetup/Update`). Main: encoder sets time (10/15/30/60 s steps, 5 min above 1 h; max 9:59:59), push = start, Stop/Quick start/Rotate → oven, Program → menu (refused while cooking). Menu (`MenuId`, structure in the `MENUS` table: parent + row): encoder browses, push selects, **tapping a row selects it** (4 rows × 42 px, geometry `MENU_ROW_*` in `app.h` shared with the UI), Stop/Program or a tap on the title = back; closes after 30 s idle, on door change or on Done. Choice lists (`MenuId` ≥ `Brightness`) check the current value; picking saves and goes back. Editors (`EditId`): defrost weight, clock HH:MM. Programs (defrost/preset/favourite) only **load** time/power/rotate and return to Main; the user starts them. Defrost beeps "turn the food" halfway. Service (hold Program 5 s): debug view; Stop/Program/push exits. Pot sets power on every screen. Also owns dimming (Settings delay, only Main + Idle/Done; encoder/touch while dimmed only wake) and touch polling (LVGL has no input device).
- `ui_text.*`: every on-screen string via `tr(Str::…)` from the English or Swedish table (Settings → Language). Add a `Str` to both tables (static_asserts check the counts). Named `ui_text`, not `strings`, to avoid clashing with the C library's `<strings.h>`.
- `console.*`: serial commands for testing (`in` prints raw input readings): `+N`/`-N`, `t N`, `p N`, `start`, `stop`, `quick`, `rot`, `open`, `close`, `s` (status), `ssid NAME` then `pass PASSWORD` (Wi-Fi network, password encrypted), `forget`, `wifi` (status + sync now), `keygen` (explains) / `keygen YES` (burns the eFuse key). State changes and relay switching are logged with millisecond timestamps.
- Actions from UI callbacks run after `oven.update()` in the same pass. Anything that stores a timestamp used in the next `update()` must take `now` from `loop()` rather than calling `millis()` itself (see `doorChanged`).

### `Multitask_LAB/`

- Multitasking without `delay()`: each task is an object with its own timing state, and an `Update()` method that checks `millis()` and acts only when its interval has elapsed. `loop()` simply calls `Update()` on every task object, so tasks run independently. New tasks should follow the same pattern; a blocking `delay()` anywhere in `loop()` stalls all of them.
- `Flasher` (`flasher.h`/`.cpp`) toggles one pin with separate on/off durations. Instances are global objects in the `.ino` (currently pins 12 and 13; pin 13 is the built-in LED).
- `pinMode()` is called in the `Flasher` constructor, which runs for global objects before the core's own initialization. If pins misbehave, move pin setup into a `begin()` method called from `setup()`.
