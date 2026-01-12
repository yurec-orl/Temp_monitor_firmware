#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "FS.h"
#include <LittleFS.h>
#include "config.h"

class TemperatureLogger {
public:
    TemperatureLogger();
    
    // Start a new recording session (creates new log file)
    bool startRecording(SamplingFrequency samplingFreq);
    
    // Stop current recording session
    void stopRecording();
    
    // Log a temperature reading (timestamp in ms, temperatures for all channels)
    bool logTemperature(unsigned long timestampMs, const float tempsC[]);
    
    // Check if currently recording
    bool isRecording() const { return m_isRecording; }
    
    // Get current log number
    int getCurrentLogNumber() const { return m_currentLogNumber; }
    
    // Get error message if any
    const char* getErrorMessage() const { return m_errorMessage; }
    
    // Check if there's an error
    bool hasError() const { return m_hasError; }
    
    // Clear error state
    void clearError() { m_hasError = false; m_errorMessage[0] = '\0'; }
    
    // Get list of existing log files (returns count)
    int getLogFileList(String* fileList, int maxCount);
    
    // Delete a specific log file by number
    bool deleteLog(int logNumber);
    
    // Get total number of log files
    int getLogCount();

private:
    bool m_isRecording;
    int m_currentLogNumber;
    File m_currentLogFile;
    unsigned long m_recordingStartTime;
    int m_samplesWritten;
    
    bool m_hasError;
    char m_errorMessage[64];
    
    // Find next available log number
    int findNextLogNumber();
    
    // Generate log filename from number
    String getLogFilename(int logNumber);
    
    // Check available space and delete old logs if needed
    bool ensureSpace(size_t requiredBytes);
    
    // Delete oldest log file
    bool deleteOldestLog();
    
    // Find oldest log file
    int findOldestLog();
    
    // Set error message
    void setError(const char* message);
    
    // Get sampling frequency description
    const char* getSamplingFreqDescription(SamplingFrequency freq);
};

#endif // LOGGER_H
