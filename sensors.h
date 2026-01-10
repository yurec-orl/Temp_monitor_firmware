#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"

// Refresh device presence information
void refreshDevicePresence();

// Read temperatures from all channels into provided array
// Returns true if result has been returned
bool readTemperatures(float tempsC[], int count);

#endif // SENSORS_H
