#ifndef MODES_H
#define MODES_H

// Mode handler functions
void handleStandbyMode(const float tempsC[]);
void handleRecordMode(const float tempsC[]);
// void handleWifiMode(const float tempsC[]);  // Disabled - replaced with USB Serial
void handleUsbSerialMode(const float tempsC[]);

#endif // MODES_H
