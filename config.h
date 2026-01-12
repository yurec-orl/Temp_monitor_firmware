#ifndef CONFIG_H
#define CONFIG_H

#include <Adafruit_ILI9341.h>

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

// --- Constants ---------------------------------------------------------------

// Number of DS18B20 sensor channels.
constexpr int SENSOR_COUNT = 4;

// Size of graph buffer (number of samples to display).
// Buffer has +1 extra element to store previous state for flicker-free erasing.
constexpr int GRAPH_DISPLAY_SIZE = 260;  // Number of pixels on graph.
constexpr int GRAPH_BUFFER_SIZE = GRAPH_DISPLAY_SIZE + 1;  // Actual buffer capacity - keep one oldest sample for erasing old graph.

// Graph layout margins.
constexpr int16_t GRAPH_TOP_MARGIN    = 40;
constexpr int16_t GRAPH_LEFT_MARGIN   = 30;
constexpr int16_t GRAPH_RIGHT_MARGIN  = 30;
constexpr int16_t GRAPH_BOTTOM_MARGIN = 20;

// Channel graph colors.
constexpr uint16_t g_channelColors[SENSOR_COUNT] = {
  ILI9341_YELLOW,
  ILI9341_BLUE,
  ILI9341_ORANGE,
  ILI9341_MAGENTA
};

// Sensor physical measurement limits.
constexpr float DS18B20_MIN_TEMP = -55.0f;
constexpr float DS18B20_MAX_TEMP = 125.0f;

// Timing intervals.
constexpr unsigned long SAMPLING_INTERVAL_OVERRIDE_MS = 1000;
constexpr unsigned long PRESENCE_REFRESH_INTERVAL_MS = 2000;

constexpr unsigned long TEMP_REQUEST_DELAY = 750;   // Required by DS18B20 to process requests.

// --- Hardware pins -----------------------------------------------------------

// TFT display pins.
static const int PIN_TFT_MISO = 11;  // SDO (not always needed).
static const int PIN_TFT_LED  = 12;  // Backlight.
static const int PIN_TFT_SCK  = 13;  // SCK.
static const int PIN_TFT_MOSI = 14;  // MOSI (SDI).
static const int PIN_TFT_DC   = 15;  // D/C.
static const int PIN_TFT_RST  = 16;  // RESET.
static const int PIN_TFT_CS   = 17;  // CS.

// Sensor channel pins (4-7).
static const int PIN_DS18B20_1 = 4;
static const int PIN_DS18B20_2 = 5;
static const int PIN_DS18B20_3 = 6;
static const int PIN_DS18B20_4 = 7;

// Button pins.
static const int PIN_BUTTON_1 = 1;
static const int PIN_BUTTON_2 = 2;
static const int PIN_BUTTON_3 = 8;
static const int PIN_BUTTON_4 = 9;

#endif // CONFIG_H
