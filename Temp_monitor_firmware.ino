#include <Arduino.h>
#include "FS.h"
#include <LittleFS.h>

// Project modules
#include "config.h"
#include "hardware.h"
#include "state.h"
#include "sensors.h"
#include "display.h"
#include "buttons.h"
#include "modes.h"

// --- Setup and Main Loop -----------------------------------------------------

void setup() {
  // Basic serial for debug
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Starting ILI9341 + DS18B20 4-channel monitor...");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }

  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();

  Serial.print("LittleFS total bytes: ");
  Serial.println(total);
  Serial.print("LittleFS used bytes: ");
  Serial.println(used);

  // Initialize hardware
  initHardware();

  // Initialize buttons
  initButtons();

  // Initial presence scan
  refreshDevicePresence();

  Serial.println("Initialization complete.");
}

void loop() {
  unsigned long now = millis();

  // Read button states (must be called frequently for debouncing)
  readButtons();

  // Periodically refresh presence to support hot-plug
  if (now - g_lastPresenceRefreshMs >= PRESENCE_REFRESH_INTERVAL_MS) {
    g_lastPresenceRefreshMs = now;
    refreshDevicePresence();
  }

  float tempsC[SENSOR_COUNT];
  if (readTemperatures(tempsC, SENSOR_COUNT)) {
    if (now - g_lastTempRequestMs >= TEMP_UPDATE_INTERVAL_MS) {
      g_lastTempRequestMs = now;
      
      // Mode-specific handling
      switch (g_mode) {
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
      Serial.print("Temps: ");
      for (int i = 0; i < SENSOR_COUNT; ++i) {
        Serial.print("CH");
        Serial.print(i + 1);
        Serial.print("=");
        if (!g_channelHasDevice[i] || tempsC[i] == DEVICE_DISCONNECTED_C) {
          Serial.print("N/A ");
        } else {
          Serial.print(tempsC[i]);
          Serial.print("C ");
        }
      }
      Serial.println();
    }
  }
}
