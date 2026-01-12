#include "wifi_manager.h"
#include "esp_wifi.h"
#include "state.h"
#include <LittleFS.h>
#include <vector>
#include <algorithm>

WiFiManager::WiFiManager()
    : m_server(nullptr)
    , m_isActive(false)
{
}

bool WiFiManager::startAP(const char* ssid, const char* password)
{
    if (m_isActive) {
        Serial.println("WiFi AP already active");
        return true;
    }
    
    Serial.println("Starting WiFi Access Point...");
    
    // STEP 1: Complete WiFi shutdown (critical for ESP32-S3)
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500);  // Give time to fully shut down
    
    // STEP 2: Set mode to AP
    WiFi.mode(WIFI_AP);
    delay(200);  // Wait for mode to be established
    
    // STEP 3: Set country code to US (AFTER mode is set - important!)
    wifi_country_t country;
    country.cc[0] = 'U';
    country.cc[1] = 'S';
    country.cc[2] = '\0';
    country.schan = 1;
    country.nchan = 11;
    country.max_tx_power = 20;
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;
    esp_wifi_set_country(&country);
    
    // STEP 4: Set transmit power
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    
    // STEP 5: Start AP with explicit parameters
    Serial.print("Starting AP: ");
    Serial.println(ssid);
    
    // Channel 6, not hidden (false), max 4 connections
    bool success = WiFi.softAP(ssid, password, 6, false, 4);
    
    if (!success) {
        Serial.println("✗ Failed to start AP");
        return false;
    }
    
    delay(500);  // Give AP time to fully initialize
    
    // Get and display the IP address
    IPAddress IP = WiFi.softAPIP();
    Serial.print("✓ AP Started! IP address: ");
    Serial.println(IP);
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("MAC: ");
    Serial.println(WiFi.softAPmacAddress());
    
    // Create web server on port 80
    m_server = new WebServer(80);
    
    // Set up routes
    m_server->on("/", [this]() { this->handleRoot(); });
    m_server->on("/logs", [this]() { this->handleListLogs(); });
    m_server->on("/download", [this]() { this->handleDownloadLog(); });
    m_server->on("/delete", [this]() { this->handleDeleteLog(); });
    m_server->on("/deleteall", [this]() { this->handleDeleteAllLogs(); });
    m_server->onNotFound([this]() { this->handleNotFound(); });
    
    // Start server
    m_server->begin();
    Serial.println("Web server started on port 80");
    
    m_isActive = true;
    return true;
}

void WiFiManager::stop()
{
    if (!m_isActive) {
        return;
    }
    
    Serial.println("Stopping WiFi AP...");
    
    if (m_server) {
        m_server->stop();
        delete m_server;
        m_server = nullptr;
    }
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    
    m_isActive = false;
    Serial.println("WiFi AP stopped");
}

void WiFiManager::handleClient()
{
    if (m_isActive && m_server) {
        m_server->handleClient();
    }
}

String WiFiManager::getIPAddress() const
{
    if (m_isActive) {
        return WiFi.softAPIP().toString();
    }
    return "N/A";
}

int WiFiManager::getClientCount() const
{
    if (m_isActive) {
        return WiFi.softAPgetStationNum();
    }
    return 0;
}

void WiFiManager::handleRoot()
{
    String content = "<h1>ESP32 Temperature Logger</h1>";
    content += "<p>Access Point Mode</p>";
    content += "<p><a href='/logs'>View/Download Logs</a></p>";
    content += "<p>Connected clients: " + String(getClientCount()) + "</p>";
    
    m_server->send(200, "text/html", generateHTML(content));
}

void WiFiManager::handleListLogs()
{
    String content = "<h1>Temperature Logs</h1>";
    content += "<p><a href='/'>Back to Home</a></p>";
    
    content += "<table border='1' cellpadding='5' cellspacing='0'>";
    content += "<tr><th>Filename</th><th>Size (bytes)</th><th>Actions</th></tr>";
    
    // Collect all log files with their info using std::vector
    struct LogFileInfo {
        String filename;
        size_t fileSize;
    };
    
    std::vector<LogFileInfo> logFiles;
    
    File root = LittleFS.open("/");
    if (root) {
        File file = root.openNextFile();
        
        while (file) {
            String filename = String(file.name());
            
            // Ensure filename starts with / for consistency
            if (!filename.startsWith("/")) {
                filename = "/" + filename;
            }
            
            Serial.print("Found file: ");
            Serial.println(filename);
            
            // Check if it's a log file (match both /log_ and log_ patterns)
            if ((filename.startsWith("/log_") || filename.startsWith("log_")) && filename.endsWith(".csv")) {
                LogFileInfo info;
                info.filename = filename;
                info.fileSize = file.size();
                logFiles.push_back(info);
            }
            
            file.close();
            file = root.openNextFile();
        }
        root.close();
        
        if (logFiles.size() > 0) {
            // Sort in reverse order (newest/highest number first) using std::sort
            std::sort(logFiles.begin(), logFiles.end(), [](const LogFileInfo& a, const LogFileInfo& b) {
                return a.filename > b.filename;  // Descending order
            });
            
            // Display sorted logs
            for (const auto& logInfo : logFiles) {
                content += "<tr>";
                content += "<td>" + logInfo.filename + "</td>";
                content += "<td>" + String(logInfo.fileSize) + "</td>";
                content += "<td>";
                content += "<a href='/download?file=" + logInfo.filename + "'>Download</a> | ";
                content += "<a href='/delete?file=" + logInfo.filename + "' onclick='return confirm(\"Delete this log?\")'>Delete</a>";
                content += "</td>";
                content += "</tr>";
            }
        } else {
            content += "<tr><td colspan='3'>No log files found</td></tr>";
        }
    } else {
        content += "<tr><td colspan='3'>Error reading filesystem</td></tr>";
    }
    
    content += "</table>";
    
    // Add "Delete All" button at the bottom if there are logs
    if (logFiles.size() > 0) {
        content += "<p style='margin-top: 20px;'><a href='/deleteall' onclick='return confirm(\"Delete ALL " + String(logFiles.size()) + " log files? This cannot be undone!\")' style='color: red; font-weight: bold;'>Delete All Logs</a></p>";
    }
    
    m_server->send(200, "text/html", generateHTML(content));
}
void WiFiManager::handleDownloadLog()
{
    if (!m_server->hasArg("file")) {
        m_server->send(400, "text/plain", "Missing file parameter");
        return;
    }
    
    String filename = m_server->arg("file");
    
    // Ensure filename has leading slash
    if (!filename.startsWith("/")) {
        filename = "/" + filename;
    }
    
    // Security check - ensure filename contains log_ and ends with .csv
    if ((!filename.startsWith("/log_") && filename.indexOf("log_") == -1) || !filename.endsWith(".csv")) {
        m_server->send(403, "text/plain", "Invalid file");
        return;
    }
    
    if (!LittleFS.exists(filename)) {
        // Try without leading slash
        String altFilename = filename.substring(1);
        if (!LittleFS.exists(altFilename)) {
            m_server->send(404, "text/plain", "File not found");
            return;
        }
        filename = altFilename;
    }
    
    File file = LittleFS.open(filename, "r");
    if (!file) {
        m_server->send(500, "text/plain", "Failed to open file");
        return;
    }
    
    // Extract just the filename (without path) for the download
    String displayName = filename;
    if (displayName.startsWith("/")) {
        displayName = displayName.substring(1);
    }
    
    // Set Content-Disposition header to specify filename
    m_server->sendHeader("Content-Disposition", "attachment; filename=\"" + displayName + "\"");
    
    // Stream file to client
    m_server->streamFile(file, "text/csv");
    file.close();
    
    Serial.print("Downloaded: ");
    Serial.println(filename);
}

void WiFiManager::handleDeleteLog()
{
    if (!m_server->hasArg("file")) {
        m_server->send(400, "text/plain", "Missing file parameter");
        return;
    }
    
    String filename = m_server->arg("file");
    
    // Ensure filename has leading slash
    if (!filename.startsWith("/")) {
        filename = "/" + filename;
    }
    
    // Security check
    if ((!filename.startsWith("/log_") && filename.indexOf("log_") == -1) || !filename.endsWith(".csv")) {
        m_server->send(403, "text/plain", "Invalid file");
        return;
    }
    
    if (!LittleFS.exists(filename)) {
        // Try without leading slash
        String altFilename = filename.substring(1);
        if (!LittleFS.exists(altFilename)) {
            m_server->send(404, "text/plain", "File not found");
            return;
        }
        filename = altFilename;
    }
    
    if (LittleFS.remove(filename)) {
        Serial.print("Deleted: ");
        Serial.println(filename);
        
        // Redirect back to logs page
        m_server->sendHeader("Location", "/logs");
        m_server->send(303);
    } else {
        m_server->send(500, "text/plain", "Failed to delete file");
    }
}

void WiFiManager::handleDeleteAllLogs()
{
    int deletedCount = 0;
    int failedCount = 0;
    
    File root = LittleFS.open("/");
    if (!root) {
        m_server->send(500, "text/plain", "Failed to open filesystem");
        return;
    }
    
    // Collect all log filenames using std::vector
    std::vector<String> logFiles;
    
    File file = root.openNextFile();
    while (file) {
        String filename = String(file.name());
        
        // Ensure filename starts with / for consistency
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        
        // Check if it's a log file
        if ((filename.startsWith("/log_") || filename.startsWith("log_")) && filename.endsWith(".csv")) {
            logFiles.push_back(filename);
        }
        
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    // Delete all collected log files
    for (const auto& filename : logFiles) {
        // Try with leading slash first
        if (LittleFS.exists(filename)) {
            if (LittleFS.remove(filename)) {
                deletedCount++;
                Serial.print("Deleted: ");
                Serial.println(filename);
            } else {
                failedCount++;
            }
        } else {
            // Try without leading slash
            String altFilename = filename.substring(1);
            if (LittleFS.exists(altFilename)) {
                if (LittleFS.remove(altFilename)) {
                    deletedCount++;
                    Serial.print("Deleted: ");
                    Serial.println(altFilename);
                } else {
                    failedCount++;
                }
            }
        }
    }
    
    Serial.print("Deleted ");
    Serial.print(deletedCount);
    Serial.println(" log files");
    
    if (failedCount > 0) {
        Serial.print("Failed to delete ");
        Serial.print(failedCount);
        Serial.println(" files");
    }
    
    // Redirect back to logs page
    m_server->sendHeader("Location", "/logs");
    m_server->send(303);
}

void WiFiManager::handleNotFound()
{
    String content = "<h1>404 - Not Found</h1>";
    content += "<p>The requested page was not found.</p>";
    content += "<p><a href='/'>Go to Home</a></p>";
    
    m_server->send(404, "text/html", generateHTML(content));
}

String WiFiManager::generateHTML(const String& content)
{
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>ESP32 Temperature Logger</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += "h1 { color: #333; }";
    html += "a { color: #0066cc; text-decoration: none; }";
    html += "a:hover { text-decoration: underline; }";
    html += "table { background: white; margin-top: 20px; }";
    html += "th { background: #0066cc; color: white; }";
    html += "</style>";
    html += "</head><body>";
    html += content;
    html += "</body></html>";
    
    return html;
}
