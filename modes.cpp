#include "modes.h"
#include "config.h"
#include "hardware.h"
#include "state.h"
#include "display.h"

// --- Mode handlers -----------------------------------------------------------

void handleStandbyMode(const float tempsC[])
{
    // Standby: just show status line, no graph yet
    drawStatusLine(tempsC, SENSOR_COUNT);
}

void handleRecordMode(const float tempsC[])
{
    // Record mode: status + graph

    // Store old range to detect changes
    float oldGraphMin = g_graphMinTemp;
    float oldGraphMax = g_graphMaxTemp;
    
    // Add new data and update running min/max incrementally
    for (int i = 0; i < SENSOR_COUNT; ++i)
    {
        float v = tempsC[i];

        // Handle first reading after hot-plug
        if (v == 85.0 && g_sensorValues[i].getLatest() == DEVICE_DISCONNECTED_C)
        {
            v = DEVICE_DISCONNECTED_C;
        }

        // Check if buffer is full and we're about to push out oldest value
        if (g_sensorValues[i].size() == GRAPH_BUFFER_SIZE)
        {
            float oldest = g_sensorValues[i].get(g_sensorValues[i].size() - 1);
            
            // If the value being pushed out was one of our extremes,
            // we need to recalculate after adding the new value
            if (oldest != DEVICE_DISCONNECTED_C)
            {
                // Check if oldest value is at or very close to our current extremes
                // Use small epsilon for floating point comparison
                const float epsilon = 0.01f;
                if (oldest <= g_dataMinTemp + epsilon || oldest >= g_dataMaxTemp - epsilon)
                {
                    g_needMinMaxRecalc = true;
                }
            }
        }

        // Add new value to buffer
        g_sensorValues[i].add(v);

        // Update running min/max with new value (if valid)
        if (v != DEVICE_DISCONNECTED_C)
        {
            // Initialize on first valid value
            if (g_dataMinTemp == DS18B20_MAX_TEMP && g_dataMaxTemp == DS18B20_MIN_TEMP)
            {
                g_dataMinTemp = v;
                g_dataMaxTemp = v;
            }
            else
            {
                // Incremental update
                if (v < g_dataMinTemp) g_dataMinTemp = v;
                if (v > g_dataMaxTemp) g_dataMaxTemp = v;
            }
        }
    }

    // Full recalculation only when necessary (when old min/max was pushed out)
    if (g_needMinMaxRecalc)
    {
        recalculateMinMax();
    }

    // Update graph range
    updateGraphRange();

    // Check if range actually changed
    bool rangeChanged = (oldGraphMin != g_graphMinTemp || oldGraphMax != g_graphMaxTemp);

    // If range changed, clear everything and redraw axis
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

    // Update status line
    drawStatusLine(tempsC, SENSOR_COUNT);
}

void handleWifiMode(const float tempsC[])
{
    // WiFi mode: for now reuse standby display
    drawStatusLine(tempsC, SENSOR_COUNT);
}
