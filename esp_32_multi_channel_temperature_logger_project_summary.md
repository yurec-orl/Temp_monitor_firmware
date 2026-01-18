# ESP32 Multi‑Channel Temperature Logger – Project Summary

## 1. Project Overview
Portable, battery‑powered, multi‑channel digital thermometer with data logging and on‑device graphing.

Primary use case:
- Profiling temperature behavior of embedded/dashboard electronics
- Evaluating cooling efficiency over time
- Stand‑alone operation with later data download via Wi‑Fi

Key characteristics:
- 4 independent temperature channels
- Continuous logging to internal flash
- Live graph on TFT display
- Battery operation with USB charging

---

## 2. Core Hardware Architecture

### MCU
- **ESP32‑S3** development board
- Native USB (firmware upload + data transfer)
- Integrated Wi‑Fi used for log download

### Sensors
- **DS18B20** digital temperature sensors
- One sensor per OneWire bus (4 total)
- Sensors wrapped in heat‑shrink tubing for electrical isolation and airflow stability

### Display
- **2.8" SPI TFT (ILI9341, 240×320)**
- SPI interface
- Used for live temperature display, menus, and graphing

### Power System
- **1× 18650 Li‑ion cell (~1800 mAh)**
- **TP4056** charge controller with protection
- **TPS63070** buck‑boost converter set to 3.3 V
- ESP32 powered directly from regulated 3.3 V rail

---

## 3. Pin Allocation (Final)

### Temperature Sensors (OneWire)
| GPIO | Channel |
|-----:|---------|
| 4 | Sensor 1 |
| 5 | Sensor 2 |
| 6 | Sensor 3 |
| 7 | Sensor 4 |

Each channel has its own pull‑up resistor.

### Display (SPI)
| GPIO | Function |
|-----:|----------|
| 11 | MISO (SDO) |
| 12 | Backlight (BL) |
| 13 | SCK |
| 14 | MOSI (SDI) |
| 15 | DC |
| 16 | RST |
| 17 | CS |

### Buttons (INPUT_PULLUP)
| GPIO | Button |
|-----:|--------|
| 1 | BTN1 |
| 2 | BTN2 |
| 8 | BTN3 |
| 9 | BTN4 |

### Battery Monitoring
| GPIO | Function |
|-----:|----------|
| 10 | Battery Voltage (ADC) |

Voltage divider: R1=220kΩ, R2=100kΩ (3.2:1 ratio for 4.2V→1.31V)

---

## 4. Firmware Architecture

### Operating Modes
- **Idle** – live temperature display
- **Recording** – periodic sampling + logging
- **Review / Transfer** – Wi‑Fi access to log files

### Logging Design
- Storage: **Internal flash (LittleFS)**
- One recording session = one file
- Fixed sampling rate per session
- Files named sequentially: `LOG_0001.csv`, `LOG_0002.csv`, …

#### Log File Format
- CSV with header metadata
- Relative timestamps based on sampling interval

Example:
```
# channels=4
# sampling_ms=1000
# columns=time_ms,T1,T2,T3,T4
0,23.4,24.1,22.9,23.7
1000,23.5,24.2,23.0,23.8
```

### Flash Management
- Oldest logs deleted automatically when storage limit is reached
- FIFO strategy based on log index

---

## 5. Graphing System

- Live graph: temperature (Y) vs time (X)
- Line plot (sample N to N‑1)
- Fixed time window
- Newest data plotted at the right edge

### Rendering Strategy
- No framebuffer or double buffering (ILI9341 limitation)
- No full redraws
- Plot erasing uses the same drawing logic as plotting, with background/grid colors
- Grid drawn once and preserved

This avoids flicker while keeping implementation simple.

---

## 6. Battery Monitoring System

### Hardware
- **Battery**: Single 18650 Li-ion cell (~1800 mAh)
- **Voltage Measurement**: GPIO 10 (ADC) via resistor divider
- **Voltage Divider**: R1=220kΩ, R2=100kΩ
  - Ratio: 0.3125 (100k / 320k)
  - 4.2V battery → 1.31V at ADC pin (within ESP32 safe range)
- **Charge Controller**: TP4056 with protection circuit

### Firmware Implementation
- **ADC Configuration**: 12-bit resolution, 11dB attenuation (0-3.3V range)
- **Averaging**: 15-sample running average for noise reduction
- **Update Rate**: Every 5 seconds
- **Calibration Factor**: 1.056× (accounts for resistor tolerances and ADC non-linearity)

### Battery States
The system detects 5 distinct battery states based on voltage:

| State | Voltage Range | Display Color | Icon |
|-------|--------------|---------------|------|
| **CHARGING** | ≥4.1V | Cyan | Battery + Lightning Bolt |
| **FULL** | 3.9-4.1V | Green | Full Battery |
| **GOOD** | 3.7-3.9V | White | Partial Battery |
| **LOW** | 3.5-3.7V | Orange | Low Battery |
| **EMPTY** | ≤3.5V | Red | Empty Battery |

### Voltage Thresholds (18650 Li-ion)
- **4.2V**: Maximum safe charge voltage
- **4.1V**: Charging detection threshold
- **3.9V**: Full battery (post-charge rest voltage)
- **3.7V**: Nominal voltage (~40% capacity)
- **3.5V**: Low battery warning (~10% capacity)
- **3.3V**: Empty/cutoff threshold (protect from over-discharge)

### User Interface
1. **Status Bar Icon** (all modes):
   - 20×8 pixel battery icon in top-right corner
   - Fill level proportional to percentage
   - Color matches state
   - Lightning bolt symbol when charging

2. **Standby Mode Display**:
   - Shows exact voltage (3 decimal places): "Batt: 3.850V"
   - State name: "CHARGING", "FULL", "GOOD", "LOW", or "EMPTY"
   - Percentage: 0-100%
   - Color-coded for quick assessment

3. **Serial Debug Output**:
   - Voltage changes >0.05V
   - State transitions
   - Format: `[BATTERY] Voltage: 3.850V, State: GOOD, Percentage: 61%`

### Charging Detection
The system distinguishes between "charging" and "fully charged":
- **During charging**: Battery voltage pushed to ~4.1-4.2V by charge controller
- **Fully charged**: Voltage settles to ~3.9-4.0V after charging stops
- **Threshold**: 4.1V separates these states

This provides clear feedback when the battery is actively charging vs. when it's full and can be disconnected.

### Battery Performance Characteristics (Measured)

#### Discharge Behavior (Standby Mode)
- **Linear discharge phase**: 4.0V → 3.6V over 14.8 hours
- **Rapid drop phase**: 3.6V → 2.83V (shutdown) in ~0.8 hours
- **Total runtime**: ~15.6 hours from full charge to shutdown
- **Discharge curve**: Typical Li-ion behavior with plateau followed by cliff
- **Shutdown voltage**: 2.83V (ESP32 brown-out or voltage regulator cutoff)

**Interpretation:**
- Usable capacity range: 4.0V to 3.6V (94% of runtime)
- Below 3.6V: Remaining capacity <5%, voltage drops rapidly
- Low battery warning (3.5V) provides ~1 hour notice before shutdown
- Average current draw in standby: ~115 mA (calculated from discharge time)

#### Charging Behavior (TP4056)
- **CC (Constant Current) phase**: 3.5 hours until voltage reached 4.33V
- **CV (Constant Voltage) phase**: Duration unknown (charge controller pin not yet connected)
- **Peak voltage during charging**: 4.33V (typical for Li-ion CC/CV charging)
- **Full charge detection**: Cannot be determined without CHRG pin monitoring

**Note:** 
- Total charge time likely 4-5 hours (typical CC+CV for 1800 mAh cell)
- Connecting TP4056 CHRG pin to ESP32 GPIO would enable accurate charge completion detection
- Current 4.1V threshold may detect "charging" but not distinguish "charged and done"

---

## 7. USB & Power Lessons Learned

### Issue
- Powering ESP32 simultaneously from USB and external 3.3 V caused instability and reboots
- USB enumeration failed when VBUS was disconnected

### Solution
- ESP32 powered **only** from regulated 3.3 V
- USB VBUS restored via **Schottky diode** for sensing only
- Prevented back‑feeding and regulator conflicts

---

## 8. Wi‑Fi Antenna Issue & Resolution

### Problem
- Very weak and unstable Wi‑Fi
- ESP32 ceramic antenna placed directly below TFT display (~1–1.5 cm)
- Display ground plane blocked antenna radiation

### Diagnosis
- Removing or moving display restored Wi‑Fi
- Confirmed RF blockage, not firmware or ESP32 fault

### Final Solution
- Rotated ESP32 board **90°**
- Positioned antenna next to enclosure wall
- Antenna oriented outward toward free space

Result:
- Strong, stable Wi‑Fi
- No need for external antenna or board change

---

## 9. Key Design Decisions (Summary)

- ESP32‑S3 chosen over Arduino for Wi‑Fi, flash, and USB
- Separate OneWire bus per sensor for deterministic channel mapping
- Fixed sampling rate per log for simplicity and analysis clarity
- CSV logging for maximum portability
- Incremental graph drawing instead of framebuffer rendering
- Mechanical RF fix preferred over antenna hacks

---

## 10. Final Status

✔ All planned features implemented
✔ Device fully assembled
✔ Stable Wi‑Fi and power behavior
✔ Logging, display, and battery systems working as intended

This project reached a **tool‑grade**, not prototype‑grade, level of completeness.

