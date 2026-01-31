#include "usb_serial_manager.h"
#include "state.h"
#include <LittleFS.h>

// Timeout for command processing (30 seconds of inactivity)
const unsigned long COMMAND_TIMEOUT_MS = 30000;

// Maximum command buffer size
const size_t MAX_COMMAND_SIZE = 256;

UsbSerialManager::UsbSerialManager()
    : m_active(false)
    , m_lastActivityTime(0)
    , m_commandsProcessed(0)
{
    m_commandBuffer.reserve(MAX_COMMAND_SIZE);
}

void UsbSerialManager::start()
{
    if (m_active) {
        return;
    }
    
    m_active = true;
    m_commandBuffer = "";
    m_lastActivityTime = millis();
    m_commandsProcessed = 0;
    
    Serial.println();
    Serial.println("=== USB Serial Mode ===");
    Serial.println("Commands: LIST, GET <filename>, STATUS, DEL <filename|*>");
    Serial.println("Ready.");
    
    Serial.flush();
}

void UsbSerialManager::stop()
{
    if (!m_active) {
        return;
    }
    
    Serial.println();
    Serial.println("=== USB Serial Mode Stopped ===");
    Serial.flush();
    
    m_active = false;
    m_commandBuffer = "";
}

void UsbSerialManager::handleClient()
{
    if (!m_active) {
        return;
    }
    
    // Read incoming serial data
    while (Serial.available() > 0) {
        char c = Serial.read();
        m_lastActivityTime = millis();
        
        // Echo character (optional - can be disabled for cleaner output)
        Serial.print(c);
        
        if (c == '\n' || c == '\r') {
            // Process command on newline
            if (m_commandBuffer.length() > 0) {
                handleCommand(m_commandBuffer);
                m_commandBuffer = "";
            }
        } else if (m_commandBuffer.length() < MAX_COMMAND_SIZE) {
            m_commandBuffer += c;
        } else {
            // Buffer overflow - clear and report error
            m_commandBuffer = "";
            sendError("Command too long");
        }
    }
}

String UsbSerialManager::getStatusMessage() const
{
    if (!m_active) {
        return "USB Serial: Inactive";
    }
    
    String msg = "USB Serial: Active\n";
    msg += "Commands: " + String(m_commandsProcessed);
    return msg;
}

void UsbSerialManager::handleCommand(const String& command)
{
    // Trim whitespace
    String cmd = command;
    cmd.trim();
    
    // Convert to uppercase for case-insensitive comparison
    String cmdUpper = cmd;
    cmdUpper.toUpperCase();
    
    Serial.println(); // New line before processing
    
    m_commandsProcessed++;
    
    if (cmdUpper == "LIST") {
        handleListCommand();
    } else if (cmdUpper.startsWith("GET ")) {
        String filename = cmd.substring(4);
        filename.trim();
        handleGetCommand(filename);
    } else if (cmdUpper == "STATUS") {
        handleStatusCommand();
    } else if (cmdUpper.startsWith("DEL ")) {
        String filename = cmd.substring(4);
        filename.trim();
        handleDelCommand(filename);
    } else {
        sendError("Unknown command: " + cmd);
        Serial.println("Valid commands: LIST, GET <filename>, STATUS, DEL <filename|*>");
    }
}

void UsbSerialManager::handleListCommand()
{
    File root = LittleFS.open("/");
    if (!root) {
        sendError("Failed to open root directory");
        return;
    }
    
    int fileCount = 0;
    Serial.println("--- LOG FILES ---");
    
    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        
        // Check if filename matches pattern "log_XXXX.csv"
        if (filename.startsWith("log_") && filename.endsWith(".csv")) {
            size_t fileSize = file.size();
            Serial.print(filename);
            Serial.print(" (");
            Serial.print(fileSize);
            Serial.println(" bytes)");
            fileCount++;
        }
        
        file = root.openNextFile();
    }
    
    Serial.println("--- END LIST ---");
    Serial.print("Total: ");
    Serial.print(fileCount);
    Serial.println(" files");
    sendOK("LIST");
}

void UsbSerialManager::handleGetCommand(const String& filename)
{
    // Validate filename
    if (!isValidLogFilename(filename)) {
        sendError("Invalid filename format. Expected: log_XXXX.csv");
        return;
    }
    
    // Ensure filename starts with /
    String fullPath = filename;
    if (!fullPath.startsWith("/")) {
        fullPath = "/" + fullPath;
    }
    
    // Check if file exists
    if (!LittleFS.exists(fullPath)) {
        sendError("File not found: " + filename);
        return;
    }
    
    // Open file for reading
    File file = LittleFS.open(fullPath, "r");
    if (!file) {
        sendError("Failed to open file: " + filename);
        return;
    }
    
    // Send file header
    Serial.println("--- BEGIN FILE: " + filename + " ---");
    
    // Stream file contents
    while (file.available()) {
        Serial.write(file.read());
    }
    
    file.close();
    
    // Send file footer
    Serial.println();
    Serial.println("--- END FILE: " + filename + " ---");
    sendOK("GET");
}

void UsbSerialManager::handleStatusCommand()
{
    Serial.println("--- SYSTEM STATUS ---");
    
    // File system info
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    size_t freeBytes = totalBytes - usedBytes;
    
    Serial.print("Storage Total: ");
    Serial.print(totalBytes);
    Serial.println(" bytes");
    
    Serial.print("Storage Used: ");
    Serial.print(usedBytes);
    Serial.print(" bytes (");
    Serial.print((usedBytes * 100) / totalBytes);
    Serial.println("%)");
    
    Serial.print("Storage Free: ");
    Serial.print(freeBytes);
    Serial.println(" bytes");
    
    // Log file count
    int logCount = g_logger.getLogCount();
    Serial.print("Log Files: ");
    Serial.println(logCount);
    
    // Current recording status
    if (g_logger.isRecording()) {
        Serial.print("Recording: YES (log_");
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d", g_logger.getCurrentLogNumber());
        Serial.print(buf);
        Serial.println(".csv)");
    } else {
        Serial.println("Recording: NO");
    }
    
    // Commands processed
    Serial.print("Commands Processed: ");
    Serial.println(m_commandsProcessed);
    
    Serial.println("--- END STATUS ---");
    sendOK("STATUS");
}

void UsbSerialManager::handleDelCommand(const String& filename)
{
    String trimmedFilename = filename;
    trimmedFilename.trim();
    
    // Check for wildcard deletion (all logs)
    if (trimmedFilename == "*") {
        Serial.println("Deleting ALL log files...");
        
        int deletedCount = 0;
        int failedCount = 0;
        
        File root = LittleFS.open("/");
        if (!root) {
            sendError("Failed to open root directory");
            return;
        }
        
        // Build list of files to delete (can't delete while iterating)
        String filesToDelete[100]; // Max 100 files
        int fileCount = 0;
        
        File file = root.openNextFile();
        while (file && fileCount < 100) {
            String fname = file.name();
            if (fname.startsWith("log_") && fname.endsWith(".csv")) {
                filesToDelete[fileCount++] = fname;
            }
            file = root.openNextFile();
        }
        
        // Delete all files
        for (int i = 0; i < fileCount; i++) {
            String fullPath = filesToDelete[i];
            if (!fullPath.startsWith("/")) {
                fullPath = "/" + fullPath;
            }
            
            if (LittleFS.remove(fullPath)) {
                Serial.print("Deleted: ");
                Serial.println(filesToDelete[i]);
                deletedCount++;
            } else {
                Serial.print("Failed to delete: ");
                Serial.println(filesToDelete[i]);
                failedCount++;
            }
        }
        
        Serial.print("Deleted ");
        Serial.print(deletedCount);
        Serial.print(" files");
        if (failedCount > 0) {
            Serial.print(", ");
            Serial.print(failedCount);
            Serial.print(" failed");
        }
        Serial.println();
        
        sendOK("DEL *");
        return;
    }
    
    // Single file deletion
    if (!isValidLogFilename(trimmedFilename)) {
        sendError("Invalid filename format. Expected: log_XXXX.csv or *");
        return;
    }
    
    String fullPath = trimmedFilename;
    if (!fullPath.startsWith("/")) {
        fullPath = "/" + fullPath;
    }
    
    if (!LittleFS.exists(fullPath)) {
        sendError("File not found: " + trimmedFilename);
        return;
    }
    
    if (LittleFS.remove(fullPath)) {
        Serial.print("Deleted: ");
        Serial.println(trimmedFilename);
        sendOK("DEL");
    } else {
        sendError("Failed to delete: " + trimmedFilename);
    }
}

void UsbSerialManager::sendOK(const String& message)
{
    Serial.print("OK");
    if (message.length() > 0) {
        Serial.print(" ");
        Serial.print(message);
    }
    Serial.println();
    Serial.flush();
}

void UsbSerialManager::sendError(const String& message)
{
    Serial.print("ERROR: ");
    Serial.println(message);
    Serial.flush();
}

bool UsbSerialManager::isValidLogFilename(const String& filename)
{
    // Remove leading slash if present
    String fname = filename;
    if (fname.startsWith("/")) {
        fname = fname.substring(1);
    }
    
    // Check format: log_XXXX.csv
    if (!fname.startsWith("log_") || !fname.endsWith(".csv")) {
        return false;
    }
    
    // Check length (log_0000.csv = 12 characters minimum)
    if (fname.length() < 12) {
        return false;
    }
    
    return true;
}
