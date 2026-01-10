#include "buttons.h"
#include "hardware.h"
#include "state.h"
#include "display.h"
#include <Arduino.h>

// --- Button callback implementations -----------------------------------------

// Button 1 callback - Channel selection
void onButton1Pressed() {
  Serial.println("Button 1 pressed - Channel selection");
  // Cycle through CHANNEL_1, CHANNEL_2, CHANNEL_3, CHANNEL_4, CHANNEL_ALL
  g_displayedChannel = nextChannel(g_displayedChannel);
  // Refresh graph and status line
  refreshUI();
}

// Button 2 callback - Mode selection
void onButton2Pressed() {
  Serial.println("Button 2 pressed - Mode selection");
  // Cycle through MODE_STANDBY, MODE_RECORD (WiFi excluded)
  g_mode = nextMode(g_mode);
  // Clear all recorded data and screen
  clearRecordedData();
  clearScreen();
  // Force refresh
  g_lastPresenceRefreshMs = 0;
  g_lastTempRequestMs = 0;
}

// Button 3 callback - Sampling frequency selection
void onButton3Pressed() {
  Serial.println("Button 3 pressed - Sampling frequency selection");
  // Cycle through all sampling frequencies
  g_samplingFreq = nextSamplingFreq(g_samplingFreq);
}

// Button 4 callback - Wifi mode
void onButton4Pressed() {
  Serial.println("Button 4 pressed - Wifi mode");
  // TODO: Implement Wifi access point functionality
}

// --- Button initialization ---------------------------------------------------

void initButtons() {
  button1.begin();
  button2.begin();
  button3.begin();
  button4.begin();
  
  // Attach button callbacks
  button1.onPressed(onButton1Pressed);
  button2.onPressed(onButton2Pressed);
  button3.onPressed(onButton3Pressed);
  button4.onPressed(onButton4Pressed);
  
  Serial.println("Buttons initialized");
}

void readButtons() {
  button1.read();
  button2.read();
  button3.read();
  button4.read();
}
