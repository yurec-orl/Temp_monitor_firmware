#include "display.h"
#include "hardware.h"
#include "state.h"
#include <Arduino.h>

// Enable profiling output (comment out to disable)
//#define ENABLE_DISPLAY_PROFILING

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
    case MODE_RECORD:  tft.print("  REC");   break;
    case MODE_WIFI:    tft.print(" WIFI");  break;
    default:           tft.print("    ?");     break;
  }

  // Channel to display on graph
  tft.print("  CH: ");
  switch (g_displayedChannel) {
    case CHANNEL_1:    tft.print("  1"); break;
    case CHANNEL_2:    tft.print("  2"); break;
    case CHANNEL_3:    tft.print("  3"); break;
    case CHANNEL_4:    tft.print("  4"); break;
    case CHANNEL_ALL:  tft.print("ALL"); break;
    default:           tft.print("  ?"); break;
  }

  // Sampling rate
  tft.print("  FREQ: ");
  switch (g_samplingFreq) {
    case SAMPLING_FREQ_1S:    tft.print(" 1s");   break;
    case SAMPLING_FREQ_5S:    tft.print(" 5s");   break;
    case SAMPLING_FREQ_10S:   tft.print("10s");   break;
    case SAMPLING_FREQ_60S:   tft.print(" 1m");   break;
    case SAMPLING_FREQ_600S:  tft.print("10m");   break;
    case SAMPLING_FREQ_3600S: tft.print(" 1h");   break;
    default:                  tft.print("  ?");    break;
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
#ifdef ENABLE_DISPLAY_PROFILING
  unsigned long startTime = millis();
#endif
  
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
  
#ifdef ENABLE_DISPLAY_PROFILING
  unsigned long elapsedTime = millis() - startTime;
  Serial.print("[PROFILE] drawGraphAxis: ");
  Serial.print(elapsedTime);
  Serial.println(" ms");
#endif
}

void drawGraph(const float tempsC[], int count, float minTemp, float maxTemp, uint16_t color) {
#ifdef ENABLE_DISPLAY_PROFILING
  unsigned long startTime = millis();
#endif
  
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
  
#ifdef ENABLE_DISPLAY_PROFILING
  unsigned long elapsedTime = millis() - startTime;
  Serial.print("[PROFILE] drawGraph (");
  Serial.print(visibleCount);
  Serial.print(" points): ");
  Serial.print(elapsedTime);
  Serial.println(" ms");
#endif
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
  if (span < 10.0f) span = 10.0f; // avoid zero/very small span

  float pad = span * 0.1f;
  newMin -= pad;
  newMax += pad;

  // Clamp to DS18B20 physical limits
  if (newMin < DS18B20_MIN_TEMP) newMin = DS18B20_MIN_TEMP;
  if (newMax > DS18B20_MAX_TEMP) newMax = DS18B20_MAX_TEMP;

  // Round to nice multiples of 10
  newMin = floorf(newMin / 10.0f) * 10.0f;
  newMax = ceilf (newMax / 10.0f) * 10.0f;

  bool result = false;

  if (newMin != g_graphMinTemp || newMax != g_graphMaxTemp) {
    result = true;
  }

  g_graphMinTemp = newMin;
  g_graphMaxTemp = newMax;

  return result;
}

void clearScreen() {
  tft.fillScreen(ILI9341_BLACK);
}

void clearGraphArea() {
  // Clear the graph area
  tft.fillRect(GRAPH_LEFT_MARGIN, GRAPH_TOP_MARGIN, tft.width() - GRAPH_RIGHT_MARGIN - GRAPH_LEFT_MARGIN + 1, tft.height() - GRAPH_BOTTOM_MARGIN - GRAPH_TOP_MARGIN + 1, ILI9341_BLACK);
}

// Redraw the entire graph for currently selected channel
void redrawGraph() {
#ifdef ENABLE_DISPLAY_PROFILING
  unsigned long startTime = millis();
#endif
  
  clearGraphArea();
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
  
#ifdef ENABLE_DISPLAY_PROFILING
  unsigned long elapsedTime = millis() - startTime;
  Serial.print("[PROFILE] redrawGraph (total): ");
  Serial.print(elapsedTime);
  Serial.println(" ms");
#endif
}

void refreshUI() {
  if (g_sensorValueIndex == 0) {
    // No data yet
    return;
  }

  redrawGraph();
  
  float tempsC[SENSOR_COUNT];
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    tempsC[i] = g_sensorValues[i][g_sensorValueIndex-1];
  }

  drawStatusLine(tempsC, SENSOR_COUNT);
}
