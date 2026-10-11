# AEG Micromat 143 — controller replacement plan

## Background

The original control board (PIC MCU + relay + SCRs) is faulty: only Quick Start (+30 s, full power) and Stop respond; the countdown display works. Discrete parts measure OK, so the PIC likely has dead I/O. The plan is to replace the control electronics with a new MCU board, a display, and relays only. The HV section (transformer, capacitor, diode, magnetron) is assumed good, since Quick Start heats.

No grill or convection heater.

## Decisions so far

| Topic | Decision |
|---|---|
| Switching | Relays only (no SCRs/triacs). Lamp may use a solid-state relay (undecided). |
| Door switches | The board carrying the door microswitches stays and is reused as-is. |
| Inputs | Rotary encoder (time), potentiometer (power), original panel buttons on a resistor ladder (Stop, Quick start, Program, Rotate; time-set button dropped → menu) |
| Indicators | Original panel LEDs: rotation active, max power |
| UI | Menu system on a display; touch used for settings and extras |
| MCU (leading candidate) | Waveshare ESP32-S3-Touch-LCD-2 (SKU 29668) — final choice still open |

### Candidate board: Waveshare ESP32-S3-Touch-LCD-2

ESP32-S3R8, 16 MB flash, 8 MB PSRAM, Wi-Fi/BLE. 2" 240×320 IPS, ST7789T3 (SPI), CST816D capacitive touch (I2C). QMI8658 IMU, TF card slot, camera connector, LiPo charger, USB-C. ~22 GPIOs exposed.
Arduino: board "ESP32S3 Dev Module" (`esp32:esp32:esp32s3`), USB CDC on boot enabled, esp32 core ≥ 3.0. Waveshare's examples use LVGL 8.4.0, GFX Library for Arduino 1.5.0, FastIMU, and their `bsp_cst816` touch driver.
Not on board: microphone, speaker/buzzer, RTC battery. These need adding if wanted.

## Safety principles

1. **Door interlocks stay in hardware.** Primary, secondary and monitor switches remain wired in the mains path exactly as original. The MCU only *reads* the door state; it is never the thing that stops the magnetron when the door opens.
2. **Thermal cutoffs and mains fuse stay.**
3. **Fail-safe outputs.** Relays are off when undriven (no pull-ups toward ON), relay pins avoid ESP32-S3 strapping pins and pins that glitch at boot, and outputs are set to OFF as the first thing in `setup()`.
4. **Watchdog.** The task watchdog resets the MCU if the control loop stalls; on reset, everything starts OFF.
5. **Magnetron needs the fan.** Software never runs the magnetron without the fan, and after heavy use (at least 3 of the last 4 minutes heating) the fan keeps cooling for 5 minutes once cooking ends, like the original oven (no run-on after a single short cook).
6. **Independent max-on-time** (optional, worth considering): a simple hardware timer/monostable in the magnetron relay coil path, so a software bug can't heat indefinitely.
7. **No empty-cavity testing.** Magnetron bring-up always with a cup of water inside.

## Hardware architecture

```
 230 V mains ──fuse──┬── door interlock chain (original, untouched) ──┬── K1  magnetron (HV transformer primary)
                     │                                                ├── K2  fan
                     │                                                ├── K3  turntable motor
                     │                                                └── K4  lamp (relay or SSR)
                     └── isolated 5 V PSU (e.g. HLK-PM05) ── ESP32 board (USB-C 5 V in)

 ESP32 ── GPIO ── driver (opto/transistor relay module) ── K1..K4 coils
 ESP32 ◄── door sense (opto-isolated, or a spare low-voltage contact on the door switch board)
 ESP32 ◄── encoder A/B/SW, buttons, pot (ADC)
 ESP32 ──► buzzer (LEDC PWM)          ESP32 ◄── I2S MEMS mic (popcorn, optional)
```

Notes:
- **K1 (magnetron)** switches a large inductive load with high inrush. Use a relay rated well above the oven's input current (≥ 16 A, inductive or motor rated). It cycles many times per cook at reduced power, so contact life matters.
- **Relay drive at 3.3 V:** many "5 V relay modules" need more than a 3.3 V GPIO can give, especially active-low ones with an opto input. Verify each module, or drive coils via transistor/ULN2003.
- **Door sense:** need a way to read door state without touching interlock wiring. Options: an extra microswitch, a spare contact on the existing board, or an opto (e.g. H11AA1) sensing mains after the interlock chain.

## Software architecture

- **State machine:** `IDLE → SETTING → COOKING ⇄ PAUSED (door opened / Stop) → DONE → IDLE`, with `COOLDOWN` (fan only) after cooking.
- **Power levels:** a magnetron can only be on or off, so power is set by duty cycle over a slow period (24 s). This keeps relay switching to a few times per minute. **Levels in watts, as the original oven's manual: 750/700/600/500/450/350/250/150/80/0 W** (decided 2026-10-11); the on-share is watts / 750, e.g. 450 W = 14.4 s on / 9.6 s off. **0 W = timer only** (as on the original oven): it counts down and beeps, with no relays on. Power is shown in watts everywhere; no percentages.
- **Output layer:** one module owns all relay pins and enforces interlocks: magnetron only if door closed and fan on; everything OFF on door open (except lamp).
- **Non-blocking:** all tasks run on `millis()` timing (same pattern as `Multitask_LAB`); UI rendering (LVGL) doesn't block control logic. Could use a separate FreeRTOS task for control on the S3.
- **UI:** LVGL screens: main (time/power/start), menu (defrost, presets, settings), clock.

## GUI requirements

320×240 landscape, dark background, large digits (as the current test layout). All UI strings go through one table per language: English, Svenska, Deutsch, Français, Español (Settings → Language). Translations are kept short enough for the screen (checked with the console command `texts N`).

**Operation:** physical controls only (encoder, pot, panel buttons). **No touch buttons on the main screens.** Touch is reserved for special functions, and **used in the menu where it's easier**: tapping a row selects it (alternative to encoder turn + push). Future choices between few options (e.g. yes/no) get large touch buttons. Touch targets are at least 40 px because touch accuracy is about ±15 px near the edges.

### Screens

| Screen | Content |
|---|---|
| **Start-up splash** | 5 s after power-up: the AEG logo filling the width on a white background, the oven model (AEG Micromat 143E/U) and "Firmware 0.9.0 · 2026-10-11" below it. The encoder or a touch ends it early; panel buttons end it and act as usual. |
| **Main, clock view** (idle with nothing set, or done) | Current time **HH:MM large in the centre**, nothing in the upper left. Shown at boot, after the timer has shown 00:00 for 10 s, and 10 s after a large green **"End"**; then a small green "End" stays under the clock until acknowledged (Stop, door, new time). Stop or the door during the large "End" switches to the clock at once. Power bar and magnetron icon as below. |
| **Main, timer view** (time set / cooking / paused) | Current time **HH:MM, small, upper left**. Remaining/set time **extra large** as `MM:SS`, or `H:MM:SS` when an hour or more is set. **Power as a bar along the bottom, labelled in watts** ("600 W"; "Timer" at 0 W). **Turning the power knob** shows the watts in place of the time, in large orange digits ("600 W"); 1 s after the last change the usual view returns (the clock, or the set/remaining time); any other control (encoder, button, touch) brings it back at once. **Small magnetron icon** in two versions: plain = magnetron on, crossed out = off (shows the duty cycling at partial power). No rotate indicator (panel LED does that), no door indicator. **Program icon** left of the magnetron icon while a program from the menu is loaded: snowflake = defrost, cloche = any preset (shared for now), star = favourite; gone when the program ends or is cleared. |
| **Door open** | **"OPEN"** in large characters (replaces the time while the door is open). |
| **Dimmed (sleep)** | After **2 min without use**: backlight dimmed; the **AEG logo in red at the top** (full width, much larger than the digits) and the current time **HH:MM in large grey digits at the bottom** (same size as before). |
| **Program menu** | Opened by the Program button. **Encoder browses the list, encoder push selects**, or tap a row; Stop or tapping the title goes back. Root: Defrost, Programs, Settings, About. |
| **Defrost** | Food type (meat, poultry, fish, bread) → weight editor (100–2000 g) showing time and power → loads the program; beep + "Turn the food" halfway. |
| **Programs** | Fixed presets (reheat plate, beverage, popcorn, soften butter, melt chocolate), 4 favourites, "Save as favourite" (stores the current time/power/rotate). Selecting loads the program; push/Quick start starts it. |
| **Settings** (in the menu) | Clock (set time HH:MM; auto time over Wi-Fi/NTP), brightness, dimmed level, dim delay, sound (key clicks, volume), language (English, Svenska, Deutsch, Français, Español), Wi-Fi, Debug log, Firmware update. Saved in flash. |
| **Debug log** (Settings → Debug log) | **Logging On/Off**: everything the serial console shows is also written to a file on a microSD card, each line with the date and time; it rolls over at 1 MB, two files kept. **Download**: the same hotspot screen as Firmware update; after the password the page lists the log files (download) and has "Delete logs". No card: "No SD card", and the oven works as normal. |
| **About** (last in the root menu) | Read-only list, scrolled with the encoder: firmware version and date, build fingerprint and slot; the oven's rating plate (AEG Micromat 143E/U, C71A000V ELC 03, E-Nr. 947.602 432, S-No 417 10918; photo `AboutTheMicro.jpg`); board (Waveshare ESP32-S3-Touch-LCD-2), chip and revision, CPU, flash, PSRAM, display, touch, clock chip (PCF85063A), relays (Velleman VMA400, magnetron on the original relay), LVGL and Arduino core versions. Fixed texts in `Micromat/about.h`. |
| **Wi-Fi** (Settings → Wi-Fi) | **Wi-Fi On/Off** (master switch, off by default: the radio is never started when off), **Network** (opens the setup screen), **Forget network**. No credentials are ever built into the firmware. |
| **Firmware update** (Settings → Firmware update) | Only when the oven is idle; nothing can start cooking while it's open. Same screen layout as Wi-Fi setup: hotspot QR code, name, password, address, status ("Receiving… 45 %", "Installed, restarting…"), the running version. Upload the `.bin` from a browser (enter the password on screen) or with `arduino-cli` over the network. Works without the eFuse key via the oven's own hotspot; over the home network once one is set up. The new firmware must run normally for 30 s, otherwise the board falls back to the previous one. |
| **Wi-Fi setup** | The oven opens a temporary network `Micromat-XXXX` with a new random password, shown as text and as a **QR code** (scan to join). The phone opens a page (captive portal, or 192.168.4.1) listing the networks in range; pick one, type the password. The oven tests it and saves it only if it connects. Closes on leaving the screen, after success, or after 5 min idle. The stored password is **encrypted** with a key derived from a read-protected eFuse key on the chip (created once with `keygen YES` over serial; permanent). **That key is not created yet, so no password can be saved and Wi-Fi isn't usable until it is.** Risks: `HARDWARE.md`, "eFuse key for the Wi-Fi password". |
| **Service** | Output indicators, raw inputs, door simulation, loop timing, and any other debug/support functions. |

### Dimming
- "Use" = any input: encoder, encoder push, panel button, pot, touch, **door opened or closed**. Any use wakes the display.
- *Proposed:* no dimming while Cooking or Paused; the 2 min count starts in Idle/Done.
- *Proposed:* when dimmed, a turn/push of the encoder or a touch **only wakes** the display; Stop/Quick start/Rotate/Program act immediately (as well as waking it).

### Proposed details (to confirm)
- Max cook time raised from 99:59 to **9:59:59** (one-digit hour).
- **Encoder steps** (decided 2026-10-11), by the time already set: 0–2 min **5 s**, 2–5 min **10 s**, 5–10 min **20 s**, 10–20 min **30 s**, 20 min–1 h **1 min**, above 1 h **5 min**. With **acceleration**: turning fast takes several steps per click (up to 5); slow turning is always one step. Thresholds checked on the hardware 2026-10-11: feel right as they are.
- Menu exit: Stop button = back/exit; the menu also closes after 30 s without use.
- Service screen reached through a hidden gesture, e.g. **holding Program for 5 s**.

### To evaluate later
- Night mode (e.g. dimmer, red/amber digits at night, based on the RTC time).
- **Individual preset icons:** each preset gets its own program icon (plate, cup, popcorn, butter, chocolate) instead of the shared cloche. The user will supply a popcorn icon example; draw the others in the same line style (34×30 px, `makeProgIcon` in `ui.cpp`).
- **Easter eggs:** the user wants a few hidden surprises; what they are is not decided yet. A later phase. They must never change how the oven heats, never get in the way of normal use, and never start anything without a deliberate action.

## Features (prioritised)

1. **Core:** timer, power level, start/stop/pause, door handling, end beep, cool-down fan
2. **Clock:** battery-backed RTC (PCF85063A, UTC) keeps time through power cuts; optional NTP over Wi-Fi to correct drift
3. **Defrost:** weight/type-based programs (duty-cycle profiles)
4. **Presets / favourites**
5. **Popcorn auto-stop:** I2S mic detects pops; stop when pop rate drops. Fan and magnetron noise will be a challenge. Alternatively try the onboard IMU to sense pop vibration.
6. **Extras:** Wi-Fi status page / app, notifications, settings on touch screen

## Phases

1. **Bench tests (no mains):** board bring-up, one test sketch per peripheral (below)
2. **Control logic, simulated:** full state machine with LEDs in place of relays, door switch as a button, logging over serial. **In progress: `Micromat/`.** Verified over serial: fan lead, 60 % duty timing, door pause, start refused with door open, resume, quick start, Done + beeps, 60 s fan run-on. Panel ladder buttons, encoder + push, pot, panel LEDs and the VMA400 relays verified with the real hardware. Bench door switch (GPIO14) and a stand-in magnetron relay card (GPIO17) fitted and verified, as are the new GUI, the menu (encoder + tap) and Quick start resuming a pause without adding time. Still to come: menu contents (defrost, programs, settings)
3. **Hardware integration:** PSU, relay board, door sense, panel mounting; wiring diagram from the oven
4. **Staged bring-up in the oven:** lamp/fan/turntable first with magnetron disconnected → short magnetron runs with water load
5. **Features:** defrost, clock, presets, popcorn, Wi-Fi

## Planned test sketches

| Sketch | Purpose |
|---|---|
| `Test_Display` ✅ | ST7789 text/colour test on the Waveshare board (Arduino_GFX; LVGL still to try) |
| `Test_LVGL` ✅ | LVGL 9.5 mock oven panel: 6 touch buttons (96×56), simulated countdown/pause/power |
| `Test_Touch` ✅ | CST816D driver, landscape mapping + calibration; ±15 px at the edges, so UI touch targets ≥ 50 px |
| `Test_Inputs` | Encoder, buttons, pot read and debounced |
| `Test_Relays` ✅ | VMA400 on GPIO18/8/9/10 (unloaded): all four switch reliably from 3.3 V; no glitches during upload or reset |
| `Test_RTC` ✅ | PCF85063A at 0x51: holds UTC, local CET/CEST via TZ rule; set from PC over serial to within ~30 ms. Battery hold-over verified (power cycle, OS flag stays clear) |
| `Test_Buzzer` ✅ | GPIO15 via transistor + flyback diode; loudest ~2.5–3 kHz; "done" = 3 × 200 ms at 2700 Hz (LEDC tone) |
| `Test_Inputs` ✅ | Raw ladder/pot mV and encoder steps; used to set the ladder windows |
| `Test_Mic` | I2S mic level / pop detection (later) |

## Open questions

- Inside photos and wiring diagram of the oven (relay count, how fan/turntable/lamp are wired today)
- What exactly is on the board that stays (door switches only? anything else?)
- Relay modules on hand (coil voltage, active high/low, opto, ratings)
- Keep the pot, or set power by encoder + display?
- How the display/controls physically mount in the front panel
- SSR or relay for the lamp
