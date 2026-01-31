#ifndef USB_SERIAL_MANAGER_H
#define USB_SERIAL_MANAGER_H

#include <Arduino.h>
#include "logger.h"

/**
 * @brief USB Serial Manager for log file access via USB CDC serial port.
 * 
 * Supports the following commands:
 * - LIST: List all log files
 * - GET <filename>: Retrieve contents of specified log file
 * - STATUS: Get system status (file count, storage info)
 * - DEL <filename|*>: Delete specified log file or all logs (with *)
 */
class UsbSerialManager {
public:
    UsbSerialManager();
    
    // Start USB serial communication mode
    void start();
    
    // Stop USB serial mode
    void stop();
    
    // Check if USB serial mode is active
    bool isActive() const { return m_active; }
    
    // Process incoming serial commands (call frequently in loop)
    void handleClient();
    
    // Get status message for display
    String getStatusMessage() const;

private:
    bool m_active;
    String m_commandBuffer;
    unsigned long m_lastActivityTime;
    int m_commandsProcessed;
    
    // Command handlers
    void handleCommand(const String& command);
    void handleListCommand();
    void handleGetCommand(const String& filename);
    void handleStatusCommand();
    void handleDelCommand(const String& filename);
    
    // Helper functions
    void sendOK(const String& message = "");
    void sendError(const String& message);
    bool isValidLogFilename(const String& filename);
};

#endif // USB_SERIAL_MANAGER_H
