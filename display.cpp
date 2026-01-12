#include <Arduino.h>

#include "display.h"
#include "hardware.h"
#include "state.h"
#include "ring_buffer.h"

// Enable profiling output (comment out to disable)
// #define ENABLE_DISPLAY_PROFILING

// Draw 2-line status at top of the content area
void drawStatusLine(const float tempsC[], int count)
{
    const int16_t baseX = 2;
    const int16_t baseY = 2;

    tft.setTextSize(1);

    // Row 1: MODE + CHANNEL
    tft.setCursor(baseX, baseY);
    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);

    // Mode
    tft.print("MODE: ");
    switch (g_mode)
    {
    case MODE_STANDBY:
        tft.print("STDBY");
        break;
    case MODE_RECORD:
        tft.print("  REC");
        break;
    case MODE_WIFI:
        tft.print(" WIFI");
        break;
    default:
        tft.print("    ?");
        break;
    }

    // Channel to display on graph
    tft.print("  CH: ");
    switch (g_displayedChannel)
    {
    case CHANNEL_1:
        tft.print("  1");
        break;
    case CHANNEL_2:
        tft.print("  2");
        break;
    case CHANNEL_3:
        tft.print("  3");
        break;
    case CHANNEL_4:
        tft.print("  4");
        break;
    case CHANNEL_ALL:
        tft.print("ALL");
        break;
    default:
        tft.print("  ?");
        break;
    }

    // Sampling rate
    tft.print("  FREQ: ");
    switch (g_samplingFreq)
    {
    case SAMPLING_FREQ_1S:
        tft.print(" 1s");
        break;
    case SAMPLING_FREQ_5S:
        tft.print(" 5s");
        break;
    case SAMPLING_FREQ_10S:
        tft.print("10s");
        break;
    case SAMPLING_FREQ_60S:
        tft.print(" 1m");
        break;
    case SAMPLING_FREQ_600S:
        tft.print("10m");
        break;
    case SAMPLING_FREQ_3600S:
        tft.print(" 1h");
        break;
    default:
        tft.print("  ?");
        break;
    }

    if (g_mode == MODE_RECORD)
    {
        tft.print("  LOG: ");
        // Log number, 4 digits, zero padding
        char buf[8];
        snprintf(buf, sizeof(buf), "%04d", g_logger.getCurrentLogNumber());
        tft.print(buf);
    }

    // Row 2: sensor values
    tft.setCursor(baseX, baseY + 16); // next text row

    for (int i = 0; i < count && i < SENSOR_COUNT; ++i)
    {
        // Label
        tft.setTextColor(g_channelColors[i], ILI9341_BLACK);
        tft.print("CH");
        tft.print(i + 1);
        tft.print(":");

        // Value
        if (!g_channelHasDevice[i] || tempsC[i] == DEVICE_DISCONNECTED_C)
        {
            tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
            tft.print("---.-  ");
        }
        else
        {
            tft.setTextColor(g_channelColors[i], ILI9341_BLACK);
            char buf[8];
            dtostrf(tempsC[i], 5, 1, buf);
            tft.print(buf);
            tft.print(static_cast<char>(247));
        }

        tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
        tft.print("  "); // small spacer between channels
    }
}

// Draw system status information in standby mode
void drawStandbyStatus()
{
    const int16_t baseX = 10;
    const int16_t baseY = 60;
    const int16_t lineHeight = 20;
    
    tft.setTextSize(2);
    
    // 1. Log files count
    int logCount = g_logger.getLogCount();
    tft.setCursor(baseX, baseY);
    tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
    tft.print("Logs: ");
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.print(logCount);
    tft.print("     ");  // Clear any leftover characters
    
    // 2. Total memory
    size_t totalBytes = LittleFS.totalBytes();
    tft.setCursor(baseX, baseY + lineHeight);
    tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
    tft.print("Total: ");
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.print(totalBytes / 1024);
    tft.print(" KB     ");
    
    // 3. Free memory (in red if <10%)
    size_t usedBytes = LittleFS.usedBytes();
    size_t freeBytes = totalBytes - usedBytes;
    float freePercent = (float)freeBytes / (float)totalBytes * 100.0f;
    
    tft.setCursor(baseX, baseY + lineHeight * 2);
    tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
    tft.print("Free:  ");
    
    // Red if less than 10%, otherwise white
    if (freePercent < 10.0f) {
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
    } else {
        tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    }
    tft.print(freeBytes / 1024);
    tft.print(" KB (");
    tft.print((int)freePercent);
    tft.print("%)     ");
    
    // 4. Logging time available with current sampling frequency
    // Average row: "timestamp,temp,temp,temp,temp\n" ≈ 35 bytes
    const size_t AVG_ROW_SIZE = 35;
    const size_t MIN_FREE_SPACE = 100 * 1024; // Reserve 100KB
    
    size_t availableForLogs = (freeBytes > MIN_FREE_SPACE) ? (freeBytes - MIN_FREE_SPACE) : 0;
    unsigned long maxSamples = availableForLogs / AVG_ROW_SIZE;
    
    // Get current sampling interval
    int samplingSeconds = getSamplingIntervalSeconds();
    unsigned long totalSeconds = maxSamples * samplingSeconds;
    
    tft.setCursor(baseX, baseY + lineHeight * 3);
    tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
    tft.print("Time:  ");
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    
    // Format time appropriately
    if (totalSeconds < 60) {
        tft.print(totalSeconds);
        tft.print(" sec     ");
    } else if (totalSeconds < 3600) {
        unsigned long minutes = totalSeconds / 60;
        tft.print(minutes);
        tft.print(" min     ");
    } else if (totalSeconds < 86400) {
        unsigned long hours = totalSeconds / 3600;
        tft.print(hours);
        tft.print(" hrs     ");
    } else {
        unsigned long days = totalSeconds / 86400;
        tft.print(days);
        tft.print(" days    ");
    }
    
    // 5. Logger error, if any
    tft.setCursor(baseX, baseY + lineHeight * 4);
    if (g_logger.hasError()) {
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.print("ERR: ");
        tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
        tft.setTextSize(1);  // Smaller text for error message
        tft.setCursor(baseX, baseY + lineHeight * 4 + 18);
        tft.print(g_logger.getErrorMessage());
        tft.print("                    ");  // Clear any leftover text
    } else {
        // Clear error area if no error
        tft.fillRect(baseX, baseY + lineHeight * 4, 300, 40, ILI9341_BLACK);
    }
}

// Helper function to get sampling interval in seconds
int getSamplingIntervalSeconds()
{
    switch (g_samplingFreq)
    {
    case SAMPLING_FREQ_1S:
        return 1;
    case SAMPLING_FREQ_5S:
        return 5;
    case SAMPLING_FREQ_10S:
        return 10;
    case SAMPLING_FREQ_60S:
        return 60;
    case SAMPLING_FREQ_600S:
        return 600;
    case SAMPLING_FREQ_3600S:
        return 3600;
    default:
        return 1;
    }
}

// Helper function to format time duration
// Negative values represent time in the past (e.g., -60s = "60s ago")
void formatTimeDuration(int seconds, char *buffer, size_t bufSize)
{
    // Handle negative values (time in the past)
    int absSeconds = abs(seconds);
    
    if (absSeconds == 0)
    {
        snprintf(buffer, bufSize, "0");
    }
    else if (absSeconds < 60)
    {
        snprintf(buffer, bufSize, "-%ds", absSeconds);
    }
    else if (absSeconds < 3600)
    {
        int mins = absSeconds / 60;
        snprintf(buffer, bufSize, "-%dm", mins);
    }
    else
    {
        int hours = absSeconds / 3600;
        snprintf(buffer, bufSize, "-%dh", hours);
    }
}

void drawGraphAxis(float minTemp, float maxTemp)
{
#ifdef ENABLE_DISPLAY_PROFILING
    unsigned long startTime = millis();
#endif

    // Reserve top area for status lines; graph starts below
    const int16_t x0 = GRAPH_LEFT_MARGIN;
    const int16_t x1 = tft.width() - GRAPH_RIGHT_MARGIN;        // max time
    const int16_t yTop = GRAPH_TOP_MARGIN;                      // top of graph
    const int16_t yBottom = tft.height() - GRAPH_BOTTOM_MARGIN; // bottom of graph

    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);

    const float range = maxTemp - minTemp;
    if (range <= 0.0f)
    {
        return; // invalid range
    }

    const int16_t graphWidth = x1 - x0;
    const int16_t graphHeight = yBottom - yTop;

    uint16_t gridColor = tft.color565(48, 48, 48);

    // Horizontal grid: divide temperature range into 10 intervals
    for (int i = 1; i <= 10; ++i)
    {
        float frac = i / 10.0f; // 0.1 .. 0.9
        int16_t y = yBottom - (int16_t)(frac * graphHeight + 0.5f);
        tft.drawFastHLine(x0, y, x1 - x0, gridColor);
    }

    // Vertical grid lines every 20 samples across full graph width
    for (int x = x0 + 20; x <= x1; x += 20)
    {
        tft.drawFastVLine(x, yTop, graphHeight, gridColor);
    }

    // Draw Y axis (full range) and X axis always at the bottom
    tft.drawFastVLine(x0, yTop, yBottom - yTop, ILI9341_WHITE); // Y axis (temp)
    tft.drawFastHLine(x0, yBottom, x1 - x0, ILI9341_WHITE);     // X axis (time at bottom)

    // Temperature labels on Y axis - display every 2 grid marks
    // Grid divides range into 10 intervals, so label at 0, 2, 4, 6, 8, 10
    for (int i = 0; i <= 10; i += 2)
    {
        float frac = i / 10.0f;  // 0.0, 0.2, 0.4, 0.6, 0.8, 1.0
        float temp = minTemp + frac * range;
        int16_t y = yBottom - (int16_t)(frac * graphHeight + 0.5f);
        
        // Draw temperature label with degree symbol
        tft.setCursor(4, y - 4);
        char buf[8];
        dtostrf(temp, 3, 0, buf);
        tft.print(buf);
        tft.print(static_cast<char>(247));  // °C (0xF8 is degree symbol in Adafruit GFX)
    }

    // Time marks along X axis - display every 3 grid lines (every 60 pixels)
    // Grid lines are every 20 pixels, so time marks at 60, 120, 180, etc.
    // Time goes from right (0, now) to left (negative, past)
    int samplingInterval = getSamplingIntervalSeconds();
    char timeBuffer[10];

    // Start from right edge and go left
    for (int x = x1; x >= x0; x -= 60)
    {
        // Calculate time in seconds from the right edge (now = 0)
        int pixelsFromRight = x1 - x;
        int totalSeconds = -pixelsFromRight * samplingInterval;  // Negative for past

        // Format the time duration (negative values)
        formatTimeDuration(totalSeconds, timeBuffer, sizeof(timeBuffer));

        // Display the time mark below the X axis
        // Center the text around the grid line
        int textWidth = strlen(timeBuffer) * 6; // Approximate width (6 pixels per char at text size 1)
        tft.setCursor(x - textWidth / 2, yBottom + 6);
        tft.print(timeBuffer);
    }

#ifdef ENABLE_DISPLAY_PROFILING
    unsigned long elapsedTime = millis() - startTime;
    Serial.print("[PROFILE] drawGraphAxis: ");
    Serial.print(elapsedTime);
    Serial.println(" ms");
#endif
}

// Erase and draw segment-by-segment to minimize flicker
// For each line segment: erase old (offset=1), draw new (offset=0)
void drawGraph(const RingBuffer &buffer, float minTemp, float maxTemp, uint16_t color)
{
    // Minimum 2 samples required for plotting
    if (buffer.size() <= 1) {
        return;
    }

    const int16_t x0 = tft.width() - GRAPH_RIGHT_MARGIN;
    const int16_t x1 = GRAPH_LEFT_MARGIN;
    const int16_t yTop = GRAPH_TOP_MARGIN;
    const int16_t yBottom = tft.height() - GRAPH_BOTTOM_MARGIN;

    const int16_t graphWidth = x0 - x1;
    const int16_t graphHeight = yBottom - yTop;

    const float range = maxTemp - minTemp;
    if (range <= 0.0f)
    {
        return; // invalid range
    }

    auto tempToY = [minTemp, maxTemp, range, yBottom, graphHeight](float t) -> int16_t
    {
        if (t < minTemp)
            t = minTemp;
        if (t > maxTemp)
            t = maxTemp;
        float norm = (t - minTemp) / range;
        return yBottom - (int16_t)(norm * graphHeight + 0.5f);
    };

    // Calculate how many samples to process (display size, not full buffer)
    int samplesToDraw = min((int)(buffer.size() - 1), GRAPH_DISPLAY_SIZE);

    // Track previous point for old data (offset=1)
    bool havePrevOld = false;
    int16_t prevXOld = 0;
    int16_t prevYOld = 0;

    // Track previous point for new data (offset=0)
    bool havePrevNew = false;
    int16_t prevXNew = 0;
    int16_t prevYNew = 0;

    for (int i = 0; i < samplesToDraw; ++i)
    {
        // Process old data (offset=1) - erase segment
        float valOld = buffer.get(1 + i);
        if (valOld != DEVICE_DISCONNECTED_C)
        {
            int16_t x = x0 - i;
            int16_t y = tempToY(valOld);

            if (havePrevOld)
            {
                // Erase old segment in black
                tft.drawLine(prevXOld, prevYOld, x, y, ILI9341_BLACK);
            }

            prevXOld = x;
            prevYOld = y;
            havePrevOld = true;
        }
        else
        {
            havePrevOld = false;
        }

        // Process new data (offset=0) - draw segment
        float valNew = buffer.get(0 + i);
        if (valNew != DEVICE_DISCONNECTED_C)
        {
            int16_t x = x0 - i;
            int16_t y = tempToY(valNew);

            if (havePrevNew)
            {
                // Draw new segment in color
                tft.drawLine(prevXNew, prevYNew, x, y, color);
            }

            prevXNew = x;
            prevYNew = y;
            havePrevNew = true;
        }
        else
        {
            havePrevNew = false;
        }
    }
}

// Update the graph range based on current data
// Returns true if the graph range was updated
bool updateGraphRange()
{
    // If we haven't seen any valid data yet, keep defaults
    if (g_dataMinTemp > g_dataMaxTemp)
    {
        return false;
    }

    // Define valid span sizes with their mark intervals
    // Each span has exactly 10 intervals with nice round marks
    struct SpanConfig {
        float span;           // Total temperature span
        float markInterval;   // Interval between marks
        float alignTo;        // Align min/max to multiples of this value
    };
    
    const SpanConfig spanConfigs[] = {
        { 5.0f,   1.0f, 5.0f },    // 1-degree marks, align to 5s 
        { 10.0f,   2.0f, 10.0f },  // 2-degree marks, align to 10s 
        { 20.0f,   2.0f, 10.0f },  // 2-degree marks, align to 10s
        { 50.0f,   5.0f, 10.0f },  // 5-degree marks, align to 10s
        {100.0f,  10.0f, 10.0f },  // 10-degree marks, align to 10s
        {200.0f,  20.0f, 20.0f },  // 20-degree marks, align to 20s
        {500.0f,  50.0f, 50.0f },  // 50-degree marks, align to 50s
    };
    
    const int numConfigs = sizeof(spanConfigs) / sizeof(spanConfigs[0]);
    
    // Add padding to data range
    const float PADDING_PERCENT = 0.05f; // 5% padding on each side
    float dataSpan = g_dataMaxTemp - g_dataMinTemp;
    float padding = dataSpan * PADDING_PERCENT;
    if (padding < 1.0f) padding = 1.0f; // Minimum 1 degree padding
    
    float requiredMin = g_dataMinTemp - padding;
    float requiredMax = g_dataMaxTemp + padding;
    float requiredSpan = requiredMax - requiredMin;
    
    // Find the smallest span that can contain the data
    // Try each span configuration, attempting to fit the data with alignment
    const SpanConfig* selectedConfig = nullptr;
    float newMin = 0.0f;
    float newMax = 0.0f;
    
    for (int configIndex = 0; configIndex < numConfigs; ++configIndex) {
        const SpanConfig* testConfig = &spanConfigs[configIndex];
        
        // Skip configs that are too small for the required span
        if (testConfig->span < requiredSpan) {
            continue;
        }
        
        // Calculate the center of the required range
        float center = (requiredMin + requiredMax) / 2.0f;
        
        // Calculate initial min/max centered around the data
        float testMin = center - testConfig->span / 2.0f;
        float testMax = center + testConfig->span / 2.0f;
        
        // Align min to nice round number (round down to multiple of alignTo)
        testMin = floorf(testMin / testConfig->alignTo) * testConfig->alignTo;
        
        // Set max to exactly min + span (ensures exactly 10 intervals)
        testMax = testMin + testConfig->span;
        
        // Adjust range to ensure it contains the required data
        // Try shifting by alignTo increments
        int maxShiftAttempts = 20; // Prevent infinite loop
        int shiftCount = 0;
        bool dataFits = false;
        
        while (shiftCount < maxShiftAttempts) {
            // Check if data fits in current range
            if (requiredMin >= testMin && requiredMax <= testMax) {
                dataFits = true;
                break;
            }
            
            // Determine which direction to shift
            if (requiredMin < testMin) {
                // Data extends below range - shift down
                testMin -= testConfig->alignTo;
                testMax -= testConfig->alignTo;
            } else if (requiredMax > testMax) {
                // Data extends above range - shift up
                testMin += testConfig->alignTo;
                testMax += testConfig->alignTo;
            }
            
            shiftCount++;
        }
        
        // If data fits with this config, use it
        if (dataFits) {
            selectedConfig = testConfig;
            newMin = testMin;
            newMax = testMax;
            break;
        }
        
        // Otherwise, try next larger span
    }
    
    // If no config worked (shouldn't happen), use the largest one
    if (selectedConfig == nullptr) {
        selectedConfig = &spanConfigs[numConfigs - 1];
        float center = (requiredMin + requiredMax) / 2.0f;
        newMin = center - selectedConfig->span / 2.0f;
        newMax = center + selectedConfig->span / 2.0f;
        newMin = floorf(newMin / selectedConfig->alignTo) * selectedConfig->alignTo;
        newMax = newMin + selectedConfig->span;
    }
    
    // Clamp to DS18B20 physical limits
    // Disabled for now - clamping messes up ranges and axis marks and makes readjustment too complex.
    // if (newMin < DS18B20_MIN_TEMP)
    //     newMin = DS18B20_MIN_TEMP;
    // if (newMax > DS18B20_MAX_TEMP)
    //     newMax = DS18B20_MAX_TEMP;

    bool result = false;

    if (newMin != g_graphMinTemp || newMax != g_graphMaxTemp)
    {
        result = true;
    }

    g_graphMinTemp = newMin;
    g_graphMaxTemp = newMax;

    return result;
}

void clearScreen()
{
    tft.fillScreen(ILI9341_BLACK);
}

void clearGraphArea()
{
    // Clear the graph area
    tft.fillRect(GRAPH_LEFT_MARGIN, GRAPH_TOP_MARGIN, tft.width() - GRAPH_RIGHT_MARGIN - GRAPH_LEFT_MARGIN + 1, tft.height() - GRAPH_BOTTOM_MARGIN - GRAPH_TOP_MARGIN + 1, ILI9341_BLACK);
}

// Redraw the entire graph for currently selected channel
void redrawGraph()
{
#ifdef ENABLE_DISPLAY_PROFILING
    unsigned long startTime = millis();
#endif

    clearGraphArea();
    drawGraphAxis(g_graphMinTemp, g_graphMaxTemp);

    if (g_displayedChannel == CHANNEL_ALL)
    {
        // Draw all channels (reverse order to have CH1 on top)
        for (int i = SENSOR_COUNT - 1; i >= 0; --i)
        {
            drawGraph(g_sensorValues[i], g_graphMinTemp, g_graphMaxTemp, g_channelColors[i]);
        }
    }
    else
    {
        // Draw selected channel only
        int channelIndex = static_cast<int>(g_displayedChannel);
        drawGraph(g_sensorValues[channelIndex], g_graphMinTemp, g_graphMaxTemp, g_channelColors[channelIndex]);
    }

#ifdef ENABLE_DISPLAY_PROFILING
    unsigned long elapsedTime = millis() - startTime;
    Serial.print("[PROFILE] redrawGraph (total): ");
    Serial.print(elapsedTime);
    Serial.println(" ms");
#endif
}

void refreshUI()
{
    redrawGraph();

    float tempsC[SENSOR_COUNT];
    for (int i = 0; i < SENSOR_COUNT; ++i)
    {
        tempsC[i] = g_sensorValues[i].getLatest();
    }

    drawStatusLine(tempsC, SENSOR_COUNT);
}
