#ifndef SENSOR_READER_H
#define SENSOR_READER_H

#include "config.h"
#include <Arduino.h>

// State machine states for non-blocking temperature reading
enum ReadingState {
  STATE_IDLE,              // Not currently reading, ready to start
  STATE_CONVERSION,        // Conversion requested, waiting for TEMP_REQUEST_DELAY
  STATE_READY              // Conversion complete, data ready to be consumed
};

/**
 * Manages non-blocking temperature reading from DS18B20 sensors.
 * Handles the state machine for requesting conversions and reading results.
 */
class SensorReader {
private:
  // Current state of the reading state machine
  ReadingState state;
  
  // Timestamp when temperature conversion was requested
  unsigned long conversionStartTime;
  
  // Buffer to store the last successful temperature readings
  float lastReadings[SENSOR_COUNT];
  
  // Timestamp of the last completed reading cycle
  unsigned long lastCompletedReadingTime;
  
  // Timestamp of the last presence check (hot-plug detection)
  unsigned long lastPresenceCheckTime;
  
  // Round-robin index for checking one sensor at a time
  int nextPresenceCheckIndex;

public:
  /**
   * Constructor - initializes the reader in idle state
   */
  SensorReader();
  
  /**
   * Call this frequently in the main loop.
   * Advances the state machine and manages conversion timing.
   * 
   * @param samplingInterval - The desired interval between readings in ms
   * @return true if new temperature data is available to be consumed
   */
  bool update(unsigned long samplingInterval);
  
  /**
   * Check if temperature conversion is currently in progress.
   * Used to prevent OneWire bus interference from other operations.
   * 
   * @return true if conversion is in progress
   */
  bool isConversionInProgress() const;
  
  /**
   * Get the latest temperature readings.
   * Should be called after update() returns true.
   * 
   * @param tempsC - Array to store temperature values
   * @param count - Number of sensors to read
   * @return true if data was successfully retrieved
   */
  bool getReadings(float tempsC[], int count);
  
  /**
   * Force an immediate temperature reading on the next update() call.
   * Useful when sampling frequency changes or on startup.
   */
  void requestImmediateReading();
  
  /**
   * Reset the state machine to idle state.
   * Call this if you need to abort an ongoing conversion.
   */
  void reset();
  
  /**
   * Get the timestamp of the last completed reading.
   * 
   * @return millis() value when last reading completed
   */
  unsigned long getLastReadingTime() const;
  
  /**
   * Update hot-plug detection (device presence checking).
   * Call this periodically (every ~2 seconds) but NOT during conversion.
   * Automatically skipped if conversion is in progress.
   * 
   * @param presenceInterval - Minimum time between presence checks in ms
   */
  void updatePresenceDetection(unsigned long presenceInterval);
  
  /**
   * Force an immediate presence check.
   * Useful for initial scan during setup.
   */
  void forcePresenceCheck();

private:
  /**
   * Internal: Start temperature conversion on all active sensors
   */
  void startConversion();
  
  /**
   * Internal: Read temperature values from all sensors after conversion
   */
  void readSensors();
  
  /**
   * Internal: Validate a temperature reading for suspicious values
   * 
   * @param temp - Temperature value to validate
   * @param channel - Channel index for debug output
   * @return true if reading appears valid
   */
  bool validateReading(float temp, int channel);
  
  /**
   * Internal: Check for sensor presence on all channels (hot-plug support)
   */
  void checkDevicePresence();
};

#endif // SENSOR_READER_H
