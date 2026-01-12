#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

class RingBuffer;

// Draw 2-line status (mode/channel + sensor values)
void drawStatusLine(const float tempsC[], int count);

// Draw system status information in standby mode
void drawStandbyStatus();

// Draw temperature/time graph axis
void drawGraphAxis(float minTemp, float maxTemp);

// Optimized graph update: erase and draw segment-by-segment to minimize flicker
void drawGraph(const RingBuffer& buffer, float minTemp, float maxTemp, uint16_t color);

// Update the graph range based on current data
// Returns true if the graph range was updated
bool updateGraphRange();

// Clear the entire screen
void clearScreen();

// Clear the graph area only
void clearGraphArea();

// Redraw the entire graph for currently selected channel
void redrawGraph();

// Refresh the entire UI
void refreshUI();

// Helper function to get sampling interval in seconds
int getSamplingIntervalSeconds();

// Helper function to format time duration
void formatTimeDuration(int seconds, char* buffer, size_t bufSize);

// Draw battery indicator icon.
void drawBatteryIcon(int16_t x, int16_t y, int16_t width, int16_t height);

#endif // DISPLAY_H
