# Original hardware: reverse engineering (AEG Micromat 143)

Made from the photos in `Original Hardware/` (2026-10-11). Companion to `HARDWARE.md` (new design) and `PLAN.md`.

How to read this:
- **[seen]**: visible in the photos (part, marking, track).
- **[inferred]**: the usual circuit for this part or layout, not traced end to end.
- **[measure]**: needs a continuity check with a meter, oven unplugged. The checklist is at the end.

Component values are left out where they can't be read, as agreed.

`Original Hardware/Overlays/` holds working images: each solder-side photo is mirrored and laid at 50 % over the component side, so every pad sits under its part, plus contrast-enhanced copper views. Use them to check or extend the traces below.

---

## 1. Overall wiring (the oven)

```
 mains cord ──► INLET FILTER board ──► SAFETY board (door switches, fuse) ──► harness
 (L, N, PE)    "STR7340_4142"          E1..E5 (5-pin connector)                │
               toroid choke,                                                    ├─► POWER board (6-way card edge)
               X cap, 2 Y caps                                                  │     ├─ own supply (TNY264 flyback)
               PE to chassis screw                                              │     ├─ 3 triacs ─► fan, turntable, lamp
                                                                                │     └─ relay K22 ─► HV transformer primary
                                                                                └─► thermal cutout (magnetron) in the HV feed
 POWER board ══ 14-way ribbon ══ CONTROL board (MCU, 4-digit LED display, keys, encoder, pot)
```

- **Inlet filter** [seen]: a small board at the cord entry, `STR7340_4142`: J1 and J2 (in/out), a toroid choke (two windings), a grey X capacitor and two blue Y capacitors. PE (green/yellow) goes to the chassis at the red screw.
- **Magnetron feed** [seen]: the relay K22 contacts go via JP1 (white 2-pin, grey wires) toward the HV transformer. A **thermal cutout** sits on the magnetron ("Temperature Switch.jpg") [inferred: in series with the transformer primary, as is usual].
- **The HV section** (transformer, capacitor, diode, magnetron) is untouched by this project and isn't covered here.

---

## 2. Safety PCB (door switch board)

Marked `UL94V0`, `080625539`, maker logo "Ipotesi" [seen]. A white rotary cam, driven by the door hooks through the black lever, actuates three microswitches.

### Parts [seen]
| Ref | Part | Notes |
|---|---|---|
| SW1 | Cherry microswitch (top, diagonal) | 3 pins visible (C, NO, NC footprint); check which two are used |
| SW2 | microswitch (bottom right, tilted) | actuated by the black lever arm |
| SW3 | Cherry/ZF **D45X**, 16(4) A 250 V~, 15 A 125–250 VAC, 1/2 HP | middle; 2 pins used (top right) |
| F1 | ceramic cartridge fuse in clips | silkscreen: "WARNING: for continued protection against risk of fire replace only with same type and rating of fuse". Read the rating off the end cap before refitting |
| E1–E5 | 6.3 mm spade tabs | to the 5-pin black harness connector, order top to bottom E1, E3, E2, E4, E5 |
| L1, N1 | spade tabs, bottom | mains in (from the inlet filter) |
| LF1, C1, C2, C3, R1 | **footprints only (not fitted)** | the filter is on the separate inlet board instead |
| (slot) | narrow slot between SW3 and E5 | creepage slot between the two mains poles |

### What the copper shows [seen in `Safety overlay.jpg` / `Safety copper enhanced.jpg`]
- The copper is single-sided with wide mains tracks only. No electronics apart from the fuse.
- **E1** connects to a wide track that runs over the top of the board to **SW1** and then down the whole left edge to the bottom (mains-in area).
- **E3** sits right next to one end of **F1**: the fuse output.
- **E2 and E4** are a short pair of tracks to the **SW3** (D45X) pins.
- **E5** joins a copper area that runs down past the slot to **SW2**'s pins.
- The long S-shaped middle track runs from the bottom (mains-in) to SW3's other side.

### Function [inferred: the standard microwave interlock, which this layout matches]
The three switches are usually:

```
 L ──[F1 fuse]──┬──[PRIMARY  (closed when door closed)]──► L to loads (power board)
                │
                └──[MONITOR (closed when door OPEN)]──┐     shorts L–N if the primary
                                                      │     welds shut: F1 blows
 N ─────────────┴──[SECONDARY (closed when door closed)]──► N to loads
```

- **Primary and secondary** break both poles when the door opens, staggered by the cam (the first opens before the hook releases).
- The **monitor** switch (often the heavy D45X) closes when the door opens. If the primary has welded shut, it short-circuits the supply and blows F1, so the magnetron can't run with the door open.
- **[measure]** which switch is which, and which E-tab carries what (checklist A). This board stays **exactly as it is**.

### Door sense for the ESP32: where to hook in
Both of your options work. Which is better depends on what checklist A finds.

**(a) Second microswitch (recommended).**
- Fit an extra small microswitch on the same cam or door hook, wired dry: COM to GND, NC or NO chosen so that **door closed = contact closed = GPIO14 LOW**. That is the preferred `DOOR_CLOSED_LEVEL = LOW`: a broken wire reads "open".
- It is fully isolated from mains, needs no parts on the mains side, and leaves the interlock chain untouched.
- After fitting, set `DOOR_CLOSED_LEVEL = LOW` in `config.h`.

**(b) 230 V optocoupler.**
- Sense the switched line **after the primary switch** (the E-tab that is live only with the door closed), against N.
- Parts: LTV-814 or H11AA1 (AC input), 2 × 100 kΩ 0.5 W in series (≥ 400 V rating together) plus a reverse diode or the AC-input type. The output transistor goes to GPIO14 with a pull-up, and 100 nF smooths the 50 Hz ripple.
- The reading pulses at 100 Hz: the firmware's 30 ms debounce smooths that only if the output is filtered, so add RC.
- Drawbacks:
  - it reads "door open" whenever the fuse has blown or the oven is unplugged;
  - it adds mains-side parts, which need creepage from the logic side.

---

## 3. Power PCB

Marked `ETU_C POWER 7340_7011/03`, label `ASM STR7340726 / PWR7340717101 / 31/03/2004 SAGEM` [seen]. Single-sided, through-hole. Several footprints are unfitted (variants with grill/combi).

### Blocks
```
 6-way card edge (J1 keying slot)                                  14-way ribbon X1/X2 ──► CONTROL board
 ┌───────────────────────┐
 │ pad1 ─► rectifier diodes ─► C1 (400 V bulk) ─► IC1 TNY264P ─► TR1 "M24841 5203" ─► D (×2) ─► C3, C2
 │                                     ▲ feedback: IC2 PC817 opto + DZ zener         │
 │                                                                                  ├─ +4.8 V (silkscreen "+4.8V", ST3) ─► MCU, display
 │                                                                                  └─ ~12 V (relay coil rail) [inferred]
 │ pad2 ─► TC1 (TO-220 triac) ─┐
 │ pad3 ─► TC3 (TO-92 triac)  ─┼─ gates via resistors + ST5/ST6 jumpers ◄── MCU (ribbon)
 │ pad4 ─► TC5 (TO-92 triac)  ─┘   (TC2 footprint empty)
 │ pad5/6 ─► F2 / "EXT" area [measure]
 └───────────────────────┘
 K22 relay (Omron G2R-1A-E, 12 VDC) ◄── T1 (TO-92) ◄── MCU; contacts ─► JP1 ─► HV transformer
 BUZ1 piezo buzzer ◄── MCU
```

### Parts [seen]
| Ref | Part | Function |
|---|---|---|
| IC1 | **TNY264P** (Power Integrations TinySwitch-II), DIP-8 | off-line flyback switcher |
| IC2 | **PC817** type opto ("L 9945 817A"), DIP-4 | feedback for the switcher |
| TR1 | flyback transformer "M24841 5203" | |
| C1 | green electrolytic, high voltage | bulk capacitor after the rectifier |
| C2, C3 | black electrolytics | output smoothing (two rails) |
| DZ | glass zener | feedback reference |
| D9 | small part next to IC1 | switcher bypass/clamp [inferred] |
| top row | 3 × rectifier diodes (1N400x type) | mains rectification |
| large resistor (yellow-violet-black-gold) | ~4.7 Ω power resistor [inferred from colours] | inrush limiter / fusible resistor in the supply input |
| `.1J63` film capacitor | 0.1 µF 63 V | IC1 bypass [inferred] |
| TC1 | triac, TO-220 | one of fan / turntable / lamp [measure] |
| TC3, TC5 | triacs, TO-92 (Z0103/Z0107 class) | the other two loads [measure] |
| TC2 | empty footprint | variant |
| F2 | round part, red with yellow band, 2 leads | near "EXT"; possibly a PTC or fuse [measure] |
| **K22** | **Omron G2R-1A-E, 12 VDC coil, SPST-NO 16 A 250 VAC** | **magnetron (HV transformer) relay** |
| T1 | small transistor (TO-92) | K22 coil driver [inferred]; T2 and T3 empty |
| D5 | footprint next to K22 | coil flyback diode position; D6 for the unfitted K3 |
| K3, JP2, ST4 "COMBI–•–GRIL" | empty footprints | grill/combi variant (not fitted) |
| BUZ1 | piezo buzzer | |
| X3 "P&B_SIGNAL", R13/R14, R10–R12, R15 | partly empty footprints | variant options |
| X1/X2 | 14-way ribbon to the control board | |

### Important finding: the logic side is probably at mains potential
The triacs are gated directly from the MCU's outputs (through resistors and jumpers, with no opto-triacs). That only works when the logic ground is tied to the triacs' MT1, which is one of the mains wires. The flyback transformer and opto give regulation, but **the whole low-voltage side, the control board and the ribbon are very likely live (at mains potential)**.

- **Never connect the ESP32, USB or a scope ground to this board's low-voltage side** while it has mains.
- **[measure]** (checklist B): continuity between the "+4.8V"/GND rail and the mains pads.

### Where to hook in
Recommended: **take the power board out of the circuit completely** and move its loads to your new isolated hardware.

1. **Magnetron:**
   - Desolder **K22** (or cut its corner of the board off; the board is marked "2" there).
   - Drive its 12 V coil from the new supply's 12 V rail with your own driver from GPIO17:
     - NPN (BC337/2N2222) or a logic-level MOSFET (2N7000/AO3400);
     - ~1 kΩ base resistor;
     - 1N4148 or 1N4007 across the coil;
     - 10 kΩ pull-down (GPIO17 already has one on the board).
   - The G2R coil takes roughly 40 mA at 12 V. [measure] the coil resistance: 12 V ÷ R.
   - The contact wires (JP1 → transformer) stay as they are.
   - This **replaces the earlier plan** in `HARDWARE.md` to reuse the board's own transistor, because that one sits on the live logic side.
2. **Fan, turntable, lamp:**
   - The three wires from the edge connector that go to the triacs move to three VMA400 relay contacts (K2 fan, K3 turntable, K4 lamp). COM goes to the switched L that fed the triacs.
   - [measure] which pad/wire is which load (checklist C).
3. **The supply** (TNY264) is no longer needed: the new 12 V + 5 V supply takes over. It must be fed **after the inlet filter** (and not through the door switches, so the controller stays on with the door open).
4. **Buzzer:** BUZ1 can be reused on the new driver (GPIO15) if you like; it is a plain piezo.

Alternative: keep the board and drive its ribbon inputs from the ESP32 through optocouplers. More work, and it leaves 20-year-old electrolytics in the circuit. Not recommended.

---

## 4. Control PCB

Marked `ETU_C CONTROL 7340_7001/03` [seen]. Your markings: "SAVE"/"KEEP" around the keys, X over the MCU and display area, cut lines drawn in.

### Parts [seen]
| Ref | Part | Notes |
|---|---|---|
| IC1 | 40-pin DIP microcontroller (marking worn off) | probably a PIC (as noted before); pin 1 bottom left, 20/21 at the top |
| Q1 | blue 3-pin ceramic resonator | MCU clock |
| IC2 | **empty** 16-pin footprint, with R39–R45 also empty | variant |
| display | 4-digit 7-segment LED module | digits driven by **T1–T6** (TO-92) with base resistors, segment resistors in a row |
| EN1 | **Alps rotary encoder** | time knob |
| EN2 | **Piher** rotary part (pot), blue paint dot | power knob (reads 21–3149 mV on the ESP32, see `HARDWARE.md`) |
| keys fitted | SW3 **R**otate, SW5 **Q**uick start, SW1 **S**top, SW10 **P**rogram (your letters); also SW9, SW2, SW6, SW14, SW12 (next to the display) | 6 × 6 mm tactile switches |
| keys not fitted | SW4, SW7, SW8, SW11, SW13, SW15 | variant |
| D1–D4 | 3 mm LEDs | D1 next to R (rotate LED), D3 next to SW6, D4 at the bottom (below the encoder) [function per the panel; measure] |
| 3 small diodes + resistors between SW9/SW2 and the MCU | | **the keys form a scanned matrix** with the display lines (diodes isolate the columns) [inferred] |
| C1, C4 `.1J63` | 0.1 µF film | decoupling |
| ST3, ST9 "GND", ST4 "+4.8V" | wire jumpers | supply from the power board via the ribbon |
| IT1, IT2, P1, P2 | test points / jumpers | |

### Where to hook in
- The board as a whole is not reused: the LCD replaces the display, and the ESP32 replaces the MCU. Your cut lines keep the key area.
- **Keys:**
  - Each key in the kept part is still connected into the old matrix (diodes, shared scan lines).
  - After cutting, **cut the remaining tracks to each key**, or lift one leg, so each key is a bare contact pair.
  - Then wire the four keys onto the resistor ladder (`HARDWARE.md`, GPIO4) or Stop to GPIO16.
  - Otherwise leftover matrix diodes and LEDs sit on the ladder line and shift the voltages.
- **LEDs D1 / D3:** keep them with their series resistors, but check the resistor value for 3.3 V drive (aim for 2–5 mA). Their cathode/anode tracks also need cutting free of the matrix.
- **Encoder / pot:** if you keep the original Alps encoder and Piher pot on the board, the same applies: cut their tracks to the MCU and wire them directly (encoder A/B/C, pot ends + wiper). The pot must sit between 3V3 and GND, not +4.8 V.
- **The ribbon** to the power board is removed (see section 3). While the power board is still connected, **the control board is live**.

---

## 5. Harness and wires [seen, functions to measure]
- 6-pin connector (to the power board card edge), wires left to right: **black, blue, red, purple, blue, brown**. Brown and blue are probably L and N from the safety board; the others go to fan, turntable motor and lamp [measure, checklist C].
- 5-pin black connector on the safety board (E1, E3, E2, E4, E5): brown, blue, purple, black wires visible.
- 2-pin white connector, **blue + white**: K22 relay contacts (JP1) to the HV transformer primary (via the thermal cutout) [inferred].
- Loads: fan motor ("Fan.jpg"), turntable motor ("Rotation Motor.jpg"), lamp ("Light.jpg"). Read their rating plates for the relay choice (all far below the VMA400's 10 A).

---

## 6. Measurement checklist (meter on continuity/ohms, **oven unplugged**, HV capacitor discharged)

**A. Safety board** (board on the bench, cam turned by hand: "closed" = as with the door shut). Fill in:

| From → To | door closed | door open | conclusion |
|---|---|---|---|
| L1 tab → E? (via F1) | | | |
| SW1 pins | | | primary / secondary / monitor? |
| SW2 pins | | | |
| SW3 (D45X) pins | | | |
| E1 ↔ E2 ↔ E3 ↔ E4 ↔ E5 (each pair) | | | |

The tab that is live **only** with the door closed (after the primary) is where an optocoupler would sense.

**B. Power board, mains reference:** ohms between the GND jumper (ST9 on the control board, or the "−" of C2/C3) and each of the 6 card-edge pads. **< 100 Ω to one pad = the logic is mains-referenced** (expected).

**C. Power board, loads:**
- Per card-edge pad: which part it reaches (TC1/TC3/TC5 pins, the rectifier, F2).
- The colour of the wire in that position.
- At the far end: which load the wire goes to.

| Pad | Wire colour | Goes to on the board | Load / L / N |
|---|---|---|---|
| 1 | | | |
| 2 | | | |
| 3 | | | |
| 4 | | | |
| 5 | | | |
| 6 | | | |

**D. K22:** coil resistance (expect ~250–350 Ω for 12 V). The contact pins to JP1.

**E. Control board LEDs:** series resistor values for D1–D4, and which panel symbol each one lights.

Send the results (or photos of the meter) and I'll turn the inferred parts into a confirmed schematic and update `HARDWARE.md`.
