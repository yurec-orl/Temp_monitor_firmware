#include "modes.h"
#include "config.h"
#include "hardware.h"
#include "state.h"
#include "display.h"

// --- Mode handlers -----------------------------------------------------------

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
