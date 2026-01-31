#include "buttons.h"
#include "hardware.h"
#include "state.h"
#include "display.h"
#include "sensor_reader.h"
#include <Arduino.h>

// External reference to the global sensor reader.
extern SensorReader g_sensorReader;

// --- Button callback implementations -----------------------------------------

// Button 1 callback - Channel selection.
void onButton1Pressed()
{
    Serial.println("Button 1 pressed - Channel selection");
    // Cycle through CHANNEL_1, CHANNEL_2, CHANNEL_3, CHANNEL_4, CHANNEL_ALL.
    g_displayedChannel = nextChannel(g_displayedChannel);
    // Refresh graph and status line.
    refreshUI();
}

// Button 2 callback - Mode selection.
void onButton2Pressed()
{
    Serial.println("Button 2 pressed - Mode selection");
    
    // Stop WiFi if currently in WiFi mode (disabled).
    // if (g_mode == MODE_WIFI && g_wifiManager.isActive()) {
    //     g_wifiManager.stop();
    // }
    
    // Stop USB Serial if currently active.
    if (g_mode == MODE_USB_SERIAL && g_usbSerialManager.isActive()) {
        g_usbSerialManager.stop();
    }
    
    // Stop logging if currently recording.
    if (g_mode == MODE_RECORD && g_logger.isRecording()) {
        g_logger.stopRecording();
    }
    
    // Cycle through MODE_STANDBY, MODE_RECORD (WiFi/USB Serial excluded).
    g_mode = nextMode(g_mode);
    
    // Clear all recorded data and screen.
    clearRecordedData();
    clearScreen();
    
    // Force immediate sensor reading.
    g_sensorReader.requestImmediateReading();
}

// Button 3 callback - Sampling frequency selection.
void onButton3Pressed()
{
    Serial.println("Button 3 pressed - Sampling frequency selection");
    if (g_mode == MODE_RECORD) {
        // Frequency is fixed in recording mode.
        return;
    }

    // Cycle through all sampling frequencies.
    g_samplingFreq = nextSamplingFreq(g_samplingFreq);
    g_sensorReader.requestImmediateReading();   // Force immediate temperature request.

    refreshUI();
}

// Button 4 callback - USB Serial mode (replaces WiFi mode).
void onButton4Pressed()
{
    Serial.println("Button 4 pressed - USB Serial mode");
    
    // If currently in USB Serial mode, exit it.
    if (g_mode == MODE_USB_SERIAL) {
        Serial.println("Exiting USB Serial mode");
        g_usbSerialManager.stop();
        g_mode = MODE_STANDBY;
        clearScreen();
        return;
    }
    
    // Stop logging if currently recording.
    if (g_mode == MODE_RECORD && g_logger.isRecording()) {
        g_logger.stopRecording();
    }
    
    // Enter USB Serial mode.
    g_mode = MODE_USB_SERIAL;
    clearScreen();
    
    // Force an immediate sensor reading so USB Serial display shows current temps.
    // Without this, with slow sampling (e.g., 1 hour), screen would be blank.
    g_sensorReader.requestImmediateReading();
    
    Serial.println("Entering USB Serial mode");
}

// WiFi mode callback (disabled - replaced with USB Serial)
/*
void onButton4Pressed()
{
    Serial.println("Button 4 pressed - WiFi mode");
    
    // If currently in WiFi mode, exit it.
    if (g_mode == MODE_WIFI) {
        Serial.println("Exiting WiFi mode");
        g_wifiManager.stop();
        g_mode = MODE_STANDBY;
        clearScreen();
        return;
    }
    
    // Stop logging if currently recording.
    if (g_mode == MODE_RECORD && g_logger.isRecording()) {
        g_logger.stopRecording();
    }
    
    // Enter WiFi mode.
    g_mode = MODE_WIFI;
    clearScreen();
    
    // Force an immediate sensor reading so WiFi display shows current temps.
    // Without this, with slow sampling (e.g., 1 hour), screen would be blank.
    g_sensorReader.requestImmediateReading();
    
    Serial.println("Entering WiFi mode");
}
*/

// --- Button initialization ---------------------------------------------------

void initButtons()
{
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

void readButtons()
{
    button1.read();
    button2.read();
    button3.read();
    button4.read();
}
