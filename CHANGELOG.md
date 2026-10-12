# Changelog

All notable changes to the Micromat controller, newest first. Versions follow `major.minor.build` (see [README.md](README.md#versions-and-releases)): a bug fix bumps the build, a feature the minor, a major rework the major. Each released version has a branch `releases/<version>`. The fingerprint is the build the oven shows in Menu → About, as installed when the version was released.

Sections per version: **Added** (new features), **Changed** (behaviour that works differently), **Fixed** (bugs), **Documentation** (no firmware change).

## [Unreleased]

### Documentation
- `CHANGELOG.md` (this file), with the history back to the start of the project.

## [1.0.2] – 2026-10-12

Fingerprint `730d4a2c`. Pull request #4.

### Fixed
- Debug log: the first lines after a restart (firmware and build, reset reason, crash report, "settings: loaded") only went to USB serial, never to the SD card log. They are now kept in memory until the card is mounted and written to the log first.
- The log is always in English: the console report `texts N` printed the translated texts into the log (and onto the SD card). It now prints the English text next to the widths measured in language N.

### Documentation
- The SD card log is tested with a 16 GB card: writing, the setting kept across a restart, download over the home network. The first mount of a card can take about 2.5 s.

## [1.0.1] – 2026-10-12

Fingerprint `077bad2a`. Pull requests #2 and #3.

### Changed
- The signal when the timer reaches 00:00 is three 1 s beeps (was 200 ms each).
- Main screen: a Wi-Fi icon left of the magnetron icon, shown only while a home network is set up. The program icon (defrost, preset, favourite) moves left of it.

### Documentation
- Reverse engineering of the oven's original boards (safety/door switches, power, control) in `ORIGINAL_HARDWARE.md`, with the photos and overlays in `Original Hardware/`. The magnetron relay is an Omron G2R-1A-E with a 12 V coil (#1).
- Versioning rules (always bump the version; bug fix → build, feature → minor, major rework → major), pull requests only into `master`, release branches `releases/<version>` (#3).

## [1.0.0] – 2026-10-11

Fingerprint `1987c560`. Tag `v1.0.0`. Feature-complete on the bench and installed over Wi-Fi; not yet in the oven.

### Added
- Firmware update over Wi-Fi (Settings → Firmware update, only when idle): the oven's own hotspot and, if set up, the home network; a browser upload page with a random password shown on screen; network upload with ArduinoOTA. A new image boots on trial and confirms itself after 30 s, otherwise the next reset returns to the previous one. Verified from an Android phone.
- Menu → About: firmware version, date, build fingerprint and slot; the oven's rating plate; the controller hardware (chip, memory, display, touch, clock, relays, LVGL and core versions); Wi-Fi network, state, IP address (or the last one used) and MAC address.
- Start-up splash: the AEG logo, the oven model and the firmware version, for 5 s.
- Power in watts, the original oven's steps: 750/700/600/500/450/350/250/150/80/0 W (0 W = timer only). Turning the power knob shows the watts large for 1 s.
- Encoder steps by the time set (5 s below 2 min up to 5 min above 1 h), with acceleration when turned fast.
- Sleep screen: the red AEG logo at the top and the clock in big grey digits at the bottom.
- Debug log to a microSD card (Settings → Debug log), downloadable over the hotspot page.
- German, French and Spanish, besides English and Swedish. Console `texts N` measures every text with the real fonts to check that translations fit.
- Paused: only the remaining time blinks.
- Crash report: the reset reason, failed memory allocations and the last core dump at start-up and with the console `crash` command; console `mem`.
- Wi-Fi password encryption key (eFuse) created automatically the first time Settings → Wi-Fi → Network is opened on a board without one; console `keygen YES` does the same. Console `wifi` checks that the saved password can be decrypted.
- `tools\build.ps1`: builds the firmware and names the image with version, build date and fingerprint. `README.md` for people. A copy of LVGL's `lv_conf.h` in `config/`.

### Changed
- Power levels in percent became watts. Favourites saved in percent are converted to the nearest watt step (settings key `s2` → `s3`); presets and defrost use watt steps.
- The firmware version became a build fingerprint (part of the image's SHA-256) instead of the compile date and time, which only changed when one particular file was recompiled.
- All log output goes through one log (USB serial and, if on, the SD card).

### Fixed
- Crashes when internal RAM ran out: LVGL's font caches used it all up (504 bytes left after showing every language), and the next system lock created on first use failed. This crashed the first `keygen YES` and opening Wi-Fi setup. Memory now comes from PSRAM first; about 268 KB of internal RAM stays free, and a small leak on each Wi-Fi setup is gone.
- Creating the eFuse key aborted inside the system the first time; the driver's lock is now created at start-up.
- The Firmware update and Wi-Fi setup screens closed at once: their idle timeout compared times taken in the wrong order and wrapped around.
- The hotspot was named `Micromat-0000`: the MAC address was read before the radio had it; it is now read from eFuse.
- Service screen: lines were cut off; firmware, Wi-Fi and fan run-on now each have their own line.
- The display went blank until a restart when the SD card used a different SPI peripheral on the display's pins; the card now shares the display's peripheral.
- Translations that didn't fit the screen were shortened (French/Spanish "door open", some German and French hints, Swedish "Uppdatering" and "Mjukt smör"); the splash drops the word "Firmware" when its line would be too wide.

## Before version control – up to 2026-10-10

The first commit (2026-10-10) holds the work done up to then. Reconstructed from `PLAN.md`, `HARDWARE.md` and the notes in `CLAUDE.md`.

### Added
- Test sketches for bringing up the Waveshare ESP32-S3-Touch-LCD-2: display, LVGL 9.5, touch (own CST816D driver with calibration), inputs, relays, buzzer and the PCF85063A clock.
- The `Micromat` controller: a non-blocking state machine (Idle / Cooking / Paused / Done); relay outputs with interlocks in software (magnetron only with the door closed and the fan running for 1 s; everything off at start-up, reset and door open); a loop watchdog; magnetron power by duty cycle over 24 s.
- Panel inputs: the original buttons on a resistor ladder, the encoder with push, the power pot; the panel LEDs.
- LVGL user interface with menus: defrost by weight, presets, 4 favourites, settings (clock, brightness, dimming, sound, language, Wi-Fi), a service screen (hold Program 5 s); English and Swedish with all fonts from Montserrat (å/ä/ö) and 77 px digits.
- Clock: the PCF85063A keeps UTC, local time with summer time; optional time over Wi-Fi (NTP), synced daily.
- Wi-Fi setup from a phone: the oven opens its own network (QR code), lists the networks in range and saves the chosen one only if it connects. The password is stored encrypted.
- Fan run-on after cooking; Quick start (+30 s while cooking, resumes a pause without adding time).
- Icons drawn on the main screen: magnetron on/off, and a program icon while a program is loaded (snowflake = defrost, one shared icon for all presets, star = favourite).

### Changed
- Fan run-on only after heavy use (at least 3 of the last 4 minutes heating), 5 minutes, like the original oven, instead of after every cook.

### Fixed
- Panel buttons: the carbon contacts close gradually, so a press passed through other buttons' voltage windows and could act as the wrong button. Now the lowest button seen wins and Stop always gets through.
- Input lag: the display was redrawn on every pass even when nothing changed (84 ms stalls 10 times a second); values are now only set when they change.
- Touch: the first readings after touching were stale and are now ignored.
