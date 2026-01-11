#include "sensor_reader.h"
#include "hardware.h"
#include "state.h"

// Constructor
SensorReader::SensorReader() 
  : state(STATE_IDLE),
    conversionStartTime(0),
    lastCompletedReadingTime(0),
    lastPresenceCheckTime(0),
    nextPresenceCheckIndex(0)
{
  // Initialize readings buffer with invalid values
  for (int i = 0; i < SENSOR_COUNT; i++) {
    lastReadings[i] = DEVICE_DISCONNECTED_C;
  }
}

// Main update function - call this in loop()
bool SensorReader::update(unsigned long samplingInterval) {
  unsigned long now = millis();
  
  switch (state) {
    case STATE_IDLE:
      // Check if it's time to start a new reading cycle
      // Start conversion TEMP_REQUEST_DELAY ms before the actual interval expires
      if (now - lastCompletedReadingTime >= samplingInterval - TEMP_REQUEST_DELAY) {
        startConversion();
        state = STATE_CONVERSION;
      }
      return false;
      
    case STATE_CONVERSION:
      // Check if enough time has elapsed for conversion to complete
      if (now - conversionStartTime >= TEMP_REQUEST_DELAY) {
        readSensors();
        state = STATE_READY;
        lastCompletedReadingTime = now;
        return true;  // New data available
      }
      return false;
      
    case STATE_READY:
      // Data has been made available but not yet consumed
      // Stay in this state until getReadings() is called
      // This prevents starting a new conversion before data is consumed
      return true;
      
    default:
      // Should never happen, reset to safe state
      reset();
      return false;
  }
}

// Check if conversion is in progress
bool SensorReader::isConversionInProgress() const {
  return (state == STATE_CONVERSION);
}

// Get the latest readings
bool SensorReader::getReadings(float tempsC[], int count) {
  if (state != STATE_READY) {
    return false;  // No data available
  }
  
  // Copy readings to output array
  int copyCount = (count < SENSOR_COUNT) ? count : SENSOR_COUNT;
  for (int i = 0; i < copyCount; i++) {
    tempsC[i] = lastReadings[i];
  }
  
  // Transition back to IDLE after data is consumed
  state = STATE_IDLE;
  
  return true;
}

// Force immediate reading
void SensorReader::requestImmediateReading() {
  // Reset the timer to force update() to trigger on next call
  lastCompletedReadingTime = millis() - getSamplingIntervalMs();
  state = STATE_IDLE;
}

// Reset state machine
void SensorReader::reset() {
  state = STATE_IDLE;
  conversionStartTime = 0;
}

// Get last reading timestamp
unsigned long SensorReader::getLastReadingTime() const {
  return lastCompletedReadingTime;
}

// Update presence detection (hot-plug support)
void SensorReader::updatePresenceDetection(unsigned long presenceInterval) {
  // Never check presence during conversion - it interferes with OneWire bus
  if (state == STATE_CONVERSION) {
    return;
  }
  
  unsigned long now = millis();
  if (now - lastPresenceCheckTime >= presenceInterval) {
    lastPresenceCheckTime = now;
    checkDevicePresence();
  }
}

// Force immediate presence check
void SensorReader::forcePresenceCheck() {
  // Run presence check on all channels at once
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    checkDevicePresence();
  }
  lastPresenceCheckTime = millis();
}

// --- Private methods ---

// Start temperature conversion on all active sensors
void SensorReader::startConversion() {
  conversionStartTime = millis();
  
  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (g_channelHasDevice[i]) {
      g_sensors[i]->requestTemperatures();
    }
  }
}

// Read temperature values from all sensors
void SensorReader::readSensors() {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (g_channelHasDevice[i]) {
      float temp = g_sensors[i]->getTempCByIndex(0);
      
      if (validateReading(temp, i)) {
        lastReadings[i] = temp;
      } else {
        // Keep previous value on suspicious reading
        Serial.print("Warning: CH");
        Serial.print(i + 1);
        Serial.print(" suspicious reading: ");
        Serial.print(temp);
        Serial.println(" - keeping previous value");
      }
    } else {
      lastReadings[i] = DEVICE_DISCONNECTED_C;
    }
  }
}

// Validate temperature reading
bool SensorReader::validateReading(float temp, int channel) {
  // Check if value is within valid range
  if (temp > DS18B20_MAX_TEMP || temp < DS18B20_MIN_TEMP) {  // DS18B20 max range is -55 to +125°C
    return false;
  }
  
  return true;
}

// Check device presence on all channels
void SensorReader::checkDevicePresence() {
  // Check only ONE sensor per call to minimize blocking time
  // This spreads the load across multiple presence check intervals
  int i = nextPresenceCheckIndex;
  
  // Call begin() to force fresh OneWire bus scan for this channel
  // This is necessary because DallasTemperature library caches device addresses
  g_sensors[i]->begin();

  // getDeviceCount() performs a OneWire search
  int count = g_sensors[i]->getDeviceCount();
  bool present = (count > 0);

  if (present != g_channelHasDevice[i]) {
    // Presence changed; log it once
    Serial.print("Channel ");
    Serial.print(i + 1);
    Serial.print(present ? " attached" : " detached");
    Serial.println();
  }
  
  g_channelHasDevice[i] = present;
  
  // Move to next sensor for next check (round-robin)
  nextPresenceCheckIndex = (nextPresenceCheckIndex + 1) % SENSOR_COUNT;
}
