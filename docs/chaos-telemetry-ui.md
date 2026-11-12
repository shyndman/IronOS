# Chaos Telemetry UI — Design & Firmware Architecture Spec

Reference specification for the **Chaos Telemetry** (F1 steering wheel / FPV OSD) redesign of IronOS on the FNIRSI HS-02A (`160×80` ST7735S TFT).

---

## 1. Pencil Canvas Map

- **Active Pencil file:** `./design/design.pen` (sync `/home/shyndman/dev/scratch/iron-firmware/IronOS/design/design.pen` if switching worktrees).
- **Top-level frames in `design.pen`:**
  - `mN3h0` (**Frame 2**): Previous baseline UI — **leave untouched** for reference.
  - `E0OsN6` (**Telemetry vs MFD Comparison**): 5×2 exploration board; **Row A (Chaos Telemetry)** selected, **Row B (Boxed MFD)** rejected.
  - `GZhfD` (**Hardest Settings — Chaos Telemetry**): 10-screen edge-case study.
  - `BZBJ9` (**Chaos Telemetry — Full System**): **Canonical 11-column, 53-screen master spec**, laid out on the exact same `(x, y)` grid as `Frame 2` (`x = 29, 229, 429, ..., 2229`; `y = 39`, then `159, 259, ...`).

---

## 2. Visual Language & Constraints

- **Display & Controls:** `160×80` px (`20×10 mm`), operated via physical `Left`, `Right`, and `OK` buttons.
- **Typography:** **Space Grotesk SemiBold (`600`)** restricted strictly to the **4 existing rasterized sizes** in `source/Core/Threads/UI/drawing/color_160x80/Hs02UiAssets.hpp`:
  - `30 px` — Hero live/target temperature digits (`0–9`)
  - `19 px` — Category headers, setting values, fault headlines, toast latches
  - `17 px` — Degree units (`°C` / `°F`), compact multi-anchor debug values
  - `15 px` — Setting titles (1-line up to `138 px` or 2-line `\n`-wrapped), pill badges, detent labels, bottom telemetry, and horizontal help marquee
- **No micro-fonts:** Never introduce `10–13 px` utility sizes inside `160×80` device screens.

### Color Tokens (`hud-*` in `design.pen`)

| Token | Hex | Role |
| :--- | :--- | :--- |
| `hud-bg` | `#0E1319` | Primary obsidian screen background |
| `hud-cell` | `#151C25` | Recessed inactive detent / help viewport surface |
| `hud-cell-hi` | `#1E2734` | Elevated neutral surface |
| `hud-border` | `#2A3647` | Crisp `1 px` neutral structural stroke |
| `hud-track` | `#212B38` | Unlit gauge segment |
| `hud-ink` | `#F4F4F0` | Primary readout & title text; `2 px` target gates |
| `hud-muted` | `#7E8E9F` | Secondary labels, units, inactive detent text |
| `hud-dark-ink` | `#0B1015` | High-contrast dark text/icons on solid bright pills |
| `hud-pwr` / `hud-pwr-dim` | `#EDBF6F` / `#2B2213` | Power category rail, battery/pack tags |
| `hud-heat` / `hud-heat-dim` | `#FF7849` / `#381F18` | Soldering category, active heating, `+` paddle |
| `hud-lock` / `hud-lock-dim` | `#2CE59B` / `#112C22` | Target-lock `Set`, IMU wake/active, safe `OK` confirm |
| `hud-watts` / `hud-watts-dim` | `#FFB547` / `#2B2112` | Heater wattage, hot/cooling caution, peak brightness |
| `hud-volts` / `hud-volts-dim` | `#5CE1E6` / `#11262E` | Interface category, supply voltage, `−` paddle, debug |
| `hud-slp` / `hud-slp-dim` | `#9EA8FF` / `#171C36` | Sleep category, standby hold temperature |
| `hud-adv` / `hud-adv-dim` | `#F19CC4` / `#2F1727` | Advanced category, calibration & keep-awake pulse |
| `hud-redline` / `hud-redline-dim` | `#FF5C77` / `#351722` | Boost redline, MOSFET cutoff, destructive reset, faults |

---

## 3. Display Engine, SRAM & Flash Plan

### Why 4bpp Indexed Needs Upgrading for This Design
- Current `LCD.hpp` uses a `6,400 B` 4bpp buffer (`16` palette slots = `4` anti-aliased ramps × `4` alpha levels baked against a single background color).
- Chaos Telemetry places anti-aliased text/icons inside colored pills (`#2CE59B`, `#FF7849`, `#FFB547`, `#351722`, `#11262E`) and mixes 5–7 hues per frame.

### Strip-Rendered 16bpp RGB565 (`Zero Extra SRAM`)
- **Panel wire format:** `LCD.cpp` already initializes the ST7735S in 16-bit RGB565 (`COLMOD = 0x05`, `LCD.cpp:95`) and streams `25,600 B` of RGB565 per refresh (`LCD.cpp:316-331`). Note `ST7735_INVON` (`LCD.cpp:92`) — RGB565 words are bitwise-inverted (`~rgb`) before SPI send (`LCD.cpp:325`).
- **Scan order:** `MADCTL` is `0x88` / `0x48` (`MV = 0`), so ST7735 `CASET` spans the `80 px` Y axis and `RASET` spans the `160 px` X axis.
- **Buffer replacement:** Repurpose the existing `6,400 B` `LCD::screenBuffer` as a **`40×80` vertical RGB565 strip buffer** (`40 × 80 × 2 = 6,400 B`; 4 strips across `160 px` — or `20×80` = `3,200 B` across 8 strips to save `3.2 KB` SRAM).
- **Command-list replay:** Record a compact per-frame draw command list (~32–64 primitives: `FillRect`, `RoundedPill`, `GlyphRun`, `AaBitmap`, `< 768 B` static struct array):
  1. Hash the recorded command list to preserve `LCD::refresh()`'s dirty-frame skip (avoiding SPI traffic when unchanged).
  2. When dirty, set the `160×80` GRAM window once, rasterize each `40×80` vertical strip with runtime 2-bit alpha blending (`0, 33%, 67%, 100%` over the strip's live RGB565 pixel), and send each strip in one contiguous `SPI_CLASS::fastSend(stripBuf, 6400)` burst (4 bursts total vs 160 column bursts today).

### Memory Budget (`HS02A_EN.elf.map` Baseline)
- **SRAM (`24,576 B` total in `n32l403_flash_user.ld`):** `19,924 B` used today (`4,652 B` free). Strip-16bpp uses the same `6,400 B` buffer (`+ ~768 B` command list -> `~3.9 KB` still free).
- **Flash (`104,448 B` / `102 KB` total):** `62,848 B` (`61.4 KB`) used today; **`41,600 B` (`40.6 KB`) free**.
  - Current UI code total: `~6.8 KB`. Current 4 Space Grotesk fonts: `5.9 KB`.
  - Needed asset additions in `Hs02UiAssets.hpp`: missing ASCII/punctuation glyphs (`~`, `<`, `>`, `=`, `%`, `/`, `→`) in `kFont15`/`kFont19` + small 2-bit alpha icons (`sun`, `rotate-ccw`, `arrow-left-right`, `x`, `check`) ≈ `~1.5 KB`.
  - Shared procedural HUD helpers + 27 setting callbacks ≈ `~4.0 KB` -> **~35 KB Flash still free**.

---

## 4. Reusable Procedural HUD Primitives

All 53 screens compose 5 shared C++ primitives (zero full-screen bitmaps):

1. **`drawSegmentedRail(count, activeIdx, activeRgb, dimRgb)`**: Splits the `3×80` left rail (`x=0..2`) into `N` vertical pips (`3` Power, `5` Soldering, `4` Sleep, `8` Interface, `7` Advanced). Replaces top-right `k/N` text counters so `138 px` 1-line labels (`"Display orientation"`, `"Boot logo duration"`) have the full `x=10..153` width without collision.
2. **`drawPill(x, y, w, h, fillRgb, strokeRgb, label, textRgb)`**: Solid (`stroke = 0`) or tinted+stroked (`1 px` border, `r = 4`) status, context, and softkey badges.
3. **`drawSegmentBar(x, y, w, count, heights[], colors[], gate1X, gate2X)`**: Renders uniform shift bars (`10` or `12` cells, `2 px` gaps), variable-height spectrums (IMU G-bars, nit ramp, strobe wave, luminance decay), and `2 px` protruding setpoint/trigger gates.
4. **`drawDetentStrip(x, y, w, h, labels[], count, activeIdx, activeRgb)`**: Multi-position rotary switch pills (`DC|3S|4S|5S|6S`, `9V|12V|15V|20V`, `Off|Hot|Slp|Idle`, `Off|Boost|Full`).
5. **`drawKeyPaddle(x, y, w, h, keyChar, capRgb, bodyRgb, actionStr)`**: Two-tone F1 hardware rocker paddles with solid `20 px` `L`/`R` end-caps (`Swap + − keys`, `Swap < > keys`).

---

## 5. Screen-by-Screen Specification & Live Firmware Mapping (`BZBJ9`)

### Category Menus (Top Row, `y = 39`)
All 5 cards (`yx9cI`, `Q8I9N`, `KDXMu`, `t1XpX7`, `iwVpq`) show: `3 px` category rail, `19 px` title, 2–3 live summary pills, `39×43` rounded icon badge, and a 5-slot color-coded MFD page bar at `y = 62`:
- **Power (`yx9cI`):** Live `Vin` (`getInputVoltageX10`) + source/tier (`DC` / `3S–6S` / `20V`).
- **Soldering (`Q8I9N`):** `SolderingTemp` (`320°`) + `BoostTemp` (`+420°` or `Off`).
- **Sleep (`KDXMu`):** 3-stage strip (`SleepTemp` `150°` · `SleepTime` `50s` · `ShutdownTime` `10m` or `X` when off) + green IMU dot (`DetectedAccelerometerVersion != None`).
- **Interface (`t1XpX7`):** `°C`/`°F` + `IMU`/`R`/`L` (`OrientationMode`) + `Nit` brightness step.
- **Advanced (`iwVpq`):** `PowerLimit` (`60W`/`Off`) + `VoltageDiv` (`540`).

### Column 1 — Power (`x = 229`, 3-pip `#EDBF6F` rail)
1. **Power source (`wNldv`):** `DC | 3S | 4S | 5S | 6S` detents (`MinDCVoltageCells`); value `DC` or `N cells` + live cyan `Vin` pill.
2. **Minimum voltage (`CTGeN`):** 10-cell LiPo chemistry bar (`2.4–3.7 V`, crimson `<3.0V`, amber `3.1V`, emerald `3.2–3.5V`, gate at `MinVoltageCells`); value `3.3V` + computed pack cutoff pill `(2 + MinDCVoltageCells) * MinVoltageCells` (e.g. `4S 13.2V`).
3. **QC voltage (`m5qQU`):** `9V | 12V | 15V | 20V` tier detents (`QCIdealVoltage`); value `20.0 V` + physics max heater power pill `min(100W, V² / 2.6Ω)` (`31W / 55W / 86W / 100W`).

### Column 2 — Soldering (`x = 429`, 5-pip `#FF7849` rail)
1. **Boost temp (`xbcOT`):** 12-cell `100–450°C` bar with white gate at `SolderingTemp` and crimson span to `BoostTemp`; value `420 °C` (or `Off`) + delta pill `+(BoostTemp - SolderingTemp)°`.
2. **Start-up behavior (`wwS7x`):** `Off | Hot | Slp | Idle` detents (`AutoStartMode` `0..3`); value `Off` / `Solder` / `Sleep` / `Room temp`.
3. **Temp change short (`J0qpz`):** 2-line title + top-right companion `H <LongStep>°` (`x=112, w=41`) + 6-pip step bar; value `<ShortStep>°` + `Tap ±N°` pill.
4. **Temp change long (`Bf8jo`):** 2-line title + top-right companion `T <ShortStep>°` (`x=112, w=41`) + 6-pip step bar; value `<LongStep>°` + `Hold N°` pill.
5. **Button lock (`g2s7i`):** `Off | Boost | Full` detents (`LockingMode`); value + `Armed` / `Off` pill.

### Column 3 — Sleep (`x = 629`, 4-pip `#9EA8FF` rail)
1. **Motion sensitivity (`E3g4p`):** Live 12-bar IMU G-spectrum driven by `MOVThread.cpp`'s 20 Hz `error = |dx|+|dy|+|dz|` history vs `movementThresholdForSensitivity(sens)` white gate; value `7` (or `Off`) + threshold `g` pill + live `WAKE` flash badge (`error > threshold`).
2. **Sleep temp (`DjrFO`):** Stepped 12-bar thermal drop profile (`SleepTemp` gate in white, `SolderingTemp` work gate in coral); value `150 °C` + delta pill `(SleepTemp - SolderingTemp)°` (`−170°`). *(See §6 caveat on top-right `~6W` pill.)*
3. **Sleep timeout (`NVCb0`):** 3-phase timeline: 5 hot `10–50s` cells + white trigger gate + 6 `1–15m` indigo cells + `0W` end chip; value `50 s` / `N min` / `Off` + `→ <SleepTemp>°` pill.
4. **Shutdown timeout (`IHIB0`):** Countdown track with white cutoff gate + alternating crimson hazard cells; value `10 min` / `Off` + solid crimson `OFF` badge.

### Column 4 — Interface (`x = 829`, 8-pip `#5CE1E6` rail)
1. **Temperature unit (`cUQV5`):** Top 2-detent `[Celsius | Fahrenheit]` selector; bottom `< [150] [320] [420] °C >` showing live `SleepTemp`, `SolderingTemp`, and `BoostTemp` preset impact followed by the active unit suffix.
2. **Display orientation (`VKLIJ`):** `R` (emerald right-grip bar) | `L` (coral left-grip bar) | `Auto` detents; value + live KXTJ3 orientation pill (`• Right` / `• Left` via `Display::getRotation()`).
3. **Cooldown flashing (`YlvGR`):** 10-bar square-wave strobe graphic (`#FFB547` high / `#351722` low, emerald `<50°` tail); `[Strobe]` / `[Steady]` pill + `>50°C` threshold pill.
4. **Swap + − keys (`J7B2y`):** Live two-tone paddles `L −<step>°` / `R +<step>°` computed from `Display::getRotation() ^ ReverseButtonTempChangeEnabled`; value `Normal` / `Swapped` + flip icon badge.
5. **Swap < > keys (`fOjuU`):** Live two-tone paddles `L Prev` / `R Next` computed from `Display::getRotation() ^ ReverseButtonSettings`; value `Normal` / `Swapped` + `Std`/`Rev` pill.
6. **Screen brightness (`fXyGe`):** 9-bar stepped nit spectrum (`indigo → cyan → emerald → gold → white`); value `<step> / 9` + gold `sun` percentage pill (`100%`).
7. **Screen timeout (`EH9gr`):** 12-bar white-to-dark luminance decay track scaled across `5–180 s` with blackout gate at `DisplayIdleTimeoutSeconds`; value `30 s` + truthful `LCD off` pill.
8. **Boot logo duration (`zNxvJ`):** Hybrid scale (`Off` crimson chip · 5 stepped `1–5s` bars · `1x` indigo chip · `Inf` gold chip); value + mode badge (`Skip` / `Splash` / `Once` / `Loop`).

### Column 5 — Advanced (`x = 1029`, 7-pip `#F19CC4` rail)
1. **Power limit (`FKCuj`):** 10-step `10–100W` load bar (amber active step, dim crimson `80–100W` tail); value `65 W` / `Off` + live `Vin` tier pill (`20V`).
2. **Calibrate CJC (`YohE3`):** 2-line title + top-right cold-junction offset pill (`|TipInC - HandleInC|°` when cold, or `Hot` if `TipInC >= 50°C`); `Off | Armed` latch pills.
3. **Calibrate input voltage (`dLHlv`):** 2-line title + `Div <VoltageDiv>` (`x=80, y=22`) + bottom `OK · Trim` action pill + live cyan `Vin` pill.
4. **Power pulse (`h37qGu`):** 10-cell `0.5–10W` bar; value `1.5 W` / `Off` + companion interval pill (`<Wait*2.5>s`).
5. **Power pulse delay (`LmMbk`):** 9-cell interval bar; formatted real seconds `KeepAwakePulseWait * 2.5 s` (`10.0 s` instead of raw `"4"`) + `<Pulse>W` pill.
6. **Power pulse duration (`JzHZ5`):** 2-line title + top-right `<Pulse>W` pill & 9-step mini-bar; formatted real ms `KeepAwakePulseDuration * 250 ms` (`750 ms` instead of raw `"3"`) + `<Wait>s` pill.
7. **Restore default settings (`zYlM1`):** 2-line title + crimson warning icon + `OK · Reset all` destructive launcher pill.

### Columns 6–11 — Interactive Modes, Modals, Home, Alerts & Debug
- **Col 6 (`x = 1229`):**
  - **Set temperature (`oySjF`):** Live cyan `• <Tip>°` pill at top-right; framed `+` / `−` badges (swapping with `ReverseButtonTempChangeEnabled`); `30 px` target; 12-cell `100–450°` scale showing cyan live tip, coral target span to white gate, and crimson `BoostTemp` marker.
  - **Active VIN calibrate (`gjg7Y`):** Live cyan `• 20.0 V` pill; 11-tick center-zero vernier around nominal `540`; `< 543 [OK Save] >`.
- **Col 7 (`x = 1429`):**
  - **Restore defaults confirm (`P6crkc`):** Crimson hazard dashes + split softkeys: crimson `[↺ OK]` vs cyan `[Hold ×]`.
  - **Calibrate CJC confirm (`B2N9lb`):** 2-line `"Tip & handle\ncold?"` + live `HandleTemp` pill (`25.4°` emerald if cold, amber/red if hot) + emerald `[✓ OK]` vs neutral `[Hold ×]`.
  - **CJC sampling (`x8Ltf`):** 12-cell progress bar (`num_dots`) + live `TipRaw uV` (cyan) and `Handle °C` (emerald).
- **Col 8 (`x = 1629`):**
  - **Settings help (`i1qF4`, `BWslZ`):** Keeps segmented rail, muted setting title, and current value pill pinned at top; `2 px` marquee scroll-progress track at `y = 30`; `15 px` scrolling description inside recessed `144×32` viewport at `y = 38`.
- **Col 9 (`x = 1829`) — All 7 Home States (`V1VYg`–`pSNvk`):**
  - Live 12-segment shift bar (`100–450°C`, `cyan → emerald → amber → coral → crimson`) filling to current tip temperature, with a `2×10 px` white gate at active target (`SolderingTemp`, `BoostTemp`, or `SleepTemp`).
  - State pills: outlined cyan `Ready` (idle), solid emerald `Set` (within `±10°C` window), solid coral `Heat` (climbing), solid crimson `Boost` (+ `MAX` tag), solid indigo `Sleep` (+ coral wake-target gate & crimson shutdown countdown pill), outlined amber `Hot` (`142°` amber digits while cooling `≥50°C`), and alternating amber hazard bar + `Open` pill on `No tip` (while keeping live cyan `20.0 V`).
- **Col 10 (`x = 2029`) — Faults (`mQKOX`, `JZkit`, `wp2lm`) vs Status Toasts (`t8u7Il`, `rIKLB`, `e9Tf85`):**
  - **Faults:** Top 10-dash hazard stripe + `triangle-alert` + `19 px` fault detail + hardware state pills (`FET OFF`, `< 6.0V`, `OK`).
  - **Toasts:** Calm category header + wide `142×34` centered latch badge (`Locked`, `Unlocked`, `Saved`).
- **Col 11 (`x = 2229`) — Debug (`wIyRo`, `N9Z0I`, `zu1rb`, `S5nPEl`):**
  - Adds `k/17` pill + contextual bars/chips: `0–22V` bar on Input voltage (`5/17`), `140° / 240° / 340°` chips above `hs02GetFactoryTipCal` counts (`17/17`), `N32L403 · 64MHz` subtitle above hex `getDeviceID()` (`2/17`), and stack headroom bar + `% ok` pill on RTOS stack pages (`14–16/17`).

---

## 6. Known Hardware Caveats to Resolve During Implementation

1. **`Alert — Tip shorted` (`0.2 Ω` pill on `JZkit`):** `isTipShorted()` in `source/Core/BSP/Fnirsi/BSP.cpp:217` currently returns `false` and `getTipResistanceX10()` returns constant `26` (`2.6 Ω`). `PB1` (`ADC_IN10` current shunt) is DMA-sampled via `getADCCurrent()`, so either calibrate/wire live `V/I` short detection first or omit the `0.2 Ω` pill until wired.
2. **`"PD"` source tags (`yx9cI`, `wIyRo`):** The WCH CH224K sink chip (`source/Core/BSP/Fnirsi/Power.cpp:9-54`) is GPIO pin-strapped (`CFG2`/`CFG3`) with no protocol readback; display actual settled tier (`20V OK` / `Req 20V` via `CH224KVoltageSettled()`, or `DC`/`3S–6S` when battery cutoff is active) rather than claiming USB-PD vs QC vs DC brick.
3. **`Sleep temp` (`~6W` pill on `DjrFO`) & `Calibrate CJC` (`6.2° drift` on `YohE3`):** Inside Settings the heater is off (`x10WattHistory == 0`), so replace `~6W` with deterministic data (e.g. `Work 320°` or `Wake IMU`) rather than an unmeasured estimate; on `Calibrate CJC`, only show `|Tip - Handle|°` drift when `TipInC < 50°C`, switching the pill to `Hot <Tip>°` if opened while still hot.
