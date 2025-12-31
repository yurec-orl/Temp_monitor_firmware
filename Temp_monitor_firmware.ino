#include <Arduino.h>

// Display libraries
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// DS18B20 libraries
#include <OneWire.h>
#include <DallasTemperature.h>

#include "FS.h"
#include <LittleFS.h>

// --- Operating modes ---------------------------------------------------------

enum OperatingMode {
  MODE_STANDBY = 0,
  MODE_RECORD,
  MODE_WIFI
};

enum SamplingFrequency {
  SAMPLING_FREQ_1S = 0,
  SAMPLING_FREQ_5S,
  SAMPLING_FREQ_10S,
  SAMPLING_FREQ_60S,
  SAMPLING_FREQ_600S,
  SAMPLING_FREQ_3600S
};

enum DisplayChannel {
  CHANNEL_1 = 0,
  CHANNEL_2,
  CHANNEL_3,
  CHANNEL_4,
  CHANNEL_ALL
};

// Global current mode (start in standby by default)
OperatingMode g_mode = MODE_RECORD;

// Global current sampling frequency
SamplingFrequency g_samplingFreq = SAMPLING_FREQ_1S;

// Global current channel displayed on graph
DisplayChannel g_displayedChannel = CHANNEL_ALL;

// Forward declarations for mode handlers
void handleStandbyMode(const float tempsC[]);
void handleRecordMode(const float tempsC[]);
void handleWifiMode(const float tempsC[]);

// Refresh device presence information
void refreshDevicePresence();

// Read temperatures from all channels into provided array
void readTemperatures(float tempsC[], int count);

// Draw 2-line status (mode/channel + sensor values)
void drawStatusLine(const float tempsC[], int count);

// Draw temperature/time graph area
void drawGraph(const float tempsC[], int count);

// Number of DS18B20 sensor channels
constexpr int SENSOR_COUNT = 4;

// Size of graph buffer (number of samples) - equals to number of pixels on graph
// 1 pixel = 1 sample
constexpr int GRAPH_BUFFER_SIZE = 260;

// Graph layout margins
constexpr int16_t GRAPH_TOP_MARGIN    = 40;
constexpr int16_t GRAPH_LEFT_MARGIN   = 30;
constexpr int16_t GRAPH_RIGHT_MARGIN  = 30;
constexpr int16_t GRAPH_BOTTOM_MARGIN = 20;

// Channel graph colors
constexpr uint16_t g_channelColors[SENSOR_COUNT] = {
  ILI9341_YELLOW,
  ILI9341_BLUE,
  ILI9341_ORANGE,
  ILI9341_MAGENTA
};

// --- Hardware pins -----------------------------------------------------------

// TFT display pins
static const int PIN_TFT_MISO = 11;  // SDO (not always needed)
static const int PIN_TFT_LED  = 12;  // Backlight
static const int PIN_TFT_SCK  = 13;  // SCK
static const int PIN_TFT_MOSI = 14;  // MOSI (SDI)
static const int PIN_TFT_DC   = 15;  // D/C
static const int PIN_TFT_RST  = 16;  // RESET
static const int PIN_TFT_CS   = 17;  // CS

// Sensor channel pins (4-7)
static const int PIN_DS18B20_1 = 4;
static const int PIN_DS18B20_2 = 5;
static const int PIN_DS18B20_3 = 6;
static const int PIN_DS18B20_4 = 7;

// Display object (hardware SPI)
Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

// OneWire and DallasTemperature objects for four sensors (one per pin)
OneWire oneWire1(PIN_DS18B20_1);
OneWire oneWire2(PIN_DS18B20_2);
OneWire oneWire3(PIN_DS18B20_3);
OneWire oneWire4(PIN_DS18B20_4);

DallasTemperature sensors1(&oneWire1);
DallasTemperature sensors2(&oneWire2);
DallasTemperature sensors3(&oneWire3);
DallasTemperature sensors4(&oneWire4);

// Array of pointers to DallasTemperature objects for easier iteration
static DallasTemperature* g_sensors[SENSOR_COUNT] = {
  &sensors1,
  &sensors2,
  &sensors3,
  &sensors4
};

// Per-channel presence flags (updated via refreshDevicePresence())
// Since sensors report wrong temperature when first plugged in,
// we need to have incrementing counters instead of boolean flags.
// Flag becomes 1 when device is detected, 2 after first reading.
static uint8_t g_channelHasDevice[SENSOR_COUNT] = { 0, 0, 0, 0 };

// Simple timing for temperature refresh
unsigned long g_lastTempRequestMs = 0;
const unsigned long TEMP_UPDATE_INTERVAL_MS = 1000; // 1 second

// How often to refresh presence information (in ms)
const unsigned long PRESENCE_REFRESH_INTERVAL_MS = 2000;
unsigned long g_lastPresenceRefreshMs = 0;

// Sensor values buffer for graph plotting
float g_sensorValues[SENSOR_COUNT][GRAPH_BUFFER_SIZE];
int g_sensorValueIndex = 0;

// Sensor physical measurement limits
constexpr float DS18B20_MIN_TEMP = -55.0f;
constexpr float DS18B20_MAX_TEMP = 125.0f;

// Dynamic scaling state
float g_dataMinTemp = 9999.0f;
float g_dataMaxTemp = -9999.0f;

// Current graph range actually used for drawing
float g_graphMinTemp = DS18B20_MAX_TEMP;
float g_graphMaxTemp = DS18B20_MIN_TEMP;

void setup() {
  // Basic serial for debug
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Starting ILI9341 + DS18B20 4-channel monitor...");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }

  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();

  Serial.print("LittleFS total bytes: ");
  Serial.println(total);
  Serial.print("LittleFS used bytes: ");
  Serial.println(used);

  // Configure backlight pin
  pinMode(PIN_TFT_LED, OUTPUT);
  digitalWrite(PIN_TFT_LED, HIGH);  // Turn backlight on (assuming active-high)

  // Reconfigure SPI pins for the display (Arduino-ESP32)
  SPI.begin(PIN_TFT_SCK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);

  // Initialize the display
  tft.begin();
  tft.setRotation(3);  // Landscape
  tft.fillScreen(ILI9341_BLACK);

  // Initialize DS18B20 sensors on GPIO 4-7
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    g_sensors[i]->begin();
    g_sensors[i]->setResolution(12); // Optional: 9–12 bits; 12 is default and slowest
  }

  // Initial presence scan
  refreshDevicePresence();

  Serial.println("Initialization complete.");
}

void loop() {
  unsigned long now = millis();

  // Periodically refresh presence to support hot-plug
  if (now - g_lastPresenceRefreshMs >= PRESENCE_REFRESH_INTERVAL_MS) {
    g_lastPresenceRefreshMs = now;
    refreshDevicePresence();
  }

  if (now - g_lastTempRequestMs >= TEMP_UPDATE_INTERVAL_MS) {
    g_lastTempRequestMs = now;

    float tempsC[SENSOR_COUNT];
    readTemperatures(tempsC, SENSOR_COUNT);

    // Mode-specific handling
    switch (g_mode) {
      case MODE_STANDBY:
        handleStandbyMode(tempsC);
        break;
      case MODE_RECORD:
        handleRecordMode(tempsC);
        break;
      case MODE_WIFI:
        handleWifiMode(tempsC);
        break;
      default:
        break;
    }

    // Serial debug stays the same
    Serial.print("Temps: ");
    for (int i = 0; i < SENSOR_COUNT; ++i) {
      Serial.print("CH");
      Serial.print(i + 1);
      Serial.print("=");
      if (!g_channelHasDevice[i] || tempsC[i] == DEVICE_DISCONNECTED_C) {
        Serial.print("N/A ");
      } else {
        Serial.print(tempsC[i]);
        Serial.print("C ");
      }
    }
    Serial.println();
  }
}

// Refresh presence info for each bus using getDeviceCount()
void refreshDevicePresence() {
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    if (!g_channelHasDevice[i]) {
      g_sensors[i]->begin();
    }

    int count = g_sensors[i]->getDeviceCount();
    bool present = (count > 0);

    if (present != (g_channelHasDevice[i]?true:false)) {
      // Presence changed; log it once
      Serial.print("Channel ");
      Serial.print(i + 1);
      Serial.print(present ? " attached" : " detached");
      Serial.println();
    }

    if (!present) {
      g_channelHasDevice[i] = 0; // No device
    } else if (!g_channelHasDevice[i]) {
      g_channelHasDevice[i] = 1;
    }
  }
}

// Read temperatures for all channels into tempsC array
void readTemperatures(float tempsC[], int count) {
  // 1) Request temperature conversion on all buses that currently have a device
  for (int i = 0; i < count && i < SENSOR_COUNT; ++i) {
    if (g_channelHasDevice[i]) {
      g_sensors[i]->requestTemperatures();
    }
  }

  // 2) Wait long enough for 12-bit conversion to complete on all sensors
  delay(750); // adjust if you change resolution

  // 3) Read results from each bus (or mark as disconnected if no device)
  for (int i = 0; i < count && i < SENSOR_COUNT; ++i) {
    if (g_channelHasDevice[i] == 2) {
      tempsC[i] = g_sensors[i]->getTempCByIndex(0);
    } else if (g_channelHasDevice[i] == 1) {
      tempsC[i] = DEVICE_DISCONNECTED_C;      // Sensor is still initializing
      g_channelHasDevice[i] = 2;
    } else {
      tempsC[i] = DEVICE_DISCONNECTED_C;
    }
  }
}

// Draw 2-line status at top of the content area
void drawStatusLine(const float tempsC[], int count) {
  const int16_t baseX = 2;
  const int16_t baseY = 2;

  tft.setTextSize(1);

  // Row 1: MODE + CHANNEL
  tft.setCursor(baseX, baseY);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);

  // Mode
  tft.print("MODE: ");
  switch (g_mode) {
    case MODE_STANDBY: tft.print("STDBY"); break;
    case MODE_RECORD:  tft.print("REC");   break;
    case MODE_WIFI:    tft.print("WIFI");  break;
    default:           tft.print("?");     break;
  }

  // Channel to display on graph (placeholder: ALL for now)
  tft.print("  CH: ");
  switch (g_displayedChannel) {
    case CHANNEL_1: tft.print("  1"); break;
    case CHANNEL_2: tft.print("  2"); break;
    case CHANNEL_3: tft.print("  3"); break;
    case CHANNEL_4: tft.print("  4"); break;
    case CHANNEL_ALL: tft.print("ALL"); break;
    default: tft.print("?"); break;
  }

  // Sampling rate
  tft.print("  FREQ: ");
  switch (g_samplingFreq) {
    case SAMPLING_FREQ_1S:   tft.print("1s");   break;
    case SAMPLING_FREQ_5S:   tft.print("5s");   break;
    case SAMPLING_FREQ_10S:  tft.print("10s");  break;
    case SAMPLING_FREQ_60S:  tft.print("1m");   break;
    case SAMPLING_FREQ_600S: tft.print("10m");  break;
    case SAMPLING_FREQ_3600S:tft.print("1h");   break;
    default:                 tft.print("?");    break;
  }

  // Row 2: sensor values
  tft.setCursor(baseX, baseY + 16); // next text row

  for (int i = 0; i < count && i < SENSOR_COUNT; ++i) {
    // Label
    tft.setTextColor(g_channelColors[i], ILI9341_BLACK);
    tft.print("CH");
    tft.print(i + 1);
    tft.print(":");

    // Value
    if (!g_channelHasDevice[i] || tempsC[i] == DEVICE_DISCONNECTED_C) {
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
      tft.print("---.-  ");
    } else {
      tft.setTextColor(g_channelColors[i], ILI9341_BLACK);
      char buf[8];
      dtostrf(tempsC[i], 5, 1, buf);
      tft.print(buf);
      tft.print(" C");
    }

    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    tft.print("  "); // small spacer between channels
  }
}

// Helper function to get sampling interval in seconds
int getSamplingIntervalSeconds() {
  switch (g_samplingFreq) {
    case SAMPLING_FREQ_1S:    return 1;
    case SAMPLING_FREQ_5S:    return 5;
    case SAMPLING_FREQ_10S:   return 10;
    case SAMPLING_FREQ_60S:   return 60;
    case SAMPLING_FREQ_600S:  return 600;
    case SAMPLING_FREQ_3600S: return 3600;
    default:                  return 1;
  }
}

// Helper function to format time duration
void formatTimeDuration(int seconds, char* buffer, size_t bufSize) {
  if (seconds < 60) {
    snprintf(buffer, bufSize, "%ds", seconds);
  } else if (seconds < 3600) {
    int mins = seconds / 60;
    snprintf(buffer, bufSize, "%dm", mins);
  } else {
    int hours = seconds / 3600;
    snprintf(buffer, bufSize, "%dh", hours);
  }
}

void drawGraphAxis(float minTemp, float maxTemp) {
  // Reserve top area for status lines; graph starts below
  const int16_t x0 = GRAPH_LEFT_MARGIN;
  const int16_t x1 = tft.width() - GRAPH_RIGHT_MARGIN;   // max time
  const int16_t yTop = GRAPH_TOP_MARGIN;                  // top of graph
  const int16_t yBottom = tft.height() - GRAPH_BOTTOM_MARGIN; // bottom of graph

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);

  const float range = maxTemp - minTemp;
  if (range <= 0.0f) {
    return; // invalid range
  }

  const int16_t graphWidth  = x1 - x0;
  const int16_t graphHeight = yBottom - yTop;

  uint16_t gridColor = tft.color565(48, 48, 48);

  // Horizontal grid: divide temperature range into 10 intervals
  for (int i = 1; i <= 10; ++i) {
    float frac = i / 10.0f; // 0.1 .. 0.9
    int16_t y = yBottom - (int16_t)(frac * graphHeight + 0.5f);
    tft.drawFastHLine(x0, y, x1 - x0, gridColor);
  }

  // Vertical grid lines every 20 samples across full graph width
  for (int x = x0 + 20; x <= x1; x += 20) {
    tft.drawFastVLine(x, yTop, graphHeight, gridColor);
  }

  // Draw Y axis (full range) and X axis always at the bottom
  tft.drawFastVLine(x0, yTop,    yBottom - yTop, ILI9341_WHITE); // Y axis (temp)
  tft.drawFastHLine(x0, yBottom, x1 - x0,        ILI9341_WHITE); // X axis (time at bottom)

  // Mark min, max and middle labels on Y axis
  tft.setCursor(0, yBottom - 4);
  tft.print((int)minTemp);
  tft.print("C");

  tft.setCursor(0, yTop - 4);
  tft.print((int)maxTemp);
  tft.print("C");

  tft.setCursor(0, (yBottom + yTop) / 2 - 4);
  tft.print((int)((minTemp + maxTemp) / 2));
  tft.print("C");

  // Time marks along X axis - display every 3 grid lines (every 60 pixels)
  // Grid lines are every 20 pixels, so time marks at 60, 120, 180, etc.
  int samplingInterval = getSamplingIntervalSeconds();
  char timeBuffer[10];
  
  for (int x = x0 + 60; x <= x1; x += 60) {
    // Calculate time in seconds: (pixels from start) × (seconds per pixel)
    int pixelsFromStart = x - x0;
    int totalSeconds = pixelsFromStart * samplingInterval;
    
    // Format the time duration
    formatTimeDuration(totalSeconds, timeBuffer, sizeof(timeBuffer));
    
    // Display the time mark below the X axis
    // Center the text around the grid line
    int textWidth = strlen(timeBuffer) * 6; // Approximate width (6 pixels per char at text size 1)
    tft.setCursor(x - textWidth / 2, yBottom + 2);
    tft.print(timeBuffer);
  }
}

void drawGraph(const float tempsC[], int count, float minTemp, float maxTemp, uint16_t color) {
  if (count <= 1) return;

  const int16_t x0 = GRAPH_LEFT_MARGIN;
  const int16_t x1 = tft.width() - GRAPH_RIGHT_MARGIN;
  const int16_t yTop = GRAPH_TOP_MARGIN;
  const int16_t yBottom = tft.height() - GRAPH_BOTTOM_MARGIN;

  const int16_t graphWidth  = x1 - x0;
  const int16_t graphHeight = yBottom - yTop;

  const float range = maxTemp - minTemp;
  if (range <= 0.0f) {
    return; // invalid range
  }

  auto tempToY = [&](float t) -> int16_t {
    if (t < minTemp) t = minTemp;
    if (t > maxTemp) t = maxTemp;
    float norm = (t - minTemp) / range;
    return yBottom - (int16_t)(norm * graphHeight + 0.5f);
  };

  // We know GRAPH_BUFFER_SIZE == graphWidth by design (1 pixel = 1 sample).
  // Align the newest sample at the right, older ones shift left.
  int visibleCount = count;
  if (visibleCount > graphWidth) visibleCount = graphWidth;

  int startIndex = count - visibleCount;  // first index to show

  bool havePrev = false;
  int16_t prevX = 0;
  int16_t prevY = 0;

  for (int i = 0; i < visibleCount; ++i) {
    float val = tempsC[startIndex + i];

    if (val == DEVICE_DISCONNECTED_C) {
      havePrev = false;
      continue;
    }

    int16_t x = x0 + i;          // 1 sample = 1 pixel
    int16_t y = tempToY(val);

    if (havePrev) {
      tft.drawLine(prevX, prevY, x, y, color);
    }

    prevX = x;
    prevY = y;
    havePrev = true;
  }
}

// Update the graph range based on current data
// Returns true if the graph range was updated
bool updateGraphRange() {
  // If we haven't seen any valid data yet, keep defaults
  if (g_dataMinTemp > g_dataMaxTemp) {
    return false;
  }

  // Check if current data is outside current graph range
  bool below = g_dataMinTemp < g_graphMinTemp;
  bool above = g_dataMaxTemp > g_graphMaxTemp;

  if (!below && !above) {
    // All data still fits in current view; don't change scale
    return false;
  }

  // Expand to new range based on data, with some padding
  float newMin = g_dataMinTemp;
  float newMax = g_dataMaxTemp;

  // Add 10% margin on each side
  float span = newMax - newMin;
  if (span < 5.0f) span = 5.0f; // avoid zero/very small span

  float pad = span * 0.1f;
  newMin -= pad;
  newMax += pad;

  // Clamp to DS18B20 physical limits
  if (newMin < DS18B20_MIN_TEMP) newMin = DS18B20_MIN_TEMP;
  if (newMax > DS18B20_MAX_TEMP) newMax = DS18B20_MAX_TEMP;

  // Round to nice multiples of 5
  newMin = floorf(newMin / 5.0f) * 5.0f;
  newMax = ceilf (newMax / 5.0f) * 5.0f;

  bool result = false;

  if (newMin != g_graphMinTemp || newMax != g_graphMaxTemp) {
    result = true;
  }

  g_graphMinTemp = newMin;
  g_graphMaxTemp = newMax;

  return result;
}

// --- Mode handlers (blank skeletons for now) ---------------------------------

void handleStandbyMode(const float tempsC[]) {
  // Standby: just show status line, no graph yet
  drawStatusLine(tempsC, SENSOR_COUNT);
}

void handleRecordMode(const float tempsC[]) {
  // Record mode: status + graph
  
  // Store current temperature in shifting buffer. When index reaches GRAPH_BUFFER_SIZE, shift buffer
  // down by half of its size.
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    float v = tempsC[i];
    g_sensorValues[i][g_sensorValueIndex] = v;

    if (v != DEVICE_DISCONNECTED_C) {
      if (v < g_dataMinTemp) g_dataMinTemp = v;
      if (v > g_dataMaxTemp) g_dataMaxTemp = v;
    }
  }

  g_sensorValueIndex++;
  if (g_sensorValueIndex >= GRAPH_BUFFER_SIZE) {
    g_sensorValueIndex = GRAPH_BUFFER_SIZE / 2;
    
    // Shift buffer
    for (int j = 0; j < SENSOR_COUNT; ++j) {
      memmove(g_sensorValues[j], g_sensorValues[j] + GRAPH_BUFFER_SIZE / 2, (GRAPH_BUFFER_SIZE / 2) * sizeof(float));
    }
    
    // Update min/max temp
    g_dataMaxTemp = DS18B20_MIN_TEMP;
    g_dataMinTemp = DS18B20_MAX_TEMP;

    for (int i = 0; i < SENSOR_COUNT; ++i) {
      for (int j = 0; j < g_sensorValueIndex; ++j) {
        float v = g_sensorValues[i][j];
        if (v != DEVICE_DISCONNECTED_C) {
          if (v < g_dataMinTemp) g_dataMinTemp = v;
          if (v > g_dataMaxTemp) g_dataMaxTemp = v;
        }
      }
    }

    // Reset graph range so it could be recalculated with new min/max temp
    g_graphMinTemp = DS18B20_MAX_TEMP;
    g_graphMaxTemp = DS18B20_MIN_TEMP;

    tft.fillRect(0, 0, tft.width(), tft.height(), ILI9341_BLACK);
  }

  if (updateGraphRange()) {
    // Clear the graph area if the range was updated - redraw whole graph with new range
    tft.fillRect(0, 0, tft.width(), tft.height(), ILI9341_BLACK);
  }

  drawStatusLine(tempsC, SENSOR_COUNT);

  drawGraphAxis(g_graphMinTemp, g_graphMaxTemp);

  if (g_displayedChannel == CHANNEL_ALL) {
    // Draw all channels (reverse order to have CH1 on top)
    for (int i = SENSOR_COUNT-1; i >= 0; --i) {
      drawGraph(g_sensorValues[i], g_sensorValueIndex, g_graphMinTemp, g_graphMaxTemp, g_channelColors[i]);
    }
  } else {
    // Draw selected channel only
    int channelIndex = static_cast<int>(g_displayedChannel);
    drawGraph(g_sensorValues[channelIndex], g_sensorValueIndex, g_graphMinTemp, g_graphMaxTemp, g_channelColors[channelIndex]);
  }
}

void handleWifiMode(const float tempsC[]) {
  // WiFi mode: for now reuse standby display
  drawStatusLine(tempsC, SENSOR_COUNT);
}