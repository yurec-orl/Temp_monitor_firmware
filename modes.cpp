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

    if (g_displayedChannel == CHANNEL_ALL)
    {
        // Draw all channels (reverse order to have CH1 on top)
        for (int i = SENSOR_COUNT - 1; i >= 0; --i)
        {
            // Erase previous graph
            drawGraph(g_sensorValues[i], g_graphMinTemp, g_graphMaxTemp, ILI9341_BLACK);
        }
    }
    else
    {
        // Draw selected channel only
        int channelIndex = static_cast<int>(g_displayedChannel);
        drawGraph(g_sensorValues[channelIndex], g_graphMinTemp, g_graphMaxTemp, ILI9341_BLACK);
    }

    // Store current temperature in ring buffer.
    for (int i = 0; i < SENSOR_COUNT; ++i)
    {
        float v = tempsC[i];

        if (v == 85.0 && g_sensorValues[i].getLatest() == DEVICE_DISCONNECTED_C)
        {
            v = DEVICE_DISCONNECTED_C;      // First reading after hot-plug is incorrect
        }

        g_sensorValues[i].add(v);

        if (v != DEVICE_DISCONNECTED_C)
        {
            if (v < g_dataMinTemp)
                g_dataMinTemp = v;
            if (v > g_dataMaxTemp)
                g_dataMaxTemp = v;
        }
    }

    // Update min/max temp
    g_dataMaxTemp = DS18B20_MIN_TEMP;
    g_dataMinTemp = DS18B20_MAX_TEMP;

    for (int i = 0; i < SENSOR_COUNT; ++i)
    {
        for (int j = 0; j < g_sensorValues[i].size(); ++j)
        {
            float v = g_sensorValues[i].get(j);
            if (v != DEVICE_DISCONNECTED_C)
            {
                if (v < g_dataMinTemp)
                    g_dataMinTemp = v;
                if (v > g_dataMaxTemp)
                    g_dataMaxTemp = v;
            }
        }
    }

    // Reset graph range so it could be recalculated with new min/max temp
    g_graphMinTemp = DS18B20_MAX_TEMP;
    g_graphMaxTemp = DS18B20_MIN_TEMP;

    // if (updateGraphRange())
    // {
    //     // Clear the graph area if the range was updated - redraw whole graph with new range
    //     tft.fillRect(0, 0, tft.width(), tft.height(), ILI9341_BLACK);
    // }
    updateGraphRange();

    drawGraphAxis(g_graphMinTemp, g_graphMaxTemp);

    if (g_displayedChannel == CHANNEL_ALL)
    {
        // Draw all channels (reverse order to have CH1 on top)
        for (int i = SENSOR_COUNT - 1; i >= 0; --i)
        {
            // Erase previous graph
            drawGraph(g_sensorValues[i], g_graphMinTemp, g_graphMaxTemp, g_channelColors[i]);
        }
    }
    else
    {
        // Draw selected channel only
        int channelIndex = static_cast<int>(g_displayedChannel);
        drawGraph(g_sensorValues[channelIndex], g_graphMinTemp, g_graphMaxTemp, g_channelColors[channelIndex]);
    }

    drawStatusLine(tempsC, SENSOR_COUNT);
}

void handleWifiMode(const float tempsC[])
{
    // WiFi mode: for now reuse standby display
    drawStatusLine(tempsC, SENSOR_COUNT);
}
