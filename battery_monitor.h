#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>
#include "config.h"

class BatteryMonitor {
public:
  BatteryMonitor();
  
  // Initialize battery monitoring.
  void begin();
  
  // Update battery voltage reading (call periodically).
  void update();
  
  // Get current battery state.
  BatteryState getState() const { return m_state; }
  
  // Get battery voltage (actual battery voltage, not ADC reading).
  float getVoltage() const { return m_voltage; }
  
  // Get battery percentage (0-100%).
  int getPercentage() const;
  
  // Check if battery is charging.
  bool isCharging() const { return m_state == BATTERY_CHARGING; }

private:
  // Read ADC and update running average.
  void readVoltage();
  
  // Calculate battery state from voltage.
  void updateState();
  
  // Convert ADC reading to actual battery voltage.
  float adcToVoltage(int adcValue);
  
  float m_voltage;              // Current battery voltage.
  BatteryState m_state;         // Current battery state.
  unsigned long m_lastUpdate;   // Last update timestamp.
  
  // Running average buffer.
  float m_voltageBuffer[BATTERY_AVG_SAMPLES];
  int m_bufferIndex;
  int m_bufferCount;
};

#endif // BATTERY_MONITOR_H
