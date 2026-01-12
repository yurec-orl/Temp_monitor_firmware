#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

class WiFiManager {
public:
    WiFiManager();
    
    // Start WiFi Access Point and web server
    bool startAP(const char* ssid, const char* password);
    
    // Stop WiFi AP and web server
    void stop();
    
    // Handle client requests (call this in loop)
    void handleClient();
    
    // Check if WiFi AP is active
    bool isActive() const { return m_isActive; }
    
    // Get AP IP address
    String getIPAddress() const;
    
    // Get number of connected clients
    int getClientCount() const;

private:
    WebServer* m_server;
    bool m_isActive;
    
    // Web server route handlers
    void handleRoot();
    void handleListLogs();
    void handleDownloadLog();
    void handleDeleteLog();
    void handleDeleteAllLogs();
    void handleNotFound();
    
    // Helper to generate HTML page
    String generateHTML(const String& content);
};

#endif // WIFI_MANAGER_H
