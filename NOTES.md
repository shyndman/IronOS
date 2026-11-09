# FNIRSI HS-02A Firmware Reverse Engineering Notes (`APP_HS_02A_V2.1.1_260112.bin`)

## 1. Binary Layout & Bootloader Header

The update image (`APP_HS_02A_V2.1.1_260112.bin`, 71,724 bytes / `0x1182C`) is **not** a zero-based flash image:
- **Base Address in Flash:** File offset `0x0000` maps to MCU Flash address **`0x08004000`**.
- **File `0x0000..0x000F` (`0x08004000`):** 16-byte FNIRSI image header:
  - `0x00`: `"bin\0"` magic (`62 69 6e 00`) for HS-02A (`"BIN\0"` on HS-02B).
  - `0x04`: `0x00011810` (uint32 LE payload size).
  - `0x08`: `0x0000000000000000` (padding).
- **File `0x0010..0x17FF` (`0x08004010..0x080057FF`):** Entry vector table (`SP = 0x20001EA0`, `Reset_Handler = 0x080059E9`), followed by NOP sled / padding up to `0x1800`.
- **File `0x1800` (`0x08005800`):** Real application Vector Table (`SCB->VTOR` is set to `0x08005800` at the start of `main`).
- **Settings Flash Page:** **`0x0801F800`** (100-byte configuration structure copied to RAM at `0x200004B0` on boot via `sub_7150`, written via `sub_98bc`).

> **Binary Ninja Rebase Note:** Rebase the binary view to **`0x08004000`** so that all absolute flash pointers (`0x0801xxxx` strings/bitmaps and vector table entries) resolve directly. (All function offsets below are listed as `file_offset` / `flash_address`).

---

## 2. MCU & Clocking

- **MCU:** Nation Technologies **N32L403** (ARM Cortex-M4F with hardware single-precision FPU, running at **64 MHz**).
- **Peripheral Base Addresses (N32L403):**
  - `TIM2`: `0x40000000`
  - `TIM3`: `0x40000400`
  - `TIM4`: `0x40000800`
  - `AFIO`: `0x40010000`
  - `GPIOA`: `0x40010800`
  - `GPIOB`: `0x40010C00`
  - `GPIOC`: `0x40011000`
  - `GPIOD`: `0x40011400`
  - `TIM1`: `0x40012C00`
  - `ADC`: `0x40020800`
  - `RCC`: `0x40021000`

---

## 3. Complete Pin Map & Peripheral Usage

| Pin | Mode / Peripheral | Direction | Function | Firmware Details |
| :--- | :--- | :--- | :--- | :--- |
| **PA1** | `ADC_IN2` | Analog In | **Tip Thermocouple** | Sampled via `sub_3db8(2)` in `sub_1adc` (`0x1adc` / `0x08005adc`) while heater PWM is off. Raw ADC stored at `0x200003FA`. |
| **PA2** | `ADC_IN3` | Analog In | *Unused Analog* | Configured as analog input in `sub_18ac`, never read. |
| **PA3** | `GPIOA` Pin 3 (`0x08`) | Output PP | **LCD RST** | Display reset (`sub_6e1c`), pulsed low for 200 ms on init. |
| **PA4** | `GPIOA` Pin 4 (`0x10`) | Output PP | **LCD CS** | Display chip select, active-low (`sub_6f50`, `sub_6f6c`, `sub_6f88`). |
| **PA5** | `GPIOA` Pin 5 (`0x20`) | Output PP | **LCD SCLK** | Bit-banged SPI clock in `sub_de8` (`GPIOA->PBC` @ `+0x28`, `GPIOA->PBSC` @ `+0x18`). Capable of hardware `SPI1_SCK`. |
| **PA6** | `GPIOA` Pin 6 (`0x40`) | Output PP | **LCD DC** | Display Data/Command (`0` = cmd in `sub_6f50`, `1` = data in `sub_6f6c`). |
| **PA7** | `GPIOA` Pin 7 (`0x80`) | Output PP | **LCD MOSI** | Bit-banged SPI MOSI in `sub_de8` driving a 160x80 **ST7735S** TFT. Capable of hardware `SPI1_MOSI`. |
| **PA8** | `TIM1_CH1` (AF2) | Output AF PP | **Heater PWM** | Configured in `sub_49e4` (`PSC=15, ARR=100`). Duty cycle (`0..100`) written to `TIM1->CCR1` (`0x40012C34`) via `sub_4a94` -> `sub_1540`. |
| **PA9** | `GPIOA` Pin 9 (`0x200`) | Output PP | **I2C SCL** | Bit-banged I2C clock (`sub_4330`, `sub_4378`, `sub_43e8`). Capable of hardware `I2C1_SCL`. |
| **PA10** | `GPIOA` Pin 10 (`0x400`) | Bi-dir GPIO | **I2C SDA** | Bit-banged I2C data (`sub_d90` sets input, `sub_dbc` sets output). Connected to **Kionix KXTJ3-1057** accelerometer at I2C 7-bit address **`0x0E`** (`0x1C` W / `0x1D` R). Capable of hardware `I2C1_SDA`. |
| **PA13** | `GPIOA` Pin 13 (`0x2000`) | Output PP | **Status LED A** | Red/Blue dual status LED (`sub_4094`, `sub_2cd4`, `sub_10fc`). **Note:** Overlaps `SWDIO` after a 100 ms boot delay! |
| **PA14** | `GPIOA` Pin 14 (`0x4000`) | Output PP | **Status LED B** | Red/Blue dual status LED (`sub_4094`, `sub_2cd4`). **Note:** Overlaps `SWCLK` after a 100 ms boot delay! |
| **PA15** | `TIM2_CH1` (AF5) | Output AF PP | **Piezo Buzzer** | Configured in `sub_1a44` (`PSC=31, ARR=250` -> 8 kHz PWM). Duty cycle written to `TIM2->CCR1` (`0x40000034`) via `sub_1540`. |
| **PB0** | `ADC_IN9` | Analog In | **DC Input Voltage ($V_{in}$)** | Sampled via `sub_3db8(9)` in `TIM4_IRQHandler` (`sub_10fc`). Multiplied by `3.3 * (1 / 4096) * 11.0` (`11:1` resistor divider) and stored as `double` at `0x20000458`. |
| **PB1** | `ADC_IN10` | Analog In | *Unused Analog* | Configured as analog input in `sub_18ac`, never read. |
| **PB3** | `GPIOB` Pin 3 (`0x08`) | Input | **OK / Middle Button** | Active-low (`sub_71e8`). Held at power-on enters USB MSC bootloader; held at app entry (`0x4da0`) enters factory calibration mode. |
| **PB4** | `GPIOB` Pin 4 (`0x10`) | Input | **Left / Right Button** | Active-low (`sub_61f4`). Swapped with `PB5` based on handedness setting `*0x200004E8` (`sub_401c`). |
| **PB5** | `GPIOB` Pin 5 (`0x20`) | Input | **Right / Left Button** | Active-low (`sub_4bbc`). Pressing `PB3 + PB5` simultaneously returns chord code `0x0E` (`sub_401c`). |
| **PB6** | `GPIOB` Pin 6 (`0x40`) | Output PP | **CH224K CFG3** | USB-PD voltage select bit (`sub_4094`, `sub_9648`). |
| **PB7** | `GPIOB` Pin 7 (`0x80`) | Output PP | **CH224K CFG2** | USB-PD voltage select bit (`sub_4094`, `sub_9648`). |
| **PD14** | `TIM1_CH3N` (AF2) | Output AF PP | **LCD Backlight PWM** | Configured in `sub_4b0c` (`PSC=15, ARR=100`). Brightness (`1..100%`) written to `TIM1->CCR3` (`0x40012C3C`) via `sub_1544`. |
| **PD15** | `GPIOD` Pin 15 (`0x8000`) | Output PP | **Power Latch / Control** | Configured as output push-pull and driven low (`sub_9c4`) in `sub_4094`. |

---

## 4. Hardware Subsystems & Control Loops

### 4.1 USB-PD Negotiation (`WCH CH224K`)
`CFG1` is tied to `GND` on the PCB. The MCU controls `CFG2` (`PB7`) and `CFG3` (`PB6`) in `sub_4094` (boot) and `sub_9648` (voltage menu):

| Setting (`*0x200004D4`) | Requested Voltage | `PB7` (`CFG2`) | `PB6` (`CFG3`) |
| :--- | :--- | :--- | :--- |
| `0` | **9 V** | `0` | `0` |
| `1` | **12 V** | `0` | `1` |
| `2` | **15 V** | `1` | `1` |
| `3` | **20 V** | `1` | `0` |

### 4.2 Heater PWM & Thermocouple Time-Division Multiplexing (`TIM3_IRQHandler`)
- **Timer Setup (`sub_9548`):** `TIM3` (`0x40000400`, IRQ #29) runs at a 10 kHz tick rate (`ARR = 99` -> 10 kHz interrupt) with a modulo-100 software counter (`*0x200003F8`, 10 ms control period / 100 Hz).
- **Multiplexing Sequence (`sub_ffc` / `0x08004ffc`):** Because the JBC C245 cartridge uses shared heater and thermocouple leads:
  1. **Tick 42 (`0x2A`):** Calls `sub_4a94(0)` (`TIM1->CCR1 = 0`) to shut off the heater MOSFET.
  2. **Tick 43 (`0x2B`):** 200 us settling window while inductive kickback and op-amp saturation decay.
  3. **Tick 44 (`0x2C`):** Calls `sub_3f8c()`:
     - Calls `sub_1adc()` to sample `ADC_IN2` (`PA1`), apply 3-point calibration (`*0x20000508`, `*0x2000050C`, `*0x20000510`), and update a 40-sample circular moving average (`0x20000338`, index `0x20000400`, sum `0x20000404`, filtered output `0x20000430`).
     - Checks raw ADC (`*0x200003FA > 4000` / `0xFA0`) to detect **Tip Disconnected** (`*0x200003FD = 1` after 50 consecutive readings).
     - Calls `sub_4624()` to run the PID / adaptive power controller and compute the new duty cycle `0..100`.
     - Calls `sub_4a94(duty)` to turn the heater PWM back on for the remaining ~9.8 ms of the cycle.

### 4.3 System Tick, Buttons, Voltage & Buzzer (`TIM4_IRQHandler`)
- **Timer Setup (`sub_95c8`):** `TIM4` (`0x40000800`, IRQ #30) runs at 1 kHz (`ARR = 999`) with a modulo-10 software divider (`*0x2000040A`, 100 Hz / 10 ms sub-tasks):
  - **Slot 0:** Calls `sub_4170()` (button polling & debouncing: short press = 6..99 ticks, long press >= 100 ticks / 1 s; triggers key beep on `TIM2->CCR1` if beep enabled `*0x200004F0 == 1`; wakes LCD backlight `TIM1->CCR3` and resets backlight timeout `*0x20000484 = 60000`).
  - **Slot 1:** Every 10th pass (10 Hz), samples `ADC_IN9` (`PB0`) for DC input voltage (`*0x20000458`), checks for undervoltage fault (`*0x200003FE = 1`).
  - **Slot 3:** Decrements sleep/standby countdown timers (`*0x20000478`, `*0x2000047C`) and LCD dim timer (`*0x20000484`).
  - **Slot 5:** Increments UI idle counter (`*0x2000040C`) and blinks `PA13` (`0x2000`) at 1 Hz when in standby (`*0x2000049B == 1`).
  - **Slot 6:** Decrements buzzer duration counter (`*0x20000480`); when it hits 0, silences buzzer (`TIM2->CCR1 = 0`).

### 4.4 Display (`ST7735S` 160x80 Color TFT)
- Initialized in `sub_6bc0`:
  - Uses standard ST7735 commands (`0x11` SLPOUT, `0x21` INVON, `0xB1..0xB4` frame rate/inversion, `0xC0..0xC5` power/VCOM, `0x36` MADCTL via `sub_76ec` for screen flip, `0xE0..0xE1` gamma, `0x3A = 0x05` RGB565 16-bit color, `0x29` DISPON).
  - Column/row window offsets are set in `sub_7684` based on screen rotation (`*0x200004E8`: X offset `+1` or `+1`, Y offset `+0x1A` / `26`).

### 4.5 Accelerometer (`Kionix KXTJ3-1057` at I2C `0x0E`)
- Initialized in `sub_a24`:
  - Reads register `0x0C` (`sub_a10`), ORs `0x50` into `CTRL_REG1` (`0x1B`, `sub_bb8`), reads register `0x1B` (`CTRL_REG1`).
- Polled in `sub_af8` (called from `sub_3dfc`):
  - Enables `PC1` (bit 7) in `CTRL_REG1` (`0x1B`, `sub_a46`), reads X/Y/Z acceleration from registers `0x06..0x0B` (16-bit if `RES` bit 6 of `0x1B` is set, else 8-bit from `0x07, 0x09, 0x0B`), and computes tilt angles (`atan2` via `sub_1570`) against previous axes (`0x20000058`, `0x20000060`) to reset sleep/idle timers.

### 4.6 Status LED States (`PA13` & `PA14`)
Controlled in `sub_2cd4` (and `TIM4` slot 5) based on iron state `*0x2000049B`:
- `0` (Idle / Ready): `PA14 = 1`, `PA13 = 0`
- `1` (Standby): `PA14 = 0`, `PA13` toggles every 500 ms (1 Hz blink)
- `2` (Heating / Working): `PA14 = 0`, `PA13 = 1`

---

## 5. Key Function Index

| File Offset | Flash Address | Name / Role |
| :--- | :--- | :--- |
| `0x0500` | `0x08004500` | `ADC_DeInit` / `ADC_Init` (`0x590`) / `ADC_Enable` (`0x524`) |
| `0x0764` | `0x08004764` | `GPIO_InitPeripheral(GPIOx, GPIO_InitStruct*)` |
| `0x09b6` | `0x080049b6` | `GPIO_ReadInputDataBit(GPIOx, pin_mask)` |
| `0x09c4` | `0x080049c4` | `GPIO_ResetBits(GPIOx, pin_mask)` (`GPIOx->PBC = mask`) |
| `0x09c8` | `0x080049c8` | `GPIO_SetBits(GPIOx, pin_mask)` (`GPIOx->PBSC = mask`) |
| `0x0a24` | `0x08004a24` | `Accel_Init()` (I2C addr `0x0E` / `0x1C`) |
| `0x0af8` | `0x08004af8` | `Accel_ReadXYZ(int16_t xyz[3])` |
| `0x0de8` | `0x08004de8` | `LCD_SPI_WriteByte(uint8_t)` (bit-banged on `PA5`/`PA7`) |
| `0x0ffc` | `0x08004ffc` | `TIM3_IRQHandler` (100 Hz heater/thermocouple TDM & PID trigger) |
| `0x10fc` | `0x080050fc` | `TIM4_IRQHandler` (button scan, $V_{in}$ ADC, timers, buzzer, LED blink) |
| `0x1540` | `0x08005540` | `TIM_SetCompare1(TIMx, uint16_t)` (`TIM1->CCR1` heater, `TIM2->CCR1` buzzer) |
| `0x1544` | `0x08005544` | `TIM_SetCompare3(TIMx, uint16_t)` (`TIM1->CCR3` LCD backlight) |
| `0x18ac` | `0x080058ac` | `ADC_Config()` (`PA1`, `PA2`, `PB0`, `PB1`) |
| `0x1a44` | `0x08005a44` | `Buzzer_TIM2_PWM_Init()` (`PA15` / `TIM2_CH1`) |
| `0x1adc` | `0x08005adc` | `TipTemp_ReadAndFilter()` (`ADC_IN2` + 3-pt cal + 40-sample MA) |
| `0x2cd4` | `0x08006cd4` | `SetOperatingState()` (updates UI & status LEDs on `PA13`/`PA14`) |
| `0x3db8` | `0x08007db8` | `ADC_ReadChannel(uint8_t ch)` |
| `0x3dfc` | `0x08007dfc` | `Accel_CheckMovement()` |
| `0x3f8c` | `0x08007f8c` | `Heater_ControlStep()` (reads tip, checks disconnect, runs PID, sets PWM) |
| `0x401c` | `0x0800801c` | `Buttons_ReadRaw()` (`PB3`, `PB4`, `PB5` + handedness swap) |
| `0x4094` | `0x08008094` | `GPIO_And_PD_Init()` (`PA13..14`, `PB6..7`, `PD15`, sets initial CH224K voltage) |
| `0x4170` | `0x08008170` | `Buttons_PollAndDebounce()` |
| `0x4330` | `0x08008330` | `I2C_BitBang_Init()` (`PA9` SCL, `PA10` SDA) |
| `0x4624` | `0x08008624` | `PID_ComputeDuty()` |
| `0x49e4` | `0x080089e4` | `Heater_TIM1_CH1_PWM_Init()` (`PA8`) |
| `0x4a94` | `0x08008a94` | `Heater_SetDuty(uint8_t percent)` |
| `0x4adc` | `0x08008adc` | `Buttons_GPIO_Init()` (`PB3`, `PB4`, `PB5`) |
| `0x4b0c` | `0x08008b0c` | `Backlight_TIM1_CH3N_PWM_Init()` (`PD14`) |
| `0x4d4c` | `0x08008d4c` | `main()` |
| `0x6b94` | `0x0800ab94` | `LCD_GPIO_Init()` (`PA3..PA7`) |
| `0x6bc0` | `0x0800abc0` | `LCD_ST7735_Init()` |
| `0x7150` | `0x0800b150` | `Flash_ReadSettings()` (from `0x0801F800`) |
| `0x8da4` | `0x0800cda4` | `Settings_LoadDefaults()` |
| `0x9548` | `0x0800d548` | `TIM3_Init()` (10 kHz ISR for heater TDM) |
| `0x95c8` | `0x0800d5c8` | `TIM4_Init()` (1 kHz system tick ISR) |
| `0x9648` | `0x0800d648` | `UI_VoltageSelectMenu()` (updates `PB6`/`PB7` CH224K pins) |
| `0x98bc` | `0x0800d8bc` | `Flash_WriteSettings()` (erases & writes `0x0801F800`) |

---

## 6. Hardware Measurements & Community IronOS Port Notes

- **Scott's C245 Tip Resistance (Cold):** `2.60 Ω` (`2.81 Ω` raw minus `0.21 Ω` probe lead resistance).
- **Community IronOS Port Lineage:**
  - [`bugparty/IronOS`](https://github.com/bugparty/IronOS) (`dev` + PR `#13`): Active fork combining `PanKleszcz/IronOS` (N32L403 BSP) and `ok2cm/IronOS` (`color_160x80` ST7735 UI).
  - Missing in `bugparty/IronOS`:
    1. Dynamic USB-PD voltage selection via CH224K (`PB6`/`PB7` currently hardcoded to 20V).
    2. Tip resistance measurement & short-circuit protection via `PB1` (`ADC_IN10`, 10 mΩ current shunt + AD8552 op-amp channel).
  - Temperature calibration quirk in `bugparty/IronOS`: Stock firmware always uses the 3 calibration words at `0x0801F858..0x0801F860` (`*0x20000508..510` minus `0x7F21` / `0x7E77` / `0x7DC4`), where `0x8000` is the factory default curve (`223 / 393 / 572` ADC counts). `bugparty/IronOS` rejects `0x8000` and substitutes a custom `26 uV/C` curve fitted on a single bevel tip.

---

## 7. Verification of Community Fork Claims Against Decompiled `V2.1.1`

Every assumption in `bugparty/IronOS` (`CLAUDE.md` & PR `#13`) was checked directly against the decompiled `APP_HS_02A_V2.1.1_260112.bin` in Binary Ninja:

1. **Thermocouple Piecewise-Linear Curve (`sub_1adc` @ `0x1adc`, literal pool `0x1c2c..0x1c58`) — CONFIRMED:**
   - Calibration anchor words are at `0x20000508` (`+0x58`), `0x2000050C` (`+0x5C`), `0x20000510` (`+0x60`), initialized to `0x8000` in `sub_8da4`.
   - Anchor ADC counts:
     - `r7 = *0x20000508 - 0x7F21` (`223` when `0x8000`) -> `140.0 °C` (`data_1c34 = 140.0`)
     - `r6 = *0x2000050C - 0x7E77` (`393` when `0x8000`) -> `240.0 °C` (`data_1c44 = 240.0`, span `data_1c3c = 100.0`)
     - `r5 = *0x20000510 - 0x7DC4` (`572` when `0x8000`) -> `340.0 °C` (`data_1c4c = 340.0`, span `data_1c3c = 100.0`)
     - 4th point: hardcoded `0x302` (`770` ADC counts) at `0x1b8c` (`subw r0, r5, #0x302`) -> `440.0 °C`.
   - Filtered by a 40-sample circular moving average (`0x20000338`, divisor `data_1c58 = 40.0f`), stored at `*0x20000430` (double) and `*0x20000448` (unfiltered double).
   - **Zero offset subtraction:** Raw `ADC_IN2` (`sub_3db8(2)`) is passed directly into the comparisons with no CJC offset subtracted.

2. **Fixed `+25.0 °C` Cold-Junction Offset (`sub_1954` @ `0x1954`, `sub_4624` @ `0x4624`, `main` @ `0x4d4c`) — CONFIRMED:**
   - `sub_1954` loads `data_1a1c = 25.0` (`0x4039000000000000`) into `*0x20000450`.
   - Both `sub_4624` (`0x46d4`: `error = target - *0x20000430 - *0x20000450`) and `main` (`0x4e9e`: `real_temp = *0x20000430 + *0x20000450`) add `25.0 °C` to the curve output.
   - Therefore, the three calibration points (`140 / 240 / 340` on the curve) correspond to `165 / 265 / 365 °C` regulated tip temperature, while the calibration UI (`sub_9288` @ `0x9524..0x9534`) labels the three tabs `"150℃"`, `"250℃"`, `"350℃"`.

3. **Display Temperature "Snap-to-Setpoint" Window (`main` @ `0x4ec0..0x4f9c`, literal pool `0x5090..0x509c`) — CONFIRMED:**
   - `data_5090 = 10.0` (`d9`), `data_509c = 20.0` (`d1`).
   - When `real_temp` (`*0x20000438`) is within `±10.0 °C` of setpoint (`*0x20000418`), `main` overwrites `*0x20000438` with the exact setpoint (`0x4f96`). Above/below setpoint, it shifts the displayed value by `10.0 °C` toward the setpoint and applies a `20.0 °C` hysteresis against the previous displayed integer (`*0x20000008`).

4. **Input Voltage Divider (`TIM4_IRQHandler` @ `0x1174..0x11a6`, literal pool `0x1294..0x12ac`) — CONFIRMED (corrected earlier note):**
   - Literal pool constants: `data_1294 = 3.3`, `data_129c = 0.000244140625` (`1 / 4096`), `data_12a4 = 11.0`, `data_12ac = 6.0` (undervoltage threshold `6.0 V`).
   - Formula: `Vin = raw_ADC * 3.3 * (1 / 4096) * 11.0` -> **11:1 divider** (e.g. `100 kΩ / 10 kΩ`), matching `VOLTAGE_DIV = 540` in `configuration.h`.

5. **Stock Controller Structure & Power Duty Cap (`sub_4624` @ `0x4624`, literal pool `0x4940..0x49dc`, `.data` @ `0x117c4`) — CONFIRMED:**
   - `.data` image at `0x117c4` (copied to `0x20000000` by `__scatterload` at `0x160`):
     - `*0x20000000 = 64000000` (`64 MHz` clock)
     - `*0x20000018 = 2.38` (`Kp` / delta-error multiplier)
     - `*0x20000020 = 0.81` (`Ki` / proportional error multiplier)
     - `*0x20000028 = 0.28` (`Kd` / second-difference error multiplier)
   - Because `0x47a6` writes `vstr d0, [r4, #0x20]` (`*0x20000030 = output`) instead of accumulating into `*0x20000030`, the stock HS-02A loop is **proportional-only** (not an integrating velocity PID), scaled by `4.5x` (`9V`), `3.0x` (`12V`), or `2.0x` (`15V`).
   - Duty cycle ceiling (`*0x20000410` at `0x47e6..0x485a`) from user power setting `P` (`*0x200004d8`, default `65`):
     - For `V_tier < 20V`: `duty_max = (P * 325.0) / V_tier^2`
     - For `V_tier == 20V` and `P <= 65`: `duty_max = (P * 275.0) / 400.0` (at `P = 65`, `duty_max = 44%`)
     - For `V_tier == 20V` and `P > 65`: `duty_max = (P * 340.0) / 400.0` (at `P = 100`, `duty_max = 85%`)

6. **`PB1` (`ADC_IN10`) Current Sense & `PA2` (`ADC_IN3`) — CONFIRMED:**
   - `sub_18ac` configures `PA1`, `PA2`, `PB0`, `PB1` as analog inputs, but `sub_3db8` (ADC read) is only ever called with channel `2` (`PA1`) and channel `9` (`PB0`). The stock firmware never reads `PB1` (`ADC_IN10`, current sense) or `PA2` (`ADC_IN3`).

7. **Accelerometer Identity (`KXTJ3-1057` at I2C `0x0E`) — CONFIRMED:**
   - Decompilation of `sub_a24`, `sub_a46`, `sub_af8`, and `sub_bb8` confirms the accelerometer is a Kionix **KXTJ3-1057** at 7-bit I2C address `0x0E` (`0x1C` write / `0x1D` read):
     - `sub_a46` toggles `PC1` (bit 7, operating/standby mode) in `CTRL_REG1` (`0x1B`).
     - `sub_bb8(0x50)` clears `PC1`, sets `RES` (bit 6, 12/14-bit resolution) + `DRDYE` (bit 4) in `CTRL_REG1` (`0x1B`), and re-enables `PC1`.
     - `sub_af8` checks `RES` (bit 6 of `0x1B`) and reads 16-bit X/Y/Z from `0x06..0x0B` (or 8-bit from `0x07, 0x09, 0x0B`).
