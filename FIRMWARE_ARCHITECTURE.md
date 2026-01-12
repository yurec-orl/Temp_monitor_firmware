# ESP32 Temperature Logger - Firmware Architecture

## Document Purpose
This document describes the software architecture, design patterns, and major implementation decisions for the ESP32 multi-channel temperature logger firmware.

---

## Table of Contents
1. [High-Level Architecture](#1-high-level-architecture)
2. [Module Organization](#2-module-organization)
3. [State Management](#3-state-management)
4. [Sensor Reading Strategy](#4-sensor-reading-strategy)
5. [Display Rendering System](#5-display-rendering-system)
6. [Data Logging System](#6-data-logging-system)
7. [WiFi & Web Server](#7-wifi--web-server)
8. [Battery Monitoring](#8-battery-monitoring)
9. [Memory Management](#9-memory-management)
10. [Key Design Decisions](#10-key-design-decisions)
11. [Performance Characteristics](#11-performance-characteristics)

---

## 1. High-Level Architecture

### System Diagram
```
┌─────────────────────────────────────────────────────────────┐
│                      Main Loop (loop())                      │
│  ┌────────────┐  ┌─────────────┐  ┌──────────────────────┐ │
│  │  Button    │  │  Sensor     │  │  Battery Monitor     │ │
│  │  Reading   │  │  Reading    │  │  (5s interval)       │ │
│  └────────────┘  └─────────────┘  └──────────────────────┘ │
└───────────────────────┬─────────────────────────────────────┘
                        │
        ┌───────────────┴───────────────┐
        │     Mode-Based Handler        │
        ├───────────┬──────────┬────────┤
        │  STANDBY  │  RECORD  │  WIFI  │
        └───────────┴──────────┴────────┘
                │         │         │
        ┌───────┴─────────┴─────────┴────────┐
        │        Display System               │
        │  ┌──────────┐  ┌─────────────────┐ │
        │  │ Status   │  │ Graph Rendering │ │
        │  │ Line     │  │ (Incremental)   │ │
        │  └──────────┘  └─────────────────┘ │
        └─────────────────────────────────────┘
```

### Execution Model
- **Single-threaded** with cooperative task scheduling
- **Non-blocking** sensor state machine
- **Event-driven** UI updates (on new sensor data)
- **Periodic** background tasks (battery, presence detection)

---

## 2. Module Organization

### File Structure
```
Temp_monitor_firmware/
├── Temp_monitor_firmware.ino  # Main entry point, setup(), loop()
├── config.h                    # Constants, enums, pin definitions
├── state.h/cpp                 # Global state variables, helper functions
├── hardware.h/cpp              # Hardware object instantiation
│
├── sensor_reader.h/cpp         # Non-blocking DS18B20 state machine
├── battery_monitor.h/cpp       # ADC-based battery voltage monitoring
├── logger.h/cpp                # CSV logging to LittleFS
├── wifi_manager.h/cpp          # WiFi AP + web server
│
├── display.h/cpp               # All TFT rendering functions
├── modes.h/cpp                 # Mode-specific UI/logic handlers
├── buttons.h/cpp               # EasyButton integration
└── ring_buffer.h               # Template circular buffer for graph data
```

### Module Dependencies
```
            ┌─────────────────┐
            │  Main (.ino)    │
            └────────┬────────┘
                     │
        ┌────────────┼────────────┐
        │            │            │
    ┌───▼───┐   ┌───▼───┐   ┌───▼────┐
    │ Modes │   │Buttons│   │ State  │
    └───┬───┘   └───────┘   └───┬────┘
        │                        │
        │  ┌──────────────┬──────┴─────────┬──────────┐
        │  │              │                │          │
    ┌───▼──▼──┐   ┌──────▼─────┐   ┌─────▼─────┐   │
    │ Display │   │   Logger   │   │   WiFi    │   │
    └────┬────┘   └────────────┘   └───────────┘   │
         │                                          │
    ┌────▼─────┐                            ┌──────▼───────┐
    │ Hardware │                            │ SensorReader │
    └──────────┘                            └──────────────┘
                                                   │
                                            ┌──────▼────────┐
                                            │BatteryMonitor │
                                            └───────────────┘
```

### Dependency Rules
1. **Hardware layer** (bottom): No dependencies on application logic
2. **Service layer** (middle): Depends only on hardware and config
3. **Application layer** (top): Can use any lower layer
4. **Circular dependencies**: Avoided through forward declarations and extern references

---

## 3. State Management

### Global State Variables (`state.h/cpp`)

#### Operating Mode
```cpp
OperatingMode g_mode;  // MODE_STANDBY, MODE_RECORD, MODE_WIFI
```

**Transitions:**
- STANDBY ↔ RECORD (Button 2)
- STANDBY ↔ WIFI (Button 4)
- RECORD → STANDBY → WIFI (no direct RECORD→WIFI)

#### Display State
```cpp
DisplayChannel g_displayedChannel;  // Which channel(s) to show on graph
SamplingFrequency g_samplingFreq;   // Current sampling rate
```

#### Sensor Data
```cpp
bool g_channelHasDevice[4];         // Hot-plug detection status
RingBuffer g_sensorValues[4];       // Graph data (261 samples each)
```

#### Graph Range
```cpp
float g_dataMinTemp, g_dataMaxTemp;   // Actual data extremes
float g_graphMinTemp, g_graphMaxTemp; // Rounded axis range
bool g_needMinMaxRecalc;              // Flag for full buffer scan
```

### State Transitions
```
Power On
   │
   └──> STANDBY ──┬──> RECORD ──┐
          ▲       │      ▲      │
          │       │      │      │
          │       └──────┘      │
          │                     │
          └─── WIFI ────────────┘
```

**Transition Actions:**
- **Enter RECORD**: Start logging, clear old data, full screen redraw
- **Exit RECORD**: Stop logging, save file
- **Enter WIFI**: Start AP, start web server, clear screen
- **Exit WIFI**: Stop AP, stop server
- **Enter STANDBY**: Show system status (logs, memory, battery)

---

## 4. Sensor Reading Strategy

### Non-Blocking State Machine
The `SensorReader` class implements a 3-state FSM to avoid blocking the main loop during the 750ms DS18B20 conversion time.

#### State Machine Diagram
```
    IDLE ──────────> CONVERSION ──────────> READY
     ▲                                        │
     │                                        │
     └────────────────────────────────────────┘
          (after data consumed)
```

#### States
1. **IDLE**: Waiting for next sampling interval
   - Checks if `millis() - lastReading >= samplingInterval`
   - Starts conversion 750ms before interval expires
   
2. **CONVERSION**: Temperature conversion in progress
   - Waits for 750ms (DS18B20 requirement)
   - Non-blocking - other tasks continue
   
3. **READY**: Data available
   - Stays in this state until `getReadings()` called
   - Prevents starting new conversion before data consumed

#### Key Methods
```cpp
bool update(samplingInterval);    // Call every loop iteration
bool getReadings(float temps[]);  // Retrieve data, return to IDLE
void requestImmediateReading();   // Force reading (for mode changes)
```

#### Timing Optimization
- Conversion starts 750ms **before** interval expires
- At interval expiration, data is immediately available
- Eliminates perceived lag in UI updates

### Hot-Plug Detection
```cpp
void updatePresenceDetection(presenceInterval);  // Every 2 seconds
void checkDevicePresence();                      // One sensor per call
```

**Design:**
- Checks **one sensor per call** (round-robin)
- Spreads 4 sensors over 4 detection cycles (8 seconds total)
- Avoids OneWire bus interference during conversions
- Automatically skipped when `state == CONVERSION`

**OneWire Scan:**
```cpp
g_sensors[i]->begin();           // Force fresh bus scan
int count = getDeviceCount();    // Performs OneWire search
g_channelHasDevice[i] = (count > 0);
```

---

## 5. Display Rendering System

### Framebuffer-Free Architecture
**Problem:** ILI9341 has no framebuffer, full redraws cause flicker.

**Solution:** Incremental rendering with intelligent update strategies.

### Rendering Layers
```
┌─────────────────────────────────────┐
│  Status Line (top 40px)             │  ← Redrawn on every update
│  - Mode, Channel, Frequency         │
│  - Temperature values               │
│  - Battery icon                     │
├─────────────────────────────────────┤
│  Content Area                       │  ← Mode-specific
│  ┌─────────────────────────────┐   │
│  │ Standby: System Status      │   │
│  │ Record:  Temperature Graph  │   │
│  │ WiFi:    Connection Info    │   │
│  └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

### Graph Rendering Strategy

#### Problem
- 260 samples × 4 channels = 1,040 line segments
- Full redraw = visible flicker + slow (>100ms)

#### Solution: Segment-by-Segment Erase & Draw
```cpp
for each new sample:
    1. Erase old segment (offset=1, black)
    2. Draw new segment (offset=0, color)
```

**Ring Buffer Design:**
- Capacity: 261 samples (260 display + 1 for old state)
- Oldest sample (index 260) used to erase previous line
- Newest sample (index 0) used to draw current line

**Rendering Algorithm:**
```cpp
for (int i = 0; i < displaySize; ++i) {
    // Erase old
    float valOld = buffer.get(1 + i);  // Previous state
    drawLine(prevX, prevY, x, y, BLACK);
    
    // Draw new
    float valNew = buffer.get(0 + i);  // Current state
    drawLine(prevX, prevY, x, y, color);
}
```

**Benefits:**
- No full screen redraws
- No flicker
- Fast updates (<10ms per channel)
- Grid preserved between updates

### Graph Scaling

#### Dynamic Range Calculation
```cpp
// Track data extremes incrementally
if (newValue < g_dataMinTemp) g_dataMinTemp = newValue;
if (newValue > g_dataMaxTemp) g_dataMaxTemp = newValue;

// Recalculate when extreme values pushed out of buffer
if (oldest == g_dataMinTemp || oldest == g_dataMaxTemp) {
    recalculateMinMax();  // Scan entire buffer
}
```

#### Axis Rounding
- Uses predefined span configurations (5°, 10°, 20°, 50°, 100°, 200°, 500°)
- Aligns min/max to nice round numbers (multiples of 5 or 10)
- Ensures exactly 10 grid intervals
- Adds 5% padding around data

**Example:**
- Data range: 23.2°C to 28.7°C (5.5° span)
- Selected span: 10°C (2° intervals)
- Axis range: 20°C to 30°C
- Grid marks: 20, 22, 24, 26, 28, 30

### Status Line Rendering
```cpp
void drawStatusLine(const float tempsC[], int count)
```

**Content:**
- Line 1: MODE, CHANNEL, FREQ, LOG# (if recording)
- Line 2: CH1-4 temperatures with color coding
- Top-right: Battery icon (20×8 pixels)

**Update Strategy:**
- Redrawn on every sensor reading
- Text drawn with background color (no explicit erase)
- Battery icon redrawn completely each time

### Battery Icon
```cpp
void drawBatteryIcon(x, y, width, height)
```

**Design:**
- 20×8 pixel outline + fill + cap
- Fill level proportional to percentage
- Lightning bolt overlay when charging
- 5 color states (cyan/green/white/orange/red)

---

## 6. Data Logging System

### File Format (CSV)
```csv
#Temperature log log_0001
#device=ESP32_TEMPERATURE_LOGGER
#sampling=10s
#timestamp_ms,ch1_c,ch2_c,ch3_c,ch4_c
0,23.4,24.1,22.9,23.7
10000,23.5,24.2,23.0,23.8
20000,23.6,24.3,23.1,23.9
```

**Features:**
- Header with metadata
- Relative timestamps (ms since recording start)
- Empty fields for disconnected sensors
- 2 decimal place precision
- Standard CSV format (Excel/Python compatible)

### File Management

#### Naming Convention
```
/log_0001.csv
/log_0002.csv
/log_0003.csv
...
/log_9999.csv
```

**Sequential numbering:**
- Scans filesystem for highest existing number
- Next log = max + 1
- Wraps at 9999

#### Space Management
```cpp
bool ensureSpace(size_t requiredBytes);
```

**Strategy:**
1. Check free space (total - used)
2. If insufficient:
   - Find oldest log (lowest number)
   - Delete oldest log
   - Recalculate free space
   - Repeat until enough space or no logs left
3. Reserve 100KB minimum for system operations

**Constants:**
- `MIN_FREE_SPACE`: 100KB reserved
- `MAX_LOG_SIZE`: 1MB per file limit
- Average row size: ~35 bytes

#### Write Optimization
```cpp
void logTemperature(timestamp, temps[]);
```

**Buffering:**
- Writes go to internal buffer
- Flush every 10 samples
- Prevents excessive flash writes
- Balances data safety vs. wear leveling

### LittleFS Integration
```cpp
LittleFS.begin(true);  // Auto-format if needed
```

**Filesystem:**
- Wear-leveling built-in
- Power-loss safe (journaling)
- No external SD card needed
- Uses portion of ESP32 flash

---

## 7. WiFi & Web Server

### Access Point Configuration
```cpp
void WiFiManager::startAP(ssid, password)
```

**Initialization Sequence (6 steps):**
```cpp
// STEP 1: Complete shutdown (critical for ESP32-S3)
WiFi.disconnect(true);
WiFi.mode(WIFI_OFF);
delay(500);

// STEP 2: Set mode to AP
WiFi.mode(WIFI_AP);
delay(200);

// STEP 3: Set country code (US for better compatibility)
esp_wifi_set_country(&country);

// STEP 4: Set TX power (19.5 dBm)
WiFi.setTxPower(WIFI_POWER_19_5dBm);

// STEP 5: Start AP
WiFi.softAP(ssid, password, channel, hidden, maxConnections);

// STEP 6: Wait for initialization
delay(500);
```

**Critical Details:**
- Country code **must** be set after `WIFI_AP` mode
- Delays are required for ESP32-S3 stability
- Channel 6 chosen for minimal interference

**Parameters:**
- **SSID**: "ESP32_TempLogger"
- **Password**: "temperature"
- **IP**: 192.168.4.1 (default AP IP)
- **Channel**: 6
- **Max Clients**: 4

### Web Server Routes
```cpp
server.on("/", HTTP_GET, handleRoot);
server.on("/logs", HTTP_GET, handleListLogs);
server.on("/download", HTTP_GET, handleDownloadLog);
server.on("/delete", HTTP_POST, handleDeleteLog);
server.on("/deleteall", HTTP_POST, handleDeleteAllLogs);
```

#### Route Handlers

**1. Root (/):**
```html
<h1>ESP32 Temperature Logger</h1>
<a href="/logs">View Logs</a>
```

**2. List Logs (/logs):**
- Scans LittleFS for all `log_*.csv` files
- Uses `std::vector<LogFileInfo>` for dynamic sizing
- Sorts newest-first using `std::sort`
- Displays table with filename, size, download/delete buttons

**Implementation:**
```cpp
std::vector<LogFileInfo> logs;
// Collect files...
std::sort(logs.begin(), logs.end(), [](const auto& a, const auto& b) {
    return a.filename > b.filename;  // Descending order
});
```

**3. Download (/download?file=log_0001.csv):**
```cpp
response.addHeader("Content-Disposition", 
                   "attachment; filename=\"" + filename + "\"");
server.streamFile(file, "text/csv");
```

**Features:**
- Proper Content-Disposition header (correct download filename)
- Streaming (no RAM buffering of entire file)
- Security checks (filename validation)

**4. Delete (/delete?file=log_0001.csv):**
- POST request (prevents accidental deletion via GET)
- Deletes single log file
- Redirects back to /logs

**5. Delete All (/deleteall):**
- POST request with confirmation
- Uses `std::vector` to collect all log filenames
- Deletes in batch
- Provides deletion count feedback

### WiFi Mode Loop Integration
```cpp
void loop() {
    // ... sensor reading ...
    
    if (g_mode == MODE_WIFI) {
        g_wifiManager.handleClient();  // Process HTTP requests
    }
}
```

**Critical:**
- `handleClient()` must be called frequently (every loop)
- Works independently of sensor timing
- Previously caused blank screen bug with slow sampling (fixed)

---

## 8. Battery Monitoring

### ADC Configuration
```cpp
analogSetAttenuation(ADC_11db);   // 0-3.3V range
analogReadResolution(12);          // 12-bit (0-4095)
```

### Voltage Calculation
```cpp
float adcToVoltage(int adcValue) {
    const float ADC_REFERENCE = 3.3f;
    const float ADC_MAX = 4095.0f;
    
    // Voltage at ADC pin
    float adcVoltage = (adcValue / ADC_MAX) * ADC_REFERENCE;
    
    // Account for voltage divider: (R1 + R2) / R2
    float batteryVoltage = adcVoltage * (BATTERY_R1 + BATTERY_R2) / BATTERY_R2;
    
    // Apply calibration factor
    const float CALIBRATION_FACTOR = 1.056f;
    batteryVoltage *= CALIBRATION_FACTOR;
    
    return batteryVoltage;
}
```

### Running Average Filter
```cpp
float m_voltageBuffer[15];  // Circular buffer
int m_bufferIndex;          // Current write position
int m_bufferCount;          // Samples collected so far
```

**Algorithm:**
1. Read ADC
2. Convert to battery voltage
3. Add to circular buffer
4. Calculate average of all samples in buffer
5. Use average as current voltage

**Benefits:**
- Filters ADC noise
- Smooths rapid fluctuations
- 15 samples over 75 seconds (5s update rate)

### State Detection
```cpp
void updateState() {
    if (voltage >= 4.1V)       state = CHARGING;
    else if (voltage >= 3.9V)  state = FULL;
    else if (voltage >= 3.7V)  state = GOOD;
    else if (voltage >= 3.5V)  state = LOW;
    else                       state = EMPTY;
}
```

**Charging Detection:**
- Voltage ≥4.1V indicates active charging
- Voltage 3.9-4.1V indicates fully charged (not charging)
- Distinguishes charging from charged state

### Integration
```cpp
void setup() {
    g_batteryMonitor.begin();  // Initialize, fill buffer
}

void loop() {
    g_batteryMonitor.update();  // Check every 5 seconds
}
```

**Debug Output:**
```
[BATTERY] Voltage: 3.850V, State: GOOD, Percentage: 61%
```

---

## 9. Memory Management

### Flash (LittleFS)
- **Total**: ~8MB (varies by ESP32-S3 variant)
- **Reserved**: 100KB for system operations
- **Available**: ~7.9MB for logs
- **Usage**: Auto-managed FIFO deletion

**Capacity Estimate:**
- 35 bytes per sample
- 3600 samples/hour at 1s interval
- ~126KB per hour of logging
- ~60 hours of continuous 1Hz logging

### RAM
- **Heap**: ~320KB available
- **Graph Buffers**: 4 × 261 × 4 bytes = ~4KB
- **Web Server**: ~20KB during WiFi mode
- **Sensor Buffers**: Minimal (readings stored immediately)

**Optimization:**
- No framebuffer (would be 240×320×2 = 150KB)
- Minimal string allocations
- Stack-based temporaries where possible

### Ring Buffer Implementation
```cpp
template<typename T, size_t Capacity>
class RingBuffer {
    T buffer[Capacity];
    size_t head;
    size_t count;
};
```

**Features:**
- Fixed size (no heap allocation)
- O(1) add operation
- O(1) get by offset
- Automatic wrap-around
- Type-safe template

---

## 10. Key Design Decisions

### 1. Non-Blocking Sensor Reading
**Decision:** State machine instead of `delay(750)`

**Rationale:**
- Keeps UI responsive during conversions
- Allows button handling during wait
- Enables WiFi to work during slow sampling

**Trade-off:**
- More complex code
- Requires careful state management

**Outcome:** ✅ Successful - eliminates all blocking delays

---

### 2. Separate OneWire Buses
**Decision:** One bus per sensor (4 GPIOs) instead of shared bus

**Rationale:**
- Deterministic channel assignment (no ROM scanning)
- Simpler hot-plug detection
- Independent sensor failures
- No address conflicts

**Trade-off:**
- Uses 4 GPIOs instead of 1
- 4 pull-up resistors instead of 1

**Outcome:** ✅ Successful - reliable channel mapping

---

### 3. Incremental Graph Rendering
**Decision:** Erase-and-draw segments instead of full redraw

**Rationale:**
- ILI9341 has no framebuffer
- Full redraw causes visible flicker
- Segmental updates preserve grid

**Trade-off:**
- Requires extra buffer sample (261 vs 260)
- More complex rendering logic

**Outcome:** ✅ Successful - flicker-free graph updates

---

### 4. CSV Logging Format
**Decision:** CSV with relative timestamps instead of binary

**Rationale:**
- Human-readable
- Excel/Python compatible
- Easy debugging
- No special tools needed

**Trade-off:**
- Larger file size vs binary
- Slower parsing (not critical)

**Outcome:** ✅ Successful - maximum portability

---

### 5. Fixed Sampling Rate Per Log
**Decision:** One sampling rate per recording session

**Rationale:**
- Simplifies analysis (constant Δt)
- Cleaner CSV format
- Avoids irregular time series

**Trade-off:**
- Can't change rate mid-recording
- Must stop/start to change

**Outcome:** ✅ Successful - clear data structure

---

### 6. STL Usage for Log Management
**Decision:** Use `std::vector` and `std::sort` for web server

**Rationale:**
- Dynamic sizing (no 100-file limit)
- Clean, maintainable code
- Standard library well-tested

**Trade-off:**
- Slight heap overhead
- C++ language features required

**Outcome:** ✅ Successful - cleaner than C arrays

---

### 7. WiFi in Main Loop
**Decision:** Call `handleClient()` every loop iteration

**Rationale:**
- Works with any sampling frequency
- Web server responsive during slow sampling (1 hour intervals)
- Avoids blank screen bug

**Trade-off:**
- WiFi code runs even with no clients
- Minimal CPU overhead

**Outcome:** ✅ Successful - fixed critical bug

---

### 8. Battery Monitoring via Voltage
**Decision:** ADC voltage measurement instead of fuel gauge IC

**Rationale:**
- Simpler hardware (2 resistors vs dedicated IC)
- Lower cost
- Sufficient accuracy for 18650 Li-ion

**Trade-off:**
- Less accurate than coulomb counting
- Voltage-to-percentage is non-linear

**Outcome:** ✅ Successful - adequate for user feedback

---

### 9. Running Average for Battery
**Decision:** 15-sample average over 75 seconds

**Rationale:**
- Filters ADC noise
- Smooths transients
- Prevents flickering battery icon

**Trade-off:**
- Slow response to rapid changes
- Delay in detecting low battery

**Outcome:** ✅ Successful - stable readings

---

### 10. Mode-Based Architecture
**Decision:** Separate handlers for each mode

**Rationale:**
- Clear separation of concerns
- Different UI requirements per mode
- Easy to test independently

**Trade-off:**
- Some code duplication (status line)
- Mode transitions require cleanup

**Outcome:** ✅ Successful - maintainable structure

---

## 11. Performance Characteristics

### Timing Analysis

**Main Loop:**
- Iteration time: ~1-5ms (typical)
- Button read: <1ms
- Presence check: ~50ms (when triggered, every 2s)
- Sensor update: <1ms (state machine check)

**Sensor Reading:**
- Conversion time: 750ms (DS18B20 hardware)
- Data retrieval: <5ms
- Total cycle: 750ms (non-blocking)

**Display Rendering:**
- Status line: ~20ms
- Graph axis: ~50ms (full redraw)
- Graph update: ~10ms per channel (incremental)
- Full screen clear: ~30ms

**WiFi Operations:**
- AP initialization: ~1 second
- HTTP request handling: ~10-100ms
- File streaming: ~2-10 KB/s (depends on client)

### CPU Load Estimation
- Idle mode: <5% CPU
- Record mode (1Hz): ~10% CPU
- WiFi mode (no clients): ~15% CPU
- WiFi mode (active download): ~40% CPU

### Flash Wear
- Log writes: Buffered, flushed every 10 samples
- Write cycles: ~1 per 10 seconds at 1Hz sampling
- LittleFS wear leveling: Distributes writes
- Expected life: >10 years continuous operation

### Power Consumption
(Approximate, varies with display brightness)
- ESP32 active: ~80mA
- Display on: ~40mA
- Sensors (4x): ~4mA
- WiFi active: +100mA
- **Total (normal)**: ~124mA
- **Total (WiFi)**: ~224mA

**Battery Life:**
- 1800mAh / 124mA = ~14.5 hours (normal)
- 1800mAh / 224mA = ~8 hours (WiFi)

---

## Conclusion

This firmware demonstrates several key embedded systems principles:

1. **Non-blocking I/O** - State machines for hardware timing
2. **Incremental updates** - Flicker-free rendering without framebuffer
3. **Memory efficiency** - Ring buffers, minimal heap allocation
4. **Separation of concerns** - Modular architecture
5. **Robustness** - Hot-plug detection, automatic space management
6. **Maintainability** - Clear naming, consistent patterns

The architecture successfully balances:
- **Performance** (responsive UI)
- **Resource efficiency** (limited RAM/flash)
- **Maintainability** (clear module structure)
- **Reliability** (error handling, edge cases)

**Total codebase:** ~3,500 lines of C/C++
**Compilation:** Fits comfortably in ESP32-S3 constraints
**Stability:** No known crashes or memory leaks
