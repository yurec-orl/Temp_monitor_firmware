#include <Arduino.h>
#include "FS.h"
#include <LittleFS.h>

// Project modules
#include "config.h"
#include "hardware.h"
#include "state.h"
#include "sensor_reader.h"
#include "display.h"
#include "buttons.h"
#include "modes.h"

// --- Global instances --------------------------------------------------------

// Sensor reading state machine
SensorReader g_sensorReader;

// --- Setup and Main Loop -----------------------------------------------------

void setup()
{
  // Basic serial for debug
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Starting ILI9341 + DS18B20 4-channel monitor...");

  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS mount failed");
    return;
  }

  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();

  Serial.print("LittleFS total bytes: ");
  Serial.println(total);
  Serial.print("LittleFS used bytes: ");
  Serial.println(used);

  // Initialize hardware
  initHardware();

  // Initialize buttons
  initButtons();

  // Initial presence scan
  g_sensorReader.forcePresenceCheck();

  // Pre-fill test data for debugging range shrinking behavior
  //prefillTestData();

  Serial.println("Initialization complete.");
}

void loop()
{
  unsigned long now = millis();

  // Read button states (must be called frequently for debouncing)
  readButtons();

  // Update hot-plug detection (sensor presence checking)
  // Automatically skipped during temperature conversion to avoid bus interference
  g_sensorReader.updatePresenceDetection(PRESENCE_REFRESH_INTERVAL_MS);

  // Update sensor reading state machine
  // This must be called frequently to advance the state machine
  if (g_sensorReader.update(getSamplingIntervalMs())) {
    // New temperature data is ready
    float tempsC[SENSOR_COUNT];
    
    if (g_sensorReader.getReadings(tempsC, SENSOR_COUNT)) {
      // Mode-specific handling
      switch (g_mode)
      {
      case MODE_STANDBY:
        handleStandbyMode(tempsC);
        break;
      case MODE_RECORD:
        handleRecordMode(tempsC);
        break;
      case MODE_WIFI:
        handleWifiMode(tempsC);
        break;
      default:
        break;
      }

      // Serial debug
      Serial.print("[");
      Serial.print(millis());
      Serial.print("] ");
      Serial.print("Temps: ");
      for (int i = 0; i < SENSOR_COUNT; ++i) {
        Serial.print("CH");
        Serial.print(i + 1);
        Serial.print("=");
        if (!g_channelHasDevice[i] || tempsC[i] == DEVICE_DISCONNECTED_C)
        {
          Serial.print("N/A ");
        }
        else
        {
          Serial.print(tempsC[i]);
          Serial.print("C ");
        }
      }
      Serial.println();
    }
  }
}
