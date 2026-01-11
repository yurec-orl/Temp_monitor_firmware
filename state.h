#ifndef STATE_H
#define STATE_H

#include "config.h"
#include "ring_buffer.h"

// --- Global state variables --------------------------------------------------

// Current mode
extern OperatingMode g_mode;

// Current sampling frequency
extern SamplingFrequency g_samplingFreq;

// Current channel displayed on graph
extern DisplayChannel g_displayedChannel;

// Per-channel presence flags (updated via refreshDevicePresence())
extern bool g_channelHasDevice[SENSOR_COUNT];

// Sensor values buffer for graph plotting
extern RingBuffer g_sensorValues[SENSOR_COUNT];

// Dynamic scaling state
extern float g_dataMinTemp;
extern float g_dataMaxTemp;

// Flag indicating if full min/max recalculation is needed
extern bool g_needMinMaxRecalc;

// Current graph range actually used for drawing
extern float g_graphMinTemp;
extern float g_graphMaxTemp;

// --- Helper functions --------------------------------------------------------

// Cycle to next display channel
DisplayChannel nextChannel(DisplayChannel current);

// Cycle to next operating mode (excluding WiFi mode)
OperatingMode nextMode(OperatingMode current);

// Cycle to next sampling frequency
SamplingFrequency nextSamplingFreq(SamplingFrequency current);

// Get the current sampling interval in milliseconds
unsigned long getSamplingIntervalMs();

// Clear all recorded temperature data
void clearRecordedData();

// Recalculate min/max from entire dataset
void recalculateMinMax();

// Pre-fill buffers with test data for debugging (125°C down to 25°C)
void prefillTestData();

#endif // STATE_H
