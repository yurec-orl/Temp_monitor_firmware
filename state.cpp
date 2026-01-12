#include "state.h"
#include "ring_buffer.h"

// --- Global state variables --------------------------------------------------

OperatingMode g_mode = MODE_STANDBY;
SamplingFrequency g_samplingFreq = SAMPLING_FREQ_1S;
DisplayChannel g_displayedChannel = CHANNEL_ALL;

bool g_channelHasDevice[SENSOR_COUNT] = { false, false, false, false };

RingBuffer g_sensorValues[SENSOR_COUNT];

// Logs temperature readings to flash memory
TemperatureLogger g_logger;

// Manages WiFi AP and web server
WiFiManager g_wifiManager;

float g_dataMinTemp = 9999.0f;
float g_dataMaxTemp = -9999.0f;

bool g_needMinMaxRecalc = false;

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

unsigned long getSamplingIntervalMs() {
  switch (g_samplingFreq) {
    case SAMPLING_FREQ_1S:    return 1000UL;
    case SAMPLING_FREQ_5S:    return 5000UL;
    case SAMPLING_FREQ_10S:   return 10000UL;
    case SAMPLING_FREQ_60S:   return 60000UL;
    case SAMPLING_FREQ_600S:  return 600000UL;
    case SAMPLING_FREQ_3600S: return 3600000UL;
    default:                  return 1000UL;
  }
}

// Clear all recorded temperature data
void clearRecordedData() {
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    g_sensorValues[i].clear();
  }
  g_dataMinTemp = DS18B20_MAX_TEMP;
  g_dataMaxTemp = DS18B20_MIN_TEMP;
  g_graphMinTemp = DS18B20_MAX_TEMP;
  g_graphMaxTemp = DS18B20_MIN_TEMP;
  g_needMinMaxRecalc = false;
}

// Recalculate min/max from entire dataset
void recalculateMinMax() {
  g_dataMinTemp = DS18B20_MAX_TEMP;
  g_dataMaxTemp = DS18B20_MIN_TEMP;
  
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    for (int j = 0; j < g_sensorValues[i].size(); ++j) {
      float v = g_sensorValues[i].get(j);
      if (v != DEVICE_DISCONNECTED_C) {
        if (v < g_dataMinTemp) g_dataMinTemp = v;
        if (v > g_dataMaxTemp) g_dataMaxTemp = v;
      }
    }
  }
  
  g_needMinMaxRecalc = false;
}

// Pre-fill buffers with test data for debugging
// 200 samples: 125°C down to 25°C at -0.5°C per sample
// Remaining samples: 25°C flat

#ifdef ENABLE_TEST_DATA_PREFILL
void prefillTestData() {
  Serial.println("Pre-filling buffers with test data...");
  
  // Clear existing data
  clearRecordedData();
  
  // Fill buffer from oldest to newest (buffer will reverse the order)
  for (int sample = GRAPH_BUFFER_SIZE - 1; sample >= 0; --sample) {
    float temp;
    
    if (sample >= 61) {
      // Samples 61-260: Linear decrease from 125°C to 25°C
      // 200 samples total: 125 - (260-sample) * 0.5
      int sampleIndex = 260 - sample; // 0 to 199
      temp = 125.0f - (sampleIndex * 0.5f);
    } else {
      // Samples 0-60: Flat at 25°C
      temp = 25.0f;
    }
    
    // Add to all channels
    for (int i = 0; i < SENSOR_COUNT; ++i) {
      g_sensorValues[i].add(temp);
    }
  }
  
  // Recalculate min/max based on test data
  recalculateMinMax();
  
  Serial.print("Test data filled. Min: ");
  Serial.print(g_dataMinTemp);
  Serial.print("°C, Max: ");
  Serial.print(g_dataMaxTemp);
  Serial.println("°C");
  Serial.print("Buffer size: ");
  Serial.println(g_sensorValues[0].size());
}
#endif
