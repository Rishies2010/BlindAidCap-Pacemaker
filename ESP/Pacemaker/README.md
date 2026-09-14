# Pacemaker Demo — Wiring & Build Documentation

**Author:** nqetm
**Board:** ESP32 DEVKIT V1
**Layout:** ESP32 mounted off-board (not seated in a breadboard), two separate non-interlocking breadboards for components

---

## 1. Overview

This document covers the full electrical build for the pacemaker demonstration model. The ESP32 reads a pulse signal, classifies it against a user-configurable normal range, and when the reading is abnormal, drives a dual-chamber servo/LED model at a corrected rate toward a target BPM. It also plays a synthesized lub-dub heartbeat through a small amplifier and speaker, sounds an alert buzzer when pacing engages, renders status on a 128×64 OLED, and broadcasts its own WiFi access point serving a live web dashboard.

Settings (volume, target BPM, normal range, theme, accent) and the event log persist across reboots in the ESP32's NVS flash.

Because the two available breadboards are different sizes and cannot be physically interlocked, the ESP32 is not seated in either board. Instead it sits free (secured separately inside the model housing) and every connection to it is made with individual jumper wires clipped directly onto its header pins.

---

## 2. Physical Layout

| Element | Role |
|---|---|
| **ESP32** | Hangs free, secured on its own (see Section 6). Not plugged into any breadboard. |
| **Breadboard A — "Logic board"** | Hosts the 3.3V/GND rails, OLED display, pulse sensor. |
| **Breadboard B — "Actuation board"** | Hosts the 5V/GND rails, both servos, both LEDs, buzzer, speaker amp. |
| **Bridge wire** | One jumper wire linking Breadboard A's ground rail to Breadboard B's ground rail. This is mandatory — see Section 3. |

Since the two boards are physically separate, every wire that needs to reach "the other side" is just a jumper wire run across the gap between them. There is no special trick here beyond making sure the **ground bridge** is in place.

---

## 3. Power Rail Setup

| Rail | Location | Fed from |
|---|---|---|
| 3.3V | Breadboard A, (+) rail | ESP32 `3V3` pin |
| GND (A) | Breadboard A, (−) rail | ESP32 `GND` pin |
| 5V | Breadboard B, (+) rail | Powerbank red wire |
| GND (B) | Breadboard B, (−) rail | Powerbank black wire |
| **Ground bridge** | A (−) rail ↔ B (−) rail | One jumper wire connecting both boards' ground rails |

> **Why the ground bridge matters:** the ESP32's logic-level signals (servo PWM, LED GPIO, I2C, DAC audio, buzzer) all reference the ESP32's own GND as their "zero." If Breadboard B's ground isn't tied back to that same reference, every signal sent to a component on Breadboard B is meaningless from the ESP32's point of view, even though the wire is physically connected. This is the single most common reason a "correctly wired" breadboard circuit does nothing.

**Setup steps:**
1. Jumper wire: ESP32 `GND` pin → Breadboard A (−) rail.
2. Jumper wire: ESP32 `3V3` pin → Breadboard A (+) rail.
3. Powerbank red wire → Breadboard B (+) rail.
4. Powerbank black wire → Breadboard B (−) rail.
5. Jumper wire: Breadboard A (−) rail → Breadboard B (−) rail. *(the ground bridge)*
6. Do **not** connect Breadboard A's (+) rail to Breadboard B's (+) rail — one is 3.3V and one is 5V, and joining them will damage components.

---

## 4. Pin Map

| Component | ESP32 Pin | Notes |
|---|---|---|
| Pulse sensor signal | GPIO34 | Input-only ADC pin |
| Atrium servo signal | GPIO18 | PWM, ordinary timer pin |
| Ventricle servo signal | GPIO19 | PWM, ordinary timer pin |
| Atrium LED | GPIO27 | Through 220Ω resistor |
| Ventricle LED | GPIO33 | Through 220Ω resistor |
| Buzzer | GPIO14 | Direct GPIO drive, no separate power needed |
| Speaker amp (INL) | GPIO25 | **DAC1 — analog output.** CA-8403 / PAM8403 board |
| OLED SDA | GPIO21 | I2C data |
| OLED SCL | GPIO22 | I2C clock |

> **Why the audio goes on GPIO25, not GPIO13:** the firmware generates the heartbeat as a real analog waveform using the ESP32's internal 8-bit DAC. Only GPIO25 (DAC1) and GPIO26 (DAC2) have this hardware. GPIO13 has no DAC, so an amp wired to GPIO13 will be silent under the current firmware. This is also why the two servos had to move off GPIO25/26 to GPIO18/19 — those pins are now reserved for analog output.

> **Pins to leave alone:** GPIO25 and GPIO26 are the only DAC pins. Do not attach servos, LEDs, or anything else to them once the amp is wired to GPIO25.

---

## 5. Component Wiring

### 5.1 OLED Display (Breadboard A)
| OLED pin | Connects to |
|---|---|
| VDD | Breadboard A (+) rail [3.3V] |
| GND | Breadboard A (−) rail |
| SCK | ESP32 GPIO22 (direct jumper) |
| SDA | ESP32 GPIO21 (direct jumper) |

### 5.2 Pulse Sensor (Breadboard A)
| Sensor wire | Connects to |
|---|---|
| + (red) | Breadboard A (+) rail [3.3V] |
| − (black) | Breadboard A (−) rail |
| Signal | ESP32 GPIO34 (direct jumper) |

### 5.3 Atrium Servo (Breadboard B)
| Servo wire | Connects to |
|---|---|
| Signal (orange) | ESP32 GPIO18 (direct jumper) |
| V+ (red) | Breadboard B (+) rail [5V] |
| GND (brown/black) | Breadboard B (−) rail |

> Never power a servo from the ESP32's own 3V3/5V pin directly — the current spikes on movement can brown out the board. Servo power always comes from the powerbank-fed rail.

### 5.4 Ventricle Servo (Breadboard B)
| Servo wire | Connects to |
|---|---|
| Signal (orange) | ESP32 GPIO19 (direct jumper) |
| V+ (red) | Breadboard B (+) rail [5V] |
| GND (brown/black) | Breadboard B (−) rail |

### 5.5 Atrium LED + 220Ω Resistor (Breadboard B)
1. Resistor: one leg → ESP32 GPIO27 (direct jumper), other leg → LED long leg (anode).
2. LED short leg (cathode) → Breadboard B (−) rail.

### 5.6 Ventricle LED + 220Ω Resistor (Breadboard B)
1. Resistor: one leg → ESP32 GPIO33 (direct jumper), other leg → LED long leg (anode).
2. LED short leg (cathode) → Breadboard B (−) rail.

### 5.7 Buzzer (Breadboard B)
| Buzzer pin | Connects to |
|---|---|
| + | ESP32 GPIO14 (direct jumper) |
| − | Breadboard B (−) rail |

### 5.8 Speaker Amp — CA-8403 / PAM8403 (Breadboard B)
| Amp pin | Connects to |
|---|---|
| VCC | Breadboard B (+) rail [5V] |
| GND | Breadboard B (−) rail |
| INL | ESP32 GPIO25 (direct jumper) — **DAC1, analog audio** |
| INR | Unused (mono setup) |
| L+ | Speaker wire 1 |
| L− | Speaker wire 2 |

> The amp expects an analog line-level signal on INL. The ESP32's DAC on GPIO25 produces exactly that, so no coupling capacitor is needed between the ESP32 and INL. Do **not** route GPIO25 through anything else (no resistor divider, no LED, no servo) once the amp is connected — the DAC stream will fight the extra load and the audio will distort.

> Keep the INL wire as short as practical and physically separated from the two servo signal wires. Servo PWM switching on GPIO18/19 can couple audible whine into the audio line. A short ground-return jumper run alongside the INL wire helps if you can't separate them.

---

## 6. Securing the Hanging ESP32

Since the ESP32 has no breadboard home, it needs to be physically anchored so its header pins don't get stressed or ripped out by jumper wires tugging on it.

- Tape or hot-glue the ESP32's underside to a flat surface inside the model housing (its silkscreen/component side facing up, away from any glue).
- Leave enough slack in every jumper wire that a small bump won't pull a wire off the header — wires should droop slightly, never pulled taut.
- Route the USB/power cable to the ESP32 along the same path as the jumpers, bundled together with tape so nothing snags independently.
- Optional but recommended: hot-glue a small dab over each jumper-to-header connection once wiring is confirmed working, to stop wires from working loose from vibration or handling during the expo.
- The ESP32 is broadcasting a WiFi access point. Do **not** wrap the antenna end of the board in metal, foil tape, or a metal enclosure — that will kill the range. If the model has a metal frame, keep the antenna end of the module facing out through plastic or a cutout.
- The INL audio wire to GPIO25 carries a low-level analog signal. If you bundle wires, keep this one separate from the two servo signal wires so PWM noise doesn't ride into the audio.

---

## 7. Pre-Power Safety Checklist

Run through this with a multimeter (continuity/beep mode) **before** connecting the powerbank:

1. Breadboard A (−) rail to Breadboard B (−) rail → **should beep** (confirms the ground bridge is actually connected).
2. Breadboard A (+) rail to Breadboard B (+) rail → **should NOT beep** (3.3V and 5V must never be joined).
3. Every LED's long leg (anode) is on the resistor side, not directly to ground — reversed LEDs simply won't light, they won't be damaged, so this is safe to check by testing after power-up if you're unsure.
4. All jumper wires seated firmly — a loose jumper looks the same as a solid one but behaves like it isn't there at all.
5. **GPIO25 is wired only to the amplifier's INL.** Nothing else — no servo, no LED, no sensor — should share that pin. GPIO18 and GPIO19 are wired only to the two servo signal lines.

Only once all of the above checks out: connect the powerbank, then upload the firmware.

---

## 8. Firmware & Filesystem

The current sketch is a multi-file project: the main `.ino` runs the pacing logic, pulse analysis, audio synthesis, and web server, while the dashboard is served from LittleFS using three separate files (`index.html`, `style.css`, `app.js`). Flashing the `.ino` alone is **not** enough — the web files must also be uploaded to the ESP32's LittleFS partition, or the dashboard will answer with `File not found`.

**Build order:**
1. Confirm all wiring against Section 4 and Section 7.
2. Upload the main sketch (`Pacemaker.ino`) using the normal Arduino IDE / PlatformIO upload.
3. Upload the `data/` folder (`index.html`, `style.css`, `app.js`) to LittleFS:
   - Arduino IDE: `Tools → ESP32 LittleFS Data Upload`
   - PlatformIO: `pio run -t uploadfs`
4. Power-cycle the ESP32.

**Connecting to the dashboard:**
1. On a phone or laptop, join the WiFi network `Pacemaker` with password `87654321`.
2. Open `http://192.168.4.1/` in a browser. Use `http://`, not `https://`.
3. `http://pacemaker.local/` also works on most devices via mDNS, but Windows and some Android builds are unreliable with mDNS — if it doesn't resolve, use the IP address.

**Required libraries:** `ESP32Servo`, `Adafruit SSD1306`, `Adafruit GFX Library`, `ArduinoJson`. The remaining dependencies (`WiFi`, `WebServer`, `ESPmDNS`, `LittleFS`, `Preferences`, `Wire`, `esp_timer`) ship with the ESP32 board package.

**Settings persistence:** volume, target BPM, normal range, theme, and accent are saved to NVS via `Preferences` and survive reboot and re-flash. To reset them, either clear NVS from the Arduino IDE (`Tools → Erase All Flash Before Sketch Upload`) or POST to `/api/restart` after clearing them from the dashboard.

---

## 9. Troubleshooting Quick Reference

| Symptom | Most likely cause |
|---|---|
| OLED blank | Ground bridge missing, or wrong I2C address (try `0x3D`) |
| Servos twitch but don't hold position | Not enough current — confirm they're on the 5V rail, not the ESP32 pin |
| Nothing on Breadboard B works at all | Ground bridge between the two boards missing or loose |
| LED doesn't light | Check anode/cathode orientation, and resistor is in the signal path |
| Speaker silent | Confirm amp VCC reads 5V, its GND is on the shared ground, and INL is on **GPIO25**. Audio will not work on GPIO13 or any other non-DAC pin. |
| Heartbeat audio distorted or crackly | INL wire running alongside the servo signal wires, or something else sharing GPIO25. Separate the wire and make sure GPIO25 is dedicated to the amp. |
| Servos jitter each time the heartbeat plays | Same as above — analog audio coupling into servo PWM. Shorten or reroute the INL wire. |
| ESP32 randomly resets | Servo current draw browning out the board — verify servo power isn't sourced from the ESP32 itself |
| Dashboard loads but shows `File not found` | The `data/` folder was never uploaded to LittleFS. See Section 8 — uploading the sketch alone is not enough. |
| Dashboard shows `undefined` for heap, flash, filesystem, or uptime | Browser cached an old `app.js`. Hard-refresh (Ctrl+F5, or long-press reload on mobile). |
| Can't reach `pacemaker.local` | mDNS is unreliable on Windows and some Android builds. Use `http://192.168.4.1/` directly. |
| Web dashboard is slow or drops requests | Too many browser tabs open on the AP, or signal interference. Only one or two clients should be connected at once for a demo. |