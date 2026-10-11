# Hardware plan — AEG Micromat 143 controller

Companion to `PLAN.md`. Pin facts come from Waveshare's schematic
(`https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-2/ESP32-S3-Touch-LCD-2-SchDoc.pdf`)
and demo code. Mains assumed 230 V / 50 Hz.

## ESP32-S3-Touch-LCD-2: pin availability

### Used on the board (not on the header, or not available)
| GPIO | Function |
|---|---|
| 0 | BOOT button (strapping pin) |
| 1 | LCD backlight |
| 3 | IMU INT1 (strapping pin) |
| 5 | Battery voltage ADC |
| 19, 20 | USB D−/D+ (on the header but needed for USB) |
| 26–37 | Flash / octal PSRAM |
| 38, 39, 40 | LCD + SD SPI (MOSI, SCLK, MISO) |
| 41 | SD card CS (microSD for the debug log; shares the LCD SPI bus; any card up to 32 GB, FAT32) |
| 42, 45 | LCD DC, LCD CS |
| 46 | Touch INT (unused so far; could make touch interrupt-driven) |

### On the header (P1/P2) and free when no camera is fitted
| GPIO | Notes |
|---|---|
| 2, 4, 6, 7, 8, 9, 10 | **ADC1**: the only ADC pins usable with Wi-Fi on (ADC2 = GPIO11–20 is blocked by Wi-Fi) |
| 11, 12, 13, 14, 15, 18 | Plain GPIO (camera data/clock lines, no onboard pulls) |
| 16, 21 | **4.7 kΩ pull-up to 3V3 on board** (camera SCCB). Good for buttons; bad for active-high relay drive |
| 17 | **10 kΩ pull-down on board** (camera PWDN, verify on schematic). Best pin for the magnetron relay |
| 43, 44 | UART0 TX/RX. TX prints the ROM boot log at reset, so don't use 43 for relays |
| 47, 48 | I2C SCL/SDA, shared with touch (0x15) and IMU (0x6B), with 4.7 kΩ pull-ups. **Use for the RTC** and any I/O expander |

Header also has: 3V3, 5V, VBAT, GND (×2+).

## Proposed pin map

| GPIO | Function | Direction | Notes |
|---|---|---|---|
| 17 | K1 magnetron relay | out | onboard pull-down keeps it OFF at boot |
| 18 | K2 fan relay | out | add 10 kΩ pull-down at driver input |
| 8 | K3 turntable relay | out | add 10 kΩ pull-down |
| 9 | K4 lamp relay / SSR | out | add 10 kΩ pull-down |
| 10 | K5 spare relay | out | add 10 kΩ pull-down |
| 11 | Encoder A | in | pull-up + RC filter |
| 12 | Encoder B | in | pull-up + RC filter |
| 13 | Encoder push | in | pull-up |
| 4 | Panel buttons (resistor ladder) | analog | ADC1, see below |
| 16 | Stop button (optional dedicated) | in | onboard pull-up; button to GND. Unused if Stop stays on the ladder |
| 14 | Door sense | in | from opto or dry contact, pull-up |
| 15 | Buzzer | out | LEDC PWM → transistor |
| 2 | Power pot | analog | ADC1 |
| 43 | LED "rotation active" | out | via 330 Ω–1 kΩ; flickers during boot log (harmless) |
| 44 | LED "max power" | out | via 330 Ω–1 kΩ |
| 6, 7, 21 | I2S mic SCK / WS / SD | — | optional (popcorn); SD in on 21 (has pull-up) |
| 47, 48 | I2C: RTC (+ expander) | — | shared bus |

Free after this: none on the header except 16 (if Stop is on the ladder). Extra buttons or outputs → I2C expander (PCF8574 / MCP23017) on 47/48.

### Panel buttons: resistor ladder on GPIO4

The original panel buttons are reused: Stop, Quick start, Program, Rotate. The time-set button goes away (time setting moves to the menu).

```
3V3 ──[ R0 10k ]──┬──────────────► GPIO4 (ADC1)
                  │      └─[100nF]─ GND   (noise filter)
                  ├─[ 1k  ]─ Stop ──────── GND
                  ├─[ 3.3k]─ Quick start ─ GND
                  ├─[ 8.2k]─ Program ───── GND
                  └─[ 22k ]─ Rotate ────── GND
```

| Button | Nominal V | Window |
|---|---|---|
| none | 3.3 | > 2.8 |
| Rotate | 2.27 | 2.0–2.7 |
| Program | 1.49 | 1.35–2.0 |
| Quick start | 0.82 | 0.8–1.35 |
| Stop | 0.30 | < 0.8 |

- **Measured on the real panel (verified):** Stop 287–295, Quick start 806–811, Program 1565–1579, Rotate 2266–2272, none 3149 mV. **Later the readings rose** (contact resistance in the pads or the common return): Stop 375–562, Quick start 890–1017, Program 1718–1724, Rotate 2311–2370 mV. With the first windows a Stop at ~555 mV was decoded as Quick start, so Stop seemed to act as Quick start and the oven could only be stopped with the encoder. A third run: Stop 317–614, Quick start 1003–1085 mV (still rising). Windows in `Micromat/config.h` are now < 800 / < 1350 / < 2000 / < 2700 mV. Check the readings on the service screen now and then; cleaning the pads or a dedicated Stop pin (GPIO16) would give more margin.
- Polled every ~10 ms (no interrupt needed); a key is accepted after 3 identical consecutive readings. As a safeguard against contacts that close gradually, a press acts after settling 60 ms (Stop at once), the lowest key seen during the press wins, and a lower key later in the same press still acts. Each release logs the lowest mV of the press. A dedicated Stop pin (GPIO16) would remove the dependence entirely.
- The lowest resistor dominates when several buttons are pressed, so **Stop wins over any combination**. Combos can't be detected.
- Windows tolerate +500 Ω contact resistance per button (membrane/carbon contacts). Set the final thresholds from real readings in `Test_Inputs`.
- An open wire on the ladder reads as "no button". Optionally move Stop to its own pin (GPIO16) so it doesn't depend on the ladder wire. The door interlock covers the magnetron in hardware either way.

## Surrounding hardware (BOM draft)

### Power
- **Isolated 230 V → 5 V, ≥ 1 A** module, e.g. Mean Well IRM-05-5 (certified) or Hi-Link HLK-5M05. The budget is the board (~250 mA, Wi-Fi peaks ~500 mA) plus 4–5 relay coils at ~70 mA each.
- Input side: fuse (e.g. 500 mA T) + varistor (275 VAC MOV) on the module's mains input.
- **Back-feed:** the header 5V pin is tied to USB VBUS. Feed it through a Schottky diode (e.g. SS34), or never plug USB into a PC while mains-powered.

### Relays and drivers
- **K1 magnetron (decided): reuse the original power relay.** It's already rated for the transformer's inrush. Driven from GPIO17 by **reusing the original driver transistor and flyback diode** next to the relay. Coil supply from the second rail of a combined 5 V + 12/24 V PSU; coil ground tied to ESP32 ground.
  - **Open: coil voltage (12–24 V)**, to be read during dismantling.
  - **Check the base resistor:** it was sized for the PIC's (likely 5 V) drive. From 3.3 V, aim for ~2–8 mA base current (≈ coil current / 10), i.e. ~330 Ω–1 kΩ. Replace it (~680 Ω) if it's in the 4.7–10 kΩ range. If the transistor is a "digital" type with built-in resistors, note its part number and check it at 3.3 V.
  - Fallback: switch the coil supply with the spare VMA400 channel (K5) instead (opto-isolated, no transistor work).
- **K2–K5 (fan, turntable, lamp, spare) (decided): Velleman VMA400** 4-channel relay module (same design as Electrokit 4101 8309). 4 × SPDT relays, 10 A @ 250 VAC, opto-isolated **active-high** inputs, 5 VDC supply (~70 mA per energised coil). Users report reliable triggering from 3.3 V (including Velleman's own Raspberry Pi example). One GPIO per channel: GPIO18/8/9/10. No onboard controller, so a reset turns everything off.
  - **Verified with `Test_Relays`:** all four channels switch from 3.3 V in order, and none flicker during upload or reset.
  - 10 kΩ pull-down on each input.
  - Mount on insulated standoffs; cheap boards have little mains-to-logic spacing.
  - (Rejected: SparkFun Qwiic Quad Relay. Relays hold their state through an ESP32 reset, the fan interlock would depend on I2C, and relay noise could lock up the shared I2C bus. It's also a retired product.)
- **K2 fan, K3 turntable:** small motors (~20–30 W); any 10 A relay.
- **K4 lamp:** relay or SSR. A zero-cross SSR is fine for an incandescent lamp; an SSR's snubber leakage can make an LED lamp glow.
- **Driver:** relay coils need more than a 3.3 V GPIO can give. Options:
  - ULN2003A/ULN2803A: 3.3 V compatible, built-in flyback diodes, off when the input floats. Recommended for a custom relay board.
  - Ready-made modules: choose **active-high (high-level trigger)** opto modules that are verified to switch at 3.3 V. Avoid low-trigger modules: a GPIO driven low during boot would switch them ON.
- Add a 10 kΩ pull-down on every relay driver input (except GPIO17, which has one).

### Door sense
- The MCU only *reads* the door. Interlock wiring stays original.
- Preferred: a spare / extra low-voltage microswitch contact on the retained switch board, to GND with pull-up.
- Alternative: AC-input optocoupler (H11AA1 / LTV-814) sensing mains after the interlock chain, with suitable series resistors (≥ 2 × 0.5 W, rated for 230 V).

### User inputs
- **Verified:** encoder on GPIO11/12 gives **2 quadrature steps per click**; clockwise = increasing (decoded in a pin-change interrupt). Pot on GPIO2 reads 21–3149 mV across its travel, turning right = higher voltage = more power. Encoder push on GPIO13 verified (after a wiring fix): push = Start, refused with an error beep when no time is set.
- **Rotary encoder:** EC11 bare or KY-040 module. A KY-040 has 10 kΩ pull-ups to its "+" pin, so power it from **3V3, not 5V**. Add 10 nF to GND on A/B for debounce.
- **Buttons:** original panel buttons on the resistor ladder (see pin map). Need: how the panel is wired (one wire per button + common, or a scanned matrix?).
- **LEDs (verified):** panel LEDs on GPIO43 (rotation enabled) and GPIO44 (max power, 750 W) with series resistors. GPIO43 may flicker during the boot log (harmless).
- **Power pot:** 10 kΩ linear between 3V3 and GND, wiper to GPIO2, 100 nF at the pin. The ESP32 ADC is non-linear near the ends, which is fine for 5–10 power steps.

### Sound (decided, tested)
- Buzzer on GPIO15 through an NPN transistor with base resistor, flyback diode across the buzzer. Loudest around 2.5–3 kHz. Drive it with an LEDC tone at **2700 Hz**; "done" signal = 3 × 200 ms beeps (`Test_Buzzer`).

### RTC (decided)
- **Soldered 333051**: PCF85063A breakout, I2C address 0x51, 0.9–5.5 V supply, battery holder, easyC (Qwiic-compatible JST-SH) connector. Arduino library from Soldered.
- Sits on the shared I2C bus (GPIO47/48) at 3.3 V. No address conflicts (touch 0x15, IMU 0x6B, unknown 0x7E).

### Popcorn sensing (optional)
- INMP441 I2S MEMS mic on GPIO 4/6/7. Alternative to try first: the onboard IMU sensing pop vibrations through the case.

### Wiring and environment
- Keep the 5 V / logic wiring physically away from the HV section (transformer, capacitor, diode, magnetron).
- Locking connectors (JST-XH / Molex) for anything that sees vibration; mains-rated wire and spade terminals on the relay side.
- The control compartment gets warm while cooking; keep the board out of the fan's hot-air path, and check temperatures during bring-up.

## eFuse key for the Wi-Fi password (created 2026-10-11)

The Wi-Fi password is stored encrypted (`Micromat/wifi_creds.*`). The AES key isn't stored anywhere: the chip's HMAC peripheral derives it from a secret key in an **eFuse key block** (purpose `HMAC_UP`). Until that eFuse key exists, the firmware refuses to save any Wi-Fi password, so Wi-Fi can't be used. It never falls back to plain text.

**Decision (2026-10-10):** keep this design, rather than plain-text storage (what most ESP32 devices do) or obfuscation with a chip-ID-derived key (cosmetic only). It has no practical effect on further development: uploads, partitions, new features and OTA are unaffected, and five key blocks stay free, enough even for flash encryption plus Secure Boot. Full flash encryption was judged overkill and is not planned. **Status: created 2026-10-11 on the bench board (MAC 44:1b:f6:85:de:78), with the user's approval.** `keygen YES` used BLOCK_KEY0. The eFuse summary (`espefuse --port COM5 summary`, read-only) afterwards: KEY_PURPOSE_0 = HMAC_UP (locked), BLOCK_KEY0 reads `??` and is read- and write-protected (`-/-`), RD_DIS = 1 (that block only); BLOCK_KEY1–5 still unused (USER); flash encryption and Secure Boot off. A saved test password encrypted and decrypted correctly (`wifi` reports "password ok"), and the test network was then forgotten. **The first `keygen YES` attempt crashed** (abort in ESP-IDF `lock_init_generic`: creating the eFuse driver's mutex returned NULL, with ~190 KB internal RAM free) **before anything was written**: the summary showed all key blocks still empty. It could not be reproduced, and the second attempt worked. Note: `espefuse` leaves the board in download mode; restart it with `esptool --port COM5 --after hard-reset chip-id` or the RESET button.

**Creating it:** automatically when the owner opens Settings → Wi-Fi → Network on a board without a key (decided 2026-10-11: choosing a network is the go-ahead; firmware update and log download never need the key), or with the serial command `keygen YES` (`keygen` alone only prints a warning). If it fails, the menu shows "Security key could not be created" and the setup doesn't open. Either way the firmware it writes a random 256-bit key to the first unused key block, sets its purpose to `HMAC_UP`, and read- and write-protects that block, then reads back that the block has purpose HMAC_UP and is read-protected, and fails otherwise. Only a completely blank block counts as unused, so a block left half-written by a power cut is skipped. The key never leaves the chip, and nothing else burns eFuses. It is never done at start-up, so a crash during the burn can't cause a restart loop: the panic handler restarts the board (all outputs off, as at any start) and the loop watchdog (5 s) covers a hang. To avoid the first attempt's crash, `credsBegin()` creates the eFuse driver's lock at start-up with an empty, cancelled write batch. **Never run `keygen YES`, or any `espefuse burn_*` command, without the user's explicit approval.**

**Risks and consequences:**
- **Permanent:** an eFuse can't be cleared. The block stays used and protected for the life of the chip, and can't be read, changed or reused. A new key could only go into another unused block, which would make the stored password unreadable, so the network would have to be entered again.
- **Uses one of six key blocks.** The others stay free for later use, e.g. flash encryption or Secure Boot.
- **Unchanged after burning:** USB upload, serial, the bootloader and download mode work exactly as before, and the board can still run any firmware, including Waveshare's factory image.
- **Bound to this chip:** a replacement board needs its own `keygen YES` and the network entered again. A flash backup restored onto another board can't decrypt the password; it stops connecting and needs a new setup.
- **The firmware's label must stay the same:** the AES key is derived from the eFuse key together with a fixed label in the firmware (`HMAC_LABEL`, "… v1"). Changing that label in a firmware update makes the stored password unreadable, so the network must be entered again. That is harmless, but don't do it by accident.
- **No practical way to brick the board:** `HMAC_UP` only feeds the HMAC peripheral; it has nothing to do with booting. The burn writes nothing to BLOCK0 apart from the key purpose and the protection bits. Even a burn interrupted by a power cut leaves the board booting normally. At worst that one key block is left unusable, and `keygen YES` can then use another one.

**What it protects against, and what not:**
- **Protects against** reading the flash off the board (`esptool read_flash`, or a desoldered flash chip). That copy holds only ciphertext, plus the Wi-Fi library's area, which `credsBegin()` keeps empty.
- **Does not protect against** someone with physical access uploading their own firmware to *this* chip and having the HMAC hardware derive the key again. Closing that needs **Secure Boot**, which allows only signed firmware, and usually **flash encryption** too. Both are bigger, risky, mostly irreversible steps: a lost signing key means the board can never be updated again, and they change how uploads work. **They are not planned.**
- **Firmware update (OTA) and the key:** any firmware that runs on this chip can have the HMAC hardware derive the key, and so decrypt the password. Without Secure Boot, USB already allows that. OTA adds no new route as long as it stays **on demand** (only while Settings → Firmware update is open, with the oven idle) and **password-protected** (a new random password each time, shown only on the oven's screen). Don't add an always-on or unauthenticated update path.
- The password is also briefly in RAM while connecting. The firmware clears its buffers afterwards, but the Wi-Fi driver keeps its own copy while connected.

**After burning, check:** run `espefuse --port COM5 summary` (read-only). The key block should show purpose `HMAC_UP`, its RD_DIS bit set (the key reads as `?? ??`), and write protection on. Serial `wifi` should report "encryption key ok". Record the date and block number here.

**Testing without burning:** the prebuilt Arduino core has no virtual-eFuse mode (`CONFIG_EFUSE_VIRTUAL` is off). Until the key exists, the serial commands, the menu and the setup screen can be tested up to the save step, which then reports "Could not save the password".

## Open questions
- Magnetron relay coil voltage (and part number); the door-switch board's contacts (photos)
- Keep the pot, or set power by encoder/touch? (frees GPIO2)
- Mic or IMU for popcorn (frees 6/7/21 if IMU)
- Stop on the ladder, or on its own pin (GPIO16)?
- Panel ribbon pinout: buttons and LEDs (photo or multimeter)
