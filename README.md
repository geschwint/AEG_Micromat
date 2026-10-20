# AEG Micromat 143 – new controller

A replacement controller for an **AEG Micromat 143E/U** microwave oven (type C71A000V, ELC 03). The original control board (a PIC microcontroller with a relay and triacs) failed: only Quick start and Stop still responded. This project replaces it with an ESP32-S3 board with a 2" touch display. It drives the oven's relays and reads the original panel buttons, a rotary encoder and a power knob. The high-voltage section (transformer, capacitor, diode, magnetron) and the door safety switches stay original.

> ⚠️ **Safety.** A microwave oven contains lethal voltages, and the high-voltage capacitor can stay charged after unplugging. Microwave radiation is dangerous. The door interlock switches stay in the mains path exactly as original; the software only *reads* the door. Never run the magnetron with an empty cavity (test with a cup of water). The safety principles are in [PLAN.md](PLAN.md#safety-principles).

## Why

Why bring an old, broken oven back to life? It wasn't completely dead; it just no longer responded when I tried to set the time or the power. In the end it came down to two choices:

1. **New electronics and firmware.** Sounds fun, but a challenge: a quick search turned up no open-source firmware for it at all. The closest thing was a student project that controlled a microwave with a mix of hardware and software, partly inside and partly outside the oven. The original PIC firmware is proprietary, of course.
2. **A new kitchen.** My kitchen is a non-standard width and this is a built-in oven, so a replacement has to fit in 47 cm. Modern built-in microwaves are 50 or 60 cm wide, and a cheap $30 countertop model won't do. Fitting a modern one means remodelling the kitchen, so the "new microwave" would end up costing $5,000 or more.

To me the choice was easy: remod... no! New electronics and firmware. It's more fun, and the oven gets another 20+ years. If the electronics fail again, the fix is simple: get a new board, flash the firmware and off you go. And if I ever find a new microwave that fits my kitchen, I'll most likely rip out its electronics and put this controller in instead. Why are microwave user interfaces so bad, anyway?

## Status

**Version 1.3.0 (2026-10-20):** feature-complete on the bench, not yet installed in the oven. Next: take the oven apart for an inventory, then plan the installation (a power supply is already chosen). The panel buttons, encoder, pot, relays, door switch and display are verified. The magnetron relay is a stand-in on the bench. Firmware updates over Wi-Fi are verified. See [PLAN.md](PLAN.md#phases) for the phases.

## What it does

- **Cooking:** set the time with the encoder (it speeds up when turned fast; Quick start starts, the encoder push never does) and the power with the knob in the original oven's steps: 750, 700, 600, 500, 450, 350, 250, 150, 80 W, or 0 W = kitchen timer. Turning the knob shows the watts in large digits for a moment. Stop pauses, then clears. Quick start adds 30 s. Rotate switches the turntable.
- **Safety in software as well:** the magnetron only runs with the door closed and the fan running. All relays are off at power-up, on reset and when the door opens. A watchdog restarts the controller, with everything off, if the software hangs. The fan keeps cooling for 5 minutes after heavy use.
- **Display:** a 5-second start-up splash with the AEG logo, the oven model and the firmware version; then a large clock when idle, and the remaining time while cooking. It dims after a while to a sleep screen with the red AEG logo and the clock, and the panel LEDs turn off with it.
- **Menu** (Program button, or tap):
  - **Defrost** by food type and weight, with a "turn the food" beep halfway.
  - **Programs:** fixed presets plus four favourites.
  - **Settings:** clock, brightness, dimming, sound, language (English, Svenska, Deutsch, Français, Español), Wi-Fi, debug log, firmware update.
  - **About:** firmware version and hardware details.
- **Clock:** a battery-backed real-time clock keeps the time through power cuts. It can also be set over Wi-Fi.
- **Wi-Fi (optional, off by default):** set up from a phone via the oven's own temporary network and a QR code. Wi-Fi is only on briefly for the daily time sync, and while the firmware update or log download screen is open; that screen shows the oven's address on your home network ("On your network: …"). Menu → About shows the network, the IP address (or the last one used) and the MAC address. The Wi-Fi password is stored encrypted with a key from the chip's eFuses. That key has to be created once per board, and that step is permanent (done on the current board, 2026-10-11); see [HARDWARE.md](HARDWARE.md#efuse-key-for-the-wi-fi-password-created-2026-10-11).
- **Firmware update over Wi-Fi** from a browser, with automatic rollback if the new firmware doesn't start properly.
- **Debug log (optional):** with a microSD card in the board's slot, Settings → Debug log → Logging writes the log to the card with date and time. Download the files from a phone or PC via Settings → Debug log → Download (the oven's own network and a password, like a firmware update). The log is always in English, whatever language the oven is set to, and starts with the start-up lines (firmware, reset reason), useful after an unexpected restart. Tested with a 16 GB card.
- **Easter eggs:** a hidden menu with a few surprises, such as a melody when the time is up. How to find it is left as a puzzle (the code is open, so the answer is in there too).

## Hardware

| Part | Used |
|---|---|
| Controller | Waveshare ESP32-S3-Touch-LCD-2: ESP32-S3, 16 MB flash, 8 MB PSRAM, 2" 320×240 IPS display (ST7789), CST816D touch |
| Real-time clock | Soldered 333051 (PCF85063A), battery-backed |
| Relays | Velleman VMA400 4-channel (fan, turntable, lamp, spare); magnetron on the oven's original relay |
| Panel | Original buttons on a resistor ladder (Stop, Quick start, Program, Rotate), rotary encoder with push, power potentiometer, original LEDs, buzzer |

Pin map, wiring notes and parts list: [HARDWARE.md](HARDWARE.md).

## Repository

| Path | Contents |
|---|---|
| `Micromat/` | The controller firmware (main sketch) |
| `Test_*/` | Small sketches used to bring up each part: display, touch, LVGL, inputs, relays, buzzer, RTC |
| `Multitask_LAB/` | Early non-blocking multitasking example (Arduino UNO R4) |
| `tools/build.ps1` | Builds the firmware and saves a stamped image (see below) |
| `tools/make_logo.ps1` | Turns the logo image into `Micromat/logo_aeg.h` for the splash and sleep screens (run with Windows PowerShell 5.1) |
| `config/lv_conf.h` | Copy of the LVGL configuration (see "Setting up") |
| `PLAN.md` | Decisions, safety principles, architecture, screens, phases |
| `HARDWARE.md` | Pins, wiring, parts, measurements, eFuse key notes |
| `ORIGINAL_HARDWARE.md`, `Original Hardware/` | The oven's original boards, reverse-engineered from photos |
| `CHANGELOG.md` | What changed in each version, from the start of the project |
| `CLAUDE.md` | Detailed technical notes, kept for Claude Code but readable by anyone |
| `LICENSE` | GNU General Public License v3.0 (see "License") |
| `AboutTheMicro.jpg` | Photo of the oven's rating plate |
| `Logo-940x520.avif` | The AEG logo, source for `tools/make_logo.ps1` (not covered by the license; see "License") |

## Setting up (Windows)

1. Install [arduino-cli](https://arduino.github.io/arduino-cli/) and the ESP32 core **3.3.11**:
   ```powershell
   arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
   arduino-cli core update-index
   arduino-cli core install esp32:esp32@3.3.11
   ```
2. Install the libraries: **lvgl 9.5.0** and **GFX Library for Arduino 1.6.9**:
   `arduino-cli lib install lvgl@9.5.0 "GFX Library for Arduino@1.6.9"`
3. Copy `config/lv_conf.h` to `Documents\Arduino\libraries\lv_conf.h` (LVGL only looks for it there). If you change one copy later, copy it over the other.

## Building – always with the build script

**Always build the firmware with `tools\build.ps1`.** This goes for people and for Claude alike. The script compiles the firmware and saves the deployable image under a name that says exactly what it is:

```
firmware\Micromat_v1.0.0_2026-10-11_5cefa112.bin
                  version  build date fingerprint
```

- **Version:** `FIRMWARE_VERSION` in `Micromat/about.h`, `major.minor.build` (see [Versions and releases](#versions-and-releases)).
- **Build date:** the day the image was built.
- **Fingerprint:** unique per build. The oven shows the same fingerprint in Menu → About, on the service screen, on the update page and in the boot log, so you can always match the oven to an image.

```powershell
.\tools\build.ps1            # build and save the stamped image
.\tools\build.ps1 -Upload    # also install it over USB (COM5; -Port COMx for another port)
```

Images in `firmware\` are not committed to git.

## Versions and releases

**Always increase the version** with every change to the firmware that goes into `master`, so each version means exactly one state of the code. The version is `major.minor.build` (`FIRMWARE_VERSION` in `Micromat/about.h`); set `FIRMWARE_DATE` next to it to the day of the change.

| Change | Bump | Example |
|---|---|---|
| Bug fixes | build | 1.0.0 → 1.0.1 |
| New features | minor (build back to 0) | 1.0.1 → 1.1.0 |
| Major rework (decided case by case) | major (minor and build back to 0) | 1.1.0 → 2.0.0 |

A change with both fixes and features bumps the minor.

`master` is protected: every change goes in through a pull request from a `feature/...` or `bug/...` branch, and the version bump is part of that pull request. **Releases are branched off `master`** at the commit with the wanted version, named `releases/<version>` (e.g. `releases/1.0.0`), and that branch is what gets built and installed. **The latest release branch is kept at the tip of `master`:** after a documentation-only pull request is merged, the latest `releases/<version>` is moved forward to `master` (a fast-forward, no history rewritten; the firmware and its fingerprint are unchanged). So checking out the latest release always gives the current documentation and license too.

**Every pull request updates [CHANGELOG.md](CHANGELOG.md)** in the same pull request: under the new version's heading for a firmware change (Added / Changed / Fixed), or for documentation only under the latest version as "Documentation added after the release" (or under "Unreleased" while `master` holds firmware changes that aren't released yet). Write for someone using the oven: what changed and why it matters, not which file.

## Installing

- **Over USB:** `.\tools\build.ps1 -Upload`. If the board doesn't show up, hold BOOT, tap RESET, release BOOT.
- **Over Wi-Fi:**
  1. On the oven, open Settings → Firmware update (only when it's not cooking). The screen shows a QR code, a network name (`Micromat-XXXX`), a password and an address.
  2. Join that network with a phone or PC and open **http://192.168.4.1**.
  3. Enter the password and upload the stamped `.bin` from `firmware\`. On a phone, if the pop-up page can't open a file, use the normal browser; turn mobile data off if the page doesn't load.
  4. The oven restarts into the new firmware. If the new firmware doesn't run normally for 30 seconds, the oven goes back to the previous one at the next restart.

## Service and testing

- **Service screen:** hold Program for 5 seconds. It shows outputs, door, button voltages, encoder, pot, oven state, clock, loop timing, fan run-on, Wi-Fi and firmware.
- **Serial console** (USB, 115200 baud): send any unknown text for a list of commands, e.g. `s` for status, `in` for raw input readings or `log` for the SD card log.

## License

Copyright (C) 2026 Stefan Lindberg

This project is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License version 3** as published by the Free Software Foundation, or (at your option) any later version. It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE](LICENSE) for the full text.

In short: you may use, change and share this firmware, also commercially, but anyone who distributes it or a modified version must do so under the GPL v3 and make the source code available.

**Not covered by this license:**

- **The AEG name and logo** are trademarks of their owner (AEG / Electrolux). The logo files (`Logo-940x520.avif` and the generated `Micromat/logo_aeg.h`) are included only so this oven can show its own brand; no rights to them are granted. This project is not affiliated with or endorsed by AEG or Electrolux. If you build this for another oven or share a modified version, remove the logo or replace it with your own.
- **The Montserrat font** embedded in the firmware (`Micromat/font_montserrat_ttf.h`) is © The Montserrat Project Authors, under the [SIL Open Font License 1.1](https://openfontlicense.org).
- **Libraries** used when building (LVGL, GFX Library for Arduino, the ESP32 Arduino core) are not part of this repository and keep their own licenses; `config/lv_conf.h` is LVGL's configuration template (MIT license), adapted.
