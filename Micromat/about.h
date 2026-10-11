// Contents of Menu -> About: the firmware version, the oven's rating plate and
// the hardware it runs on. Measured values (chip revision, CPU, memory, build
// fingerprint, active slot) are read at run time in App::menuRow().
#pragma once

// Firmware version, major.minor.build: bump it with every firmware change
// (bug fix: build, feature: minor, major rework: major; see README.md
// "Versions and releases") and set the date to the day of the change. (A build
// date from __DATE__ isn't used: with arduino-cli's build cache it only changes
// when that one file is recompiled. The build fingerprint identifies every
// build exactly; see FirmwareUpdate::version().)
constexpr const char *FIRMWARE_VERSION = "1.0.1";
constexpr const char *FIRMWARE_DATE = "2026-10-12";

// Oven rating plate, inside the door frame (photo: AboutTheMicro.jpg).
constexpr const char *OVEN_MAKER = "AEG";
constexpr const char *OVEN_NAME = "Micromat 143E/U";
constexpr const char *OVEN_MODEL = "AEG Micromat 143E/U";  // both (splash)
constexpr const char *OVEN_TYPE = "C71A000V  ELC 03";
constexpr const char *OVEN_E_NR = "947.602 432";
constexpr const char *OVEN_SERIAL = "417 10918";

// Controller hardware (see HARDWARE.md).
constexpr const char *BOARD_MAKER = "Waveshare";
constexpr const char *BOARD_MODEL = "S3-Touch-LCD-2";
constexpr const char *DISPLAY_INFO = "2\" 320\xC3\x97" "240 IPS";  // 320×240
constexpr const char *TOUCH_CHIP = "CST816D";
constexpr const char *RTC_CHIP = "PCF85063A";
constexpr const char *RELAY_BOARD = "Velleman VMA400";
