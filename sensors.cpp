#include "sensors.h"
#include "hardware.h"
#include "state.h"
#include <Arduino.h>

// Refresh presence info for each bus using getDeviceCount()
void refreshDevicePresence() {
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    if (!g_channelHasDevice[i]) {
      g_sensors[i]->begin();
    }

    int count = g_sensors[i]->getDeviceCount();
    bool present = (count > 0);

    if (present != (g_channelHasDevice[i]?true:false)) {
      // Presence changed; log it once
      Serial.print("Channel ");
      Serial.print(i + 1);
      Serial.print(present ? " attached" : " detached");
      Serial.println();
    }

    if (!present) {
      g_channelHasDevice[i] = 0; // No device
    } else if (!g_channelHasDevice[i]) {
      g_channelHasDevice[i] = 1;
    }
  }
}

// Read temperatures for all channels into tempsC array
// Has 750 ms delay before results are available (non-blocking)
// Return true if result has been returned
bool readTemperatures(float tempsC[], int count) {
  // This function will make a request temp call for each channel.
  // After that, it will wait (non-blocking) for 750 ms and then read the results.
  static bool requestInProgress = false;
  // Last request timestamp
  static unsigned long lastRequestTime = 0;

  // 1) Request temperature conversion on all buses that currently have a device
  if (!requestInProgress) {
    for (int i = 0; i < count && i < SENSOR_COUNT; ++i) {
      if (g_channelHasDevice[i]) {
        g_sensors[i]->requestTemperatures();
      }
    }
    requestInProgress = true;
    lastRequestTime = millis();
  } else if (millis() - lastRequestTime > 750) {
    // 3) Read results from each bus (or mark as disconnected if no device)
    for (int i = 0; i < count && i < SENSOR_COUNT; ++i) {
      if (g_channelHasDevice[i] == 2) {
        tempsC[i] = g_sensors[i]->getTempCByIndex(0);
      } else if (g_channelHasDevice[i] == 1) {
        tempsC[i] = DEVICE_DISCONNECTED_C;      // Sensor is still initializing
        g_channelHasDevice[i] = 2;
      } else {
        tempsC[i] = DEVICE_DISCONNECTED_C;
      }
    }
    requestInProgress = false;
    return true;
  }

  return false;
}
