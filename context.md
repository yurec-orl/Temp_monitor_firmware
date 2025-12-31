1. High-level overview
An ESP32-S3–based standalone temperature logger/monitor with:

Up to 4 DS18B20 sensors, each on its own dedicated OneWire bus using four GPIO pins (GPIO 37, 38, 39, 40) – one sensor per pin.
2.8" 240x320 SPI TFT with ILI9341 controller (GPIO 11–17 as given).
4 hardware buttons for:
Channel selection (1–4 or ALL).
Mode selection (standby / record / WiFi).
Possibly sampling rate and UI navigation (depending how you map them).
Local CSV logging to internal flash (or SPIFFS/LittleFS/FATFS).
WiFi AP mode + HTTP server to download log files.
Simple UI showing:
Selected channel (or ALL).
Current temps.
Current mode.
Scrolling temp vs time graph.
Assumption: ESP-IDF or Arduino-ESP32 environment; specifics can be adjusted later.

2. System architecture and main components
Think of the firmware as a set of cooperating modules:

Core application / main control

Initializes all subsystems (sensors, display, buttons, FS, WiFi).
Implements the main state machine: STANDBY, RECORD, WIFI.
Coordinates periodic tasks:
Sensor sampling.
UI refresh.
Logging.
Coordinates transitions into/out of WiFi AP mode.
Sensor Manager (DS18B20 / OneWire)

Hardware:
Four independent OneWire data lines: GPIO 37, GPIO 38, GPIO 39, GPIO 40.
Each physical socket/sensor has its own dedicated bus (one sensor per GPIO), so channels map 1:1 to pins instead of sharing a single bus.
Responsibilities:
OneWire bus init for each pin and periodic temperature conversion.
Optionally discover connected sensors on each bus (scan ROM on that pin at startup and maybe on-demand), but because there is only one sensor per pin, the mapping is fixed: channel index 0..3 → GPIO 37..40 respectively.
Handle unplugged sensors:
Timeouts, CRC failures, or missing ROMs → mark as “disconnected”.
Provide simple API:
bool sensorAvailable(int channel)
float getTemperatureC(int channel) (or NAN/flag if missing)
void update() (run conversions/reads periodically or via timer).
Display & UI Module (ILI9341 driver + UI layer)

Display driver:

SPI pins:
GPIO11 → MISO (SDO)
GPIO12 → BL (LED backlight, output, PWM optional)
GPIO13 → SCK
GPIO14 → MOSI (SDI)
GPIO15 → DC
GPIO16 → RST
GPIO17 → CS
Handles low-level drawing:
Init ILI9341.
fillScreen, drawLine, drawText, drawRect, drawGraphPoint, etc.
UI layer:

Knows the layout:
Top area: mode + selected channel.
Middle: numeric temps.
Bottom / main area: scrolling graph.
Responsibilities:
Render static layout (borders, labels).
Render current mode (STANDBY, RECORD, WIFI).
Render selected channel (CH1, CH2, CH3, CH4, or ALL).
Render temperatures:
If in ALL mode, show all 4.
If a single channel selected, highlight that channel, but still may show others smaller/greyed or only one, depending on chosen design.
Render graph:
Temperature vs time for the “active” view:
When selected channel: graph that sensor.
When ALL selected: graph could be multi-line, one color per channel.
Graph scrolls horizontally:
When x position hits right edge, shift/scroll left and continue plotting.
Should be relatively stateless; takes a UIState struct and draws based on it.
Button Input Module

Hardware: 4 GPIOs for buttons (exact pins TBD).
Responsibilities:
GPIO config with internal pull-ups or pull-downs.
Debounce (either via periodic polling or interrupts with software filtering).
Abstracted events:
BUTTON_CHANNEL_NEXT
BUTTON_CHANNEL_PREV (if you assign one)
BUTTON_MODE
BUTTON_RATE or other function depending on your mapping.
Returns events to main loop:
ButtonEvent getNextEvent() or callback/queue.
Mode & Sampling Controller

Application-level state machine managing:
Current mode: STANDBY, RECORD, WIFI.
Current selected channel: 0..3 or ALL.
Current sampling interval (one of:
1 s, 5 s, 10 s, 60 s, 600 s, 3600 s).
Responsibilities:
On button events:
Cycle channel selection.
Cycle modes (Standby → Record → WiFi → Standby).
Cycle sampling intervals (if you dedicate a button).
Enforce semantics:
STANDBY: show temps, no CSV writing.
RECORD: show temps, write to CSV at sampling interval.
WIFI: stop recording, show WiFi status screen.
Provide timestamp / elapsed time for logging and graph (RTC or millis + base).
Logging & Filesystem Module

Backing storage: internal flash (e.g., SPIFFS or LittleFS, or FATFS on partition).
Responsibilities:
Initialize filesystem.
Manage log files:
Create new log file when entering RECORD mode (e.g., log_YYYYMMDD_HHMMSS.csv).
Close file when leaving RECORD or entering WIFI mode.
CSV format example:
Header:
timestamp, t1, t2, t3, t4
Each row at sampling time:
timestamp (e.g., ISO8601 or UNIX seconds)
tN temperature value or blank if sensor unplugged.
Example: 2025-12-28T10:15:00,23.5,24.0,,25.2
Provide APIs:
bool startNewLog(SamplingRate rate)
void appendLogRow(const LogRow&)
void closeLog()
FileList listLogs() (for HTTP server).
WiFi AP & HTTP Server Module

Responsibilities:
When WIFI mode is entered:
Disable recording timers/log file writes.
Turn on WiFi in AP mode (e.g., SSID “TempMonitor_xxx”).
Start a simple HTTP server.
HTTP endpoints:
GET / – simple HTML with list of log files (links).
GET /logs – JSON or HTML listing log filenames/sizes.
GET /log/<filename> – raw CSV download (Content-Type text/csv).
Optional: DELETE /log/<filename> to free space.
When leaving WIFI mode:
Stop HTTP server.
Turn off WiFi.
Return to STANDBY or RECORD depending on design (probably always STANDBY).
Timing & Task Scheduling

A small scheduler or use of FreeRTOS tasks/timers:
Sensor task: reads DS18B20 every N ms based on required resolution (or triggered at logging intervals).
UI task: refreshes at some FPS (e.g., 5–10 Hz) for smooth graph.
Logging task: wakes up at sampling interval to write CSV rows.
Button task: scans/debounces at 50–100 Hz or uses GPIO interrupts plus queue.