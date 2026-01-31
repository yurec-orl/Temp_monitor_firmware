#include "battery_monitor.h"

BatteryMonitor::BatteryMonitor()
  : m_voltage(0.0f)
  , m_state(BATTERY_GOOD)
  , m_lastUpdate(0)
  , m_isChargingPin(false)
  , m_bufferIndex(0)
  , m_bufferCount(0)
{
  // Initialize buffer with zeros.
  for (int i = 0; i < BATTERY_AVG_SAMPLES; i++) {
    m_voltageBuffer[i] = 0.0f;
  }
}

void BatteryMonitor::begin()
{
  // Configure ADC pin for battery voltage measurement.
  pinMode(PIN_BATTERY_VOLTAGE, INPUT);
  
  // Configure CHRG status pin with internal pull-up.
  // TP4056 CHRG pin is open-drain: LOW = charging, HIGH-Z = not charging.
  pinMode(PIN_CHARGE_STATUS, INPUT_PULLUP);
  
  // ESP32-S3 ADC configuration for better accuracy.
  analogSetAttenuation(ADC_11db);  // 0-3.3V range.
  analogReadResolution(12);         // 12-bit resolution (0-4095).
  
  // Take initial readings to fill buffer.
  for (int i = 0; i < BATTERY_AVG_SAMPLES; i++) {
    readVoltage();
    delay(10);  // Small delay between readings.
  }
  
  // Read initial CHRG pin status.
  m_isChargingPin = (digitalRead(PIN_CHARGE_STATUS) == LOW);
  
  updateState();
  
  Serial.print("Battery monitor initialized. Voltage: ");
  Serial.print(m_voltage);
  Serial.print("V, CHRG pin: ");
  Serial.print(m_isChargingPin ? "LOW (charging)" : "HIGH (not charging)");
  Serial.print(", State: ");
  Serial.println(m_state);
}

void BatteryMonitor::update()
{
  unsigned long now = millis();
  
  // Update at specified interval.
  if (now - m_lastUpdate >= BATTERY_UPDATE_INTERVAL_MS) {
    m_lastUpdate = now;
    
    BatteryState oldState = m_state;
    float oldVoltage = m_voltage;
    bool oldChargingPin = m_isChargingPin;
    
    readVoltage();
    
    // Read CHRG pin status (LOW = charging, HIGH = not charging).
    m_isChargingPin = (digitalRead(PIN_CHARGE_STATUS) == LOW);
    
    updateState();
    
    // Debug output when voltage, CHRG pin, or state changes significantly.
    if (abs(m_voltage - oldVoltage) > 0.05f || m_state != oldState || m_isChargingPin != oldChargingPin) {
      Serial.print("[BATTERY] Voltage: ");
      Serial.print(m_voltage, 3);
      Serial.print("V, CHRG pin: ");
      Serial.print(m_isChargingPin ? "LOW (charging)" : "HIGH");
      Serial.print(", State: ");
      switch (m_state) {
        case BATTERY_CHARGING: Serial.print("CHARGING"); break;
        case BATTERY_FULL: Serial.print("FULL"); break;
        case BATTERY_GOOD: Serial.print("GOOD"); break;
        case BATTERY_LOW: Serial.print("LOW"); break;
        case BATTERY_EMPTY: Serial.print("EMPTY"); break;
      }
      Serial.print(", Percentage: ");
      Serial.print(getPercentage());
      Serial.println("%");
    }
  }
}

void BatteryMonitor::readVoltage()
{
  // Read ADC value.
  int adcValue = analogRead(PIN_BATTERY_VOLTAGE);
  
  // Convert to voltage.
  float voltage = adcToVoltage(adcValue);
  
  // Debug: Print raw ADC and calculated voltage every read.
  // Uncomment for detailed calibration.
  // Serial.print("[BATTERY] ADC: ");
  // Serial.print(adcValue);
  // Serial.print(", ADC_V: ");
  // Serial.print((adcValue / 4095.0f) * 3.3f, 3);
  // Serial.print("V, Calculated: ");
  // Serial.print(voltage, 3);
  // Serial.println("V");
  
  // Add to circular buffer.
  m_voltageBuffer[m_bufferIndex] = voltage;
  m_bufferIndex = (m_bufferIndex + 1) % BATTERY_AVG_SAMPLES;
  
  if (m_bufferCount < BATTERY_AVG_SAMPLES) {
    m_bufferCount++;
  }
  
  // Calculate running average.
  float sum = 0.0f;
  for (int i = 0; i < m_bufferCount; i++) {
    sum += m_voltageBuffer[i];
  }
  m_voltage = sum / m_bufferCount;
}

void BatteryMonitor::updateState()
{
  // Determine battery state based on CHRG pin and voltage.
  // CHRG pin provides definitive charging status.
  
  // CHRG pin LOW = actively charging (TP4056 pulls it LOW).
  if (m_isChargingPin) {
    m_state = BATTERY_CHARGING;
    return;
  }
  
  // CHRG pin HIGH (high-Z) = not charging.
  // Determine state based on voltage.
  
  if (m_voltage >= BATTERY_VOLTAGE_FULL) {
    // Fully charged (CHRG pin went HIGH after charging complete).
    m_state = BATTERY_FULL;
  }
  else if (m_voltage >= BATTERY_VOLTAGE_MID) {
    // Good battery level.
    m_state = BATTERY_GOOD;
  }
  else if (m_voltage >= BATTERY_VOLTAGE_LOW) {
    // Low battery warning.
    m_state = BATTERY_LOW;
  }
  else {
    // Empty/critical.
    m_state = BATTERY_EMPTY;
  }
}

float BatteryMonitor::adcToVoltage(int adcValue)
{
  // ESP32-S3 ADC: 12-bit (0-4095) maps to 0-3.3V with 11db attenuation.
  const float ADC_REFERENCE = 3.3f;
  const float ADC_MAX = 4095.0f;
  
  // Voltage at ADC pin.
  float adcVoltage = (adcValue / ADC_MAX) * ADC_REFERENCE;
  
  // Calculate actual battery voltage from voltage divider.
  // V_battery = V_adc * (R1 + R2) / R2
  float batteryVoltage = adcVoltage * (BATTERY_R1 + BATTERY_R2) / BATTERY_R2;
  
  // Apply calibration factor (measured 4.13V actual vs 3.91V displayed = 1.056).
  const float CALIBRATION_FACTOR = 1.056f;
  batteryVoltage *= CALIBRATION_FACTOR;
  
  return batteryVoltage;
}

int BatteryMonitor::getPercentage() const
{
  // Map voltage to percentage (3.3V = 0%, 4.2V = 100%).
  // Use linear approximation for simplicity.
  const float minVoltage = BATTERY_VOLTAGE_EMPTY;
  const float maxVoltage = BATTERY_VOLTAGE_FULL;
  
  float percentage = ((m_voltage - minVoltage) / (maxVoltage - minVoltage)) * 100.0f;
  
  // Clamp to 0-100 range.
  if (percentage < 0.0f) percentage = 0.0f;
  if (percentage > 100.0f) percentage = 100.0f;
  
  return (int)percentage;
}

bool BatteryMonitor::isFullyCharged() const
{
  // Fully charged = CHRG pin HIGH (not charging) AND voltage >= 4.1V.
  return !m_isChargingPin && (m_voltage >= BATTERY_CHARGING_THRESHOLD);
}
