#include "logger.h"

// Reserve space for log operations (e.g., 100KB)
const size_t MIN_FREE_SPACE = 100 * 1024;

// Maximum single log file size (e.g., 1MB)
const size_t MAX_LOG_SIZE = 1 * 1024 * 1024;

TemperatureLogger::TemperatureLogger()
    : m_isRecording(false)
    , m_currentLogNumber(0)
    , m_recordingStartTime(0)
    , m_samplesWritten(0)
    , m_hasError(false)
{
    m_errorMessage[0] = '\0';
}

bool TemperatureLogger::startRecording(SamplingFrequency samplingFreq)
{
    if (m_isRecording) {
        stopRecording();
    }
    
    // Clear any previous errors
    clearError();
    
    // Find next log number
    m_currentLogNumber = findNextLogNumber();
    
    // Generate filename
    String filename = getLogFilename(m_currentLogNumber);
    
    // Ensure we have enough space
    if (!ensureSpace(MIN_FREE_SPACE)) {
        setError("Insufficient space");
        return false;
    }
    
    // Open file for writing
    m_currentLogFile = LittleFS.open(filename, "w");
    if (!m_currentLogFile) {
        setError("Failed to create log");
        return false;
    }
    
    // Write header
    m_currentLogFile.print("#Temperature log ");
    m_currentLogFile.println(m_currentLogNumber);
    m_currentLogFile.println("#device=ESP32_TEMPERATURE_LOGGER");
    m_currentLogFile.print("#sampling=");
    m_currentLogFile.println(getSamplingFreqDescription(samplingFreq));
    m_currentLogFile.println("#timestamp_ms,ch1_c,ch2_c,ch3_c,ch4_c");
    
    m_recordingStartTime = millis();
    m_samplesWritten = 0;
    m_isRecording = true;
    
    Serial.print("Started recording to ");
    Serial.println(filename);
    
    return true;
}

void TemperatureLogger::stopRecording()
{
    if (!m_isRecording) {
        return;
    }
    
    if (m_currentLogFile) {
        m_currentLogFile.close();
        
        String filename = getLogFilename(m_currentLogNumber);
        Serial.print("Stopped recording. Wrote ");
        Serial.print(m_samplesWritten);
        Serial.print(" samples to ");
        Serial.println(filename);
    }
    
    m_isRecording = false;
    m_samplesWritten = 0;
}

bool TemperatureLogger::logTemperature(unsigned long timestampMs, const float tempsC[])
{
    if (!m_isRecording || !m_currentLogFile) {
        return false;
    }
    
    // Check if log file is getting too large
    size_t currentSize = m_currentLogFile.size();
    if (currentSize > MAX_LOG_SIZE) {
        setError("Log file too large");
        stopRecording();
        return false;
    }
    
    // Write timestamp
    m_currentLogFile.print(timestampMs);
    
    // Write temperature values for all channels
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        m_currentLogFile.print(",");
        
        // Only write value if sensor reading is valid
        if (tempsC[i] >= DS18B20_MIN_TEMP && tempsC[i] <= DS18B20_MAX_TEMP) {
            m_currentLogFile.print(tempsC[i], 2); // 2 decimal places
        }
        // else leave empty (just the comma)
    }
    
    m_currentLogFile.println();
    
    // Flush periodically to ensure data is written
    m_samplesWritten++;
    if (m_samplesWritten % 10 == 0) {
        m_currentLogFile.flush();
    }
    
    return true;
}

int TemperatureLogger::findNextLogNumber()
{
    int maxLogNumber = 0;
    
    File root = LittleFS.open("/");
    if (!root) {
        return 1;
    }
    
    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        
        // Check if filename matches pattern "log_XXXX.csv"
        if (filename.startsWith("log_") && filename.endsWith(".csv")) {
            // Extract number
            int startIdx = 4; // After "log_"
            int endIdx = filename.indexOf(".csv");
            if (endIdx > startIdx) {
                String numberStr = filename.substring(startIdx, endIdx);
                int logNumber = numberStr.toInt();
                if (logNumber > maxLogNumber) {
                    maxLogNumber = logNumber;
                }
            }
        }
        
        file = root.openNextFile();
    }
    
    return maxLogNumber + 1;
}

String TemperatureLogger::getLogFilename(int logNumber)
{
    char filename[32];
    snprintf(filename, sizeof(filename), "/log_%04d.csv", logNumber);
    return String(filename);
}

bool TemperatureLogger::ensureSpace(size_t requiredBytes)
{
    size_t totalSpace = LittleFS.totalBytes();
    size_t usedSpace = LittleFS.usedBytes();
    size_t freeSpace = totalSpace - usedSpace;
    
    // If we have enough free space, we're good
    if (freeSpace >= requiredBytes) {
        return true;
    }
    
    // Try deleting old logs until we have enough space
    Serial.println("Insufficient space, deleting old logs...");
    
    while (freeSpace < requiredBytes) {
        int oldestLog = findOldestLog();
        if (oldestLog == -1) {
            // No more logs to delete
            Serial.println("Cannot free enough space");
            return false;
        }
        
        if (!deleteOldestLog()) {
            Serial.println("Failed to delete old log");
            return false;
        }
        
        // Recalculate free space
        usedSpace = LittleFS.usedBytes();
        freeSpace = totalSpace - usedSpace;
        
        Serial.print("Deleted log ");
        Serial.print(oldestLog);
        Serial.print(", free space: ");
        Serial.println(freeSpace);
    }
    
    return true;
}

bool TemperatureLogger::deleteOldestLog()
{
    int oldestLog = findOldestLog();
    if (oldestLog == -1) {
        return false;
    }
    
    return deleteLog(oldestLog);
}

int TemperatureLogger::findOldestLog()
{
    int oldestLogNumber = -1;
    
    File root = LittleFS.open("/");
    if (!root) {
        return -1;
    }
    
    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        
        // Check if filename matches pattern "log_XXXX.csv"
        if (filename.startsWith("log_") && filename.endsWith(".csv")) {
            // Extract number
            int startIdx = 4; // After "log_"
            int endIdx = filename.indexOf(".csv");
            if (endIdx > startIdx) {
                String numberStr = filename.substring(startIdx, endIdx);
                int logNumber = numberStr.toInt();
                if (oldestLogNumber == -1 || logNumber < oldestLogNumber) {
                    oldestLogNumber = logNumber;
                }
            }
        }
        
        file = root.openNextFile();
    }
    
    return oldestLogNumber;
}

bool TemperatureLogger::deleteLog(int logNumber)
{
    String filename = getLogFilename(logNumber);
    
    if (LittleFS.exists(filename)) {
        return LittleFS.remove(filename);
    }
    
    return false;
}

int TemperatureLogger::getLogFileList(String* fileList, int maxCount)
{
    int count = 0;
    
    File root = LittleFS.open("/");
    if (!root) {
        return 0;
    }
    
    File file = root.openNextFile();
    while (file && count < maxCount) {
        String filename = file.name();
        
        // Check if filename matches pattern "log_XXXX.csv"
        if (filename.startsWith("log_") && filename.endsWith(".csv")) {
            fileList[count] = filename;
            count++;
        }
        
        file = root.openNextFile();
    }
    
    return count;
}

int TemperatureLogger::getLogCount()
{
    int count = 0;
    
    File root = LittleFS.open("/");
    if (!root) {
        return 0;
    }
    
    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        
        // Check if filename matches pattern "log_XXXX.csv"
        if (filename.startsWith("log_") && filename.endsWith(".csv")) {
            count++;
        }
        
        file = root.openNextFile();
    }
    
    return count;
}

void TemperatureLogger::setError(const char* message)
{
    m_hasError = true;
    strncpy(m_errorMessage, message, sizeof(m_errorMessage) - 1);
    m_errorMessage[sizeof(m_errorMessage) - 1] = '\0';
    
    Serial.print("Logger error: ");
    Serial.println(message);
}

const char* TemperatureLogger::getSamplingFreqDescription(SamplingFrequency freq)
{
    switch (freq) {
        case SAMPLING_FREQ_1S:    return "1s";
        case SAMPLING_FREQ_5S:    return "5s";
        case SAMPLING_FREQ_10S:   return "10s";
        case SAMPLING_FREQ_60S:   return "60s";
        case SAMPLING_FREQ_600S:  return "600s";
        case SAMPLING_FREQ_3600S: return "3600s";
        default:                  return "unknown";
    }
}
