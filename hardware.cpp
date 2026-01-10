#include "hardware.h"
#include <SPI.h>

// --- Display object ----------------------------------------------------------
Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

// --- Sensor objects ----------------------------------------------------------
OneWire oneWire1(PIN_DS18B20_1);
OneWire oneWire2(PIN_DS18B20_2);
OneWire oneWire3(PIN_DS18B20_3);
OneWire oneWire4(PIN_DS18B20_4);

DallasTemperature sensors1(&oneWire1);
DallasTemperature sensors2(&oneWire2);
DallasTemperature sensors3(&oneWire3);
DallasTemperature sensors4(&oneWire4);

// Array of pointers to DallasTemperature objects for easier iteration
DallasTemperature* g_sensors[SENSOR_COUNT] = {
  &sensors1,
  &sensors2,
  &sensors3,
  &sensors4
};

// --- Button objects ----------------------------------------------------------
EasyButton button1(PIN_BUTTON_1);
EasyButton button2(PIN_BUTTON_2);
EasyButton button3(PIN_BUTTON_3);
EasyButton button4(PIN_BUTTON_4);

// --- Hardware initialization -------------------------------------------------
void initHardware() {
  // Configure backlight pin
  pinMode(PIN_TFT_LED, OUTPUT);
  digitalWrite(PIN_TFT_LED, HIGH);  // Turn backlight on (assuming active-high)

  // Reconfigure SPI pins for the display (Arduino-ESP32)
  SPI.begin(PIN_TFT_SCK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);

  // Initialize the display
  tft.begin();
  tft.setRotation(3);  // Landscape
  tft.fillScreen(ILI9341_BLACK);

  // Initialize DS18B20 sensors
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    g_sensors[i]->begin();
    g_sensors[i]->setResolution(12); // Optional: 9–12 bits; 12 is default and slowest
  }

  Serial.println("Hardware initialized");
}
