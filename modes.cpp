#include "modes.h"
#include "config.h"
#include "hardware.h"
#include "state.h"
#include "display.h"

// --- Mode handlers -----------------------------------------------------------

void handleStandbyMode(const float tempsC[])
{
    // Standby: show status line and system status.
    drawStatusLine(tempsC, SENSOR_COUNT);
    drawStandbyStatus();
}

void handleRecordMode(const float tempsC[])
{
    // Record mode: status + graph.
    
    // Start logging if not already started.
    if (!g_logger.isRecording()) {
        g_logger.startRecording(g_samplingFreq);
    }
    
    // Log the temperature reading.
    g_logger.logTemperature(millis(), tempsC);

    // Store old range to detect changes.
    float oldGraphMin = g_graphMinTemp;
    float oldGraphMax = g_graphMaxTemp;
    
    // Add new data and update running min/max incrementally.
    for (int i = 0; i < SENSOR_COUNT; ++i)
    {
        float v = tempsC[i];

        // Handle first reading after hot-plug.
        if (v == 85.0 && g_sensorValues[i].getLatest() == DEVICE_DISCONNECTED_C)
        {
            v = DEVICE_DISCONNECTED_C;
        }

        // Check if buffer is full and we're about to push out oldest value.
        if (g_sensorValues[i].size() == GRAPH_BUFFER_SIZE)
        {
            float oldest = g_sensorValues[i].get(g_sensorValues[i].size() - 1);
            
            // If the value being pushed out was one of our extremes,
            // we need to recalculate after adding the new value.
            if (oldest != DEVICE_DISCONNECTED_C)
            {
                // Check if oldest value is at or very close to our current extremes.
                // Use small epsilon for floating point comparison.
                const float epsilon = 0.01f;
                if (oldest <= g_dataMinTemp + epsilon || oldest >= g_dataMaxTemp - epsilon)
                {
                    g_needMinMaxRecalc = true;
                }
            }
        }

        // Add new value to buffer.
        g_sensorValues[i].add(v);

        // Update running min/max with new value (if valid).
        // Only do incremental update if we're NOT about to do a full recalc.
        if (!g_needMinMaxRecalc && v != DEVICE_DISCONNECTED_C)
        {
            // Initialize on first valid value.
            if (g_dataMinTemp == DS18B20_MAX_TEMP && g_dataMaxTemp == DS18B20_MIN_TEMP)
            {
                g_dataMinTemp = v;
                g_dataMaxTemp = v;
            }
            else
            {
                // Incremental update - only expand, never shrink.
                if (v < g_dataMinTemp) g_dataMinTemp = v;
                if (v > g_dataMaxTemp) g_dataMaxTemp = v;
            }
        }
    }

    // Full recalculation when necessary (when old min/max was pushed out).
    // This scans the entire buffer and will correctly shrink the range if needed.
    if (g_needMinMaxRecalc)
    {
        recalculateMinMax();
    }

    // Update graph range.
    updateGraphRange();

    // Check if range actually changed.
    bool rangeChanged = (oldGraphMin != g_graphMinTemp || oldGraphMax != g_graphMaxTemp);

    // If range changed, clear everything and redraw axis.
    if (rangeChanged)
    {
        clearGraphArea();
    }

    drawGraphAxis(g_graphMinTemp, g_graphMaxTemp);
    
    if (g_displayedChannel == CHANNEL_ALL)
    {
        for (int i = SENSOR_COUNT - 1; i >= 0; --i)
        {
            drawGraph(g_sensorValues[i], g_graphMinTemp, g_graphMaxTemp, g_channelColors[i]);
        }
    }
    else
    {
        int channelIndex = static_cast<int>(g_displayedChannel);
        drawGraph(g_sensorValues[channelIndex], g_graphMinTemp, g_graphMaxTemp, g_channelColors[channelIndex]);
    }

    // Update status line.
    drawStatusLine(tempsC, SENSOR_COUNT);
}

void handleWifiMode(const float tempsC[])
{
    // WiFi mode: Start AP and web server.
    
    // Start WiFi AP if not already started (safe to call multiple times).
    if (!g_wifiManager.isActive()) {
        Serial.println("Starting WiFi AP from handleWifiMode...");
        g_wifiManager.startAP("ESP32_TempLogger", "temperature");
    }
    
    // Display WiFi status.
    drawStatusLine(tempsC, SENSOR_COUNT);
    
    // Show WiFi info on screen (static display, safe to redraw).
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
    
    tft.setCursor(10, 50);
    tft.print("WiFi AP Active     ");  // Extra spaces to clear old text
    
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    
    tft.setCursor(10, 80);
    tft.print("SSID: ESP32_TempLogger     ");
    
    tft.setCursor(10, 95);
    tft.print("Password: temperature      ");
    
    tft.setCursor(10, 110);
    tft.print("IP: ");
    tft.print(g_wifiManager.getIPAddress());
    tft.print("          ");  // Clear any leftover text
    
    tft.setCursor(10, 130);
    tft.print("Clients: ");
    tft.print(g_wifiManager.getClientCount());
    tft.print("     ");  // Clear any leftover text
    
    tft.setCursor(10, 150);
    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    tft.print("Access logs at:              ");
    tft.setCursor(10, 165);
    tft.print("http://");
    tft.print(g_wifiManager.getIPAddress());
    tft.print("/logs          ");
}
