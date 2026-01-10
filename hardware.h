#ifndef HARDWARE_H
#define HARDWARE_H

#include <Adafruit_ILI9341.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EasyButton.h>
#include "config.h"

// --- Display object ----------------------------------------------------------
extern Adafruit_ILI9341 tft;

// --- Sensor objects ----------------------------------------------------------
extern OneWire oneWire1;
extern OneWire oneWire2;
extern OneWire oneWire3;
extern OneWire oneWire4;

extern DallasTemperature sensors1;
extern DallasTemperature sensors2;
extern DallasTemperature sensors3;
extern DallasTemperature sensors4;

// Array of pointers to DallasTemperature objects for easier iteration
extern DallasTemperature* g_sensors[SENSOR_COUNT];

// --- Button objects ----------------------------------------------------------
extern EasyButton button1;
extern EasyButton button2;
extern EasyButton button3;
extern EasyButton button4;

// --- Hardware initialization -------------------------------------------------
void initHardware();

#endif // HARDWARE_H
