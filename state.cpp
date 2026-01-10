#include "state.h"

// --- Global state variables --------------------------------------------------

OperatingMode g_mode = MODE_RECORD;
SamplingFrequency g_samplingFreq = SAMPLING_FREQ_1S;
DisplayChannel g_displayedChannel = CHANNEL_ALL;

uint8_t g_channelHasDevice[SENSOR_COUNT] = { 0, 0, 0, 0 };

unsigned long g_lastTempRequestMs = 0;
unsigned long g_lastPresenceRefreshMs = 0;

float g_sensorValues[SENSOR_COUNT][GRAPH_BUFFER_SIZE];
int g_sensorValueIndex = 0;

float g_dataMinTemp = 9999.0f;
float g_dataMaxTemp = -9999.0f;

float g_graphMinTemp = DS18B20_MAX_TEMP;
float g_graphMaxTemp = DS18B20_MIN_TEMP;

// --- Helper functions --------------------------------------------------------

// Cycle to next display channel
DisplayChannel nextChannel(DisplayChannel current) {
  switch(current) {
    case CHANNEL_1:   return CHANNEL_2;
    case CHANNEL_2:   return CHANNEL_3;
    case CHANNEL_3:   return CHANNEL_4;
    case CHANNEL_4:   return CHANNEL_ALL;
    case CHANNEL_ALL: return CHANNEL_1;
    default:          return CHANNEL_1;
  }
}

// Cycle to next operating mode (excluding WiFi mode)
OperatingMode nextMode(OperatingMode current) {
  // Switches between standby and recording modes. Wifi mode has dedicated button.
  switch(current) {
    case MODE_STANDBY: return MODE_RECORD;
    case MODE_RECORD:  return MODE_STANDBY;
    case MODE_WIFI:    return MODE_STANDBY;
    default:           return MODE_STANDBY;
  }
}

// Cycle to next sampling frequency
SamplingFrequency nextSamplingFreq(SamplingFrequency current) {
  if (g_mode == MODE_RECORD) {
    // In Recording mode, sampling frequency is fixed (no cycling)
    return current;
  }

  switch(current) {
    case SAMPLING_FREQ_1S:    return SAMPLING_FREQ_5S;
    case SAMPLING_FREQ_5S:    return SAMPLING_FREQ_10S;
    case SAMPLING_FREQ_10S:   return SAMPLING_FREQ_60S;
    case SAMPLING_FREQ_60S:   return SAMPLING_FREQ_600S;
    case SAMPLING_FREQ_600S:  return SAMPLING_FREQ_3600S;
    case SAMPLING_FREQ_3600S: return SAMPLING_FREQ_1S;
    default:                  return SAMPLING_FREQ_1S;
  }
}

// Clear all recorded temperature data
void clearRecordedData() {
  memset(g_sensorValues, 0, sizeof(g_sensorValues));
  g_sensorValueIndex = 0;
  g_dataMinTemp = DS18B20_MAX_TEMP;
  g_dataMaxTemp = DS18B20_MIN_TEMP;
  g_graphMinTemp = DS18B20_MAX_TEMP;
  g_graphMaxTemp = DS18B20_MIN_TEMP;
}
