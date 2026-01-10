#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

// Draw 2-line status (mode/channel + sensor values)
void drawStatusLine(const float tempsC[], int count);

// Draw temperature/time graph axis
void drawGraphAxis(float minTemp, float maxTemp);

// Draw temperature/time graph data
void drawGraph(const float tempsC[], int count, float minTemp, float maxTemp, uint16_t color);

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

#endif // DISPLAY_H
