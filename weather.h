#pragma once

#include "TFT_eSPI.h"
#include "constants.h"
#include "credentials.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <string.h>
#include <time.h>

// External reference to the global EPaper instance
extern EPaper epaper;

// ============================================================
// Data structures
// ============================================================

struct WeatherData {
    bool valid;
    char icon[4];
    char description[64];
    char fetchTime[32];
    float temp;
    int humidity;
    float rain;
    float windSpeed;
};

struct ForecastEntry {
    bool valid;
    char timeStr[6];
    char icon[4];
    float tempHigh;
    float tempLow;
};

static WeatherData weatherData = {false, "", "", "", 0.0f, 0, 0.0f, 0.0f};
static ForecastEntry forecastData[FORECAST_COUNT] = {};

// ============================================================
// Helper
// ============================================================
const char *getWeatherLabel(const char *icon) {
    for (int i = 0; i < NUM_CONDITIONS; i++) {
        if (icon[0] == WEATHER_CONDITIONS[i].iconPrefix[0] &&
            icon[1] == WEATHER_CONDITIONS[i].iconPrefix[1]) {
            return WEATHER_CONDITIONS[i].label;
        }
    }
    return "Unknown";
}

// ============================================================
// Fetch current weather
// ============================================================
bool fetchWeatherData() {
    WiFiClientSecure client;
    client.setInsecure();

    char encodedLocation[128];
    const char *src = WX_LOCATION;
    char *dst = encodedLocation;
    while (*src && (dst - encodedLocation) < (int)sizeof(encodedLocation) - 3) {
        if (*src == ' ') {
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '0';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric",
             encodedLocation, OPENWEATHER_API_KEY);
    http.begin(client, url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        Serial.printf("Weather API returned %d\n", code);
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("JSON parse error: %s\n", err.c_str());
        return false;
    }

    const char *icon = doc["weather"][0]["icon"];
    const char *desc = doc["weather"][0]["description"];
    float temp = doc["main"]["temp"];
    int humidity = doc["main"]["humidity"];
    float rain = 0.0f;
    if (doc["rain"]["1h"].is<float>())
        rain = doc["rain"]["1h"];
    else if (doc["rain"]["3h"].is<float>())
        rain = doc["rain"]["3h"];

    if (icon)
        strncpy(weatherData.icon, icon, sizeof(weatherData.icon) - 1);
    if (desc)
        strncpy(weatherData.description, desc, sizeof(weatherData.description) - 1);

    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    strftime(weatherData.fetchTime, sizeof(weatherData.fetchTime), "%Y-%m-%d @ %H:%M", tm_info);

    weatherData.temp = temp;
    weatherData.humidity = humidity;
    weatherData.rain = rain;
    weatherData.windSpeed = doc["wind"]["speed"].is<float>() ? doc["wind"]["speed"].as<float>() : 0.0f;
    weatherData.valid = true;

    Serial.printf("Weather: %s, %s, %.1f°C, %d%%, %.1fmm, %.1fm/s wind\n",
                  weatherData.icon, weatherData.description,
                  weatherData.temp, weatherData.humidity, weatherData.rain,
                  weatherData.windSpeed);
    return true;
}

// ============================================================
// Fetch 5-period 3-hourly forecast
// ============================================================
bool fetchForecastData() {
    WiFiClientSecure client;
    client.setInsecure();

    char encodedLocation[128];
    const char *src = WX_LOCATION;
    char *dst = encodedLocation;
    while (*src && (dst - encodedLocation) < (int)sizeof(encodedLocation) - 3) {
        if (*src == ' ') {
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '0';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.openweathermap.org/data/2.5/forecast?q=%s&appid=%s&units=metric&cnt=5",
             encodedLocation, OPENWEATHER_API_KEY);
    http.begin(client, url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        Serial.printf("Forecast API returned %d\n", code);
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("Forecast JSON error: %s\n", err.c_str());
        return false;
    }

    JsonArray list = doc["list"];
    for (int i = 0; i < FORECAST_COUNT && i < list.size(); i++) {
        const char *icon = list[i]["weather"][0]["icon"];
        const char *dt_txt = list[i]["dt_txt"];
        float high = list[i]["main"]["temp_max"];
        float low = list[i]["main"]["temp_min"];
        if (icon)
            strncpy(forecastData[i].icon, icon, sizeof(forecastData[i].icon) - 1);
        if (dt_txt && strlen(dt_txt) >= 16) {
            strncpy(forecastData[i].timeStr, dt_txt + 11, 5);
            forecastData[i].timeStr[5] = '\0';
        }
        forecastData[i].tempHigh = high;
        forecastData[i].tempLow = low;
        forecastData[i].valid = true;
        Serial.printf("Forecast[%d]: %s %s %.0f/%.0f°C\n",
                      i, forecastData[i].timeStr, forecastData[i].icon,
                      forecastData[i].tempHigh, forecastData[i].tempLow);
    }
    return true;
}

// ============================================================
// Weather icon drawing primitives
// ============================================================

void addcloud(int x, int y, int scale, int linesize) {
    epaper.fillCircle(x - scale * 3, y, scale, TFT_BLACK);
    epaper.fillCircle(x + scale * 3, y, scale, TFT_BLACK);
    epaper.fillCircle(x - scale, y - scale, scale * 1.4, TFT_BLACK);
    epaper.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, TFT_BLACK);
    epaper.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, TFT_BLACK);
    epaper.fillCircle(x - scale * 3, y, scale - linesize, TFT_WHITE);
    epaper.fillCircle(x + scale * 3, y, scale - linesize, TFT_WHITE);
    epaper.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, TFT_WHITE);
    epaper.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, TFT_WHITE);
    epaper.fillRect(x - scale * 3 + 2, y - scale + linesize - 1,
                    scale * 5.9, scale * 2 - linesize * 2 + 2, TFT_WHITE);
}

void addsun(int x, int y, int scale, bool isLarge) {
    int linesize = 5;
    epaper.fillRect(x - scale * 2, y, scale * 4, linesize, TFT_BLACK);
    epaper.fillRect(x, y - scale * 2, linesize, scale * 4, TFT_BLACK);
    epaper.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, TFT_BLACK);
    epaper.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, TFT_BLACK);
    if (isLarge) {
        epaper.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, TFT_BLACK);
        epaper.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, TFT_BLACK);
        epaper.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, TFT_BLACK);
        epaper.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, TFT_BLACK);
        epaper.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, TFT_BLACK);
        epaper.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, TFT_BLACK);
    }
    epaper.fillCircle(x, y, scale * 1.3, TFT_WHITE);
    epaper.fillCircle(x, y, scale, TFT_BLACK);
    epaper.fillCircle(x, y, scale - linesize, TFT_WHITE);
}

void addmoon(int x, int y, int scale, bool isLarge) {
    if (isLarge) {
        epaper.fillCircle(x - 85, y - 100, uint16_t(scale * 0.8), TFT_BLACK);
        epaper.fillCircle(x - 57, y - 100, uint16_t(scale * 1.6), TFT_WHITE);
    } else {
        epaper.fillCircle(x - 28, y - 37, uint16_t(scale * 1.0), TFT_BLACK);
        epaper.fillCircle(x - 20, y - 37, uint16_t(scale * 1.6), TFT_WHITE);
    }
}

void addrain(int x, int y, int scale, bool isLarge) {
    int count = isLarge ? 7 : 5;
    int spread = scale * 2;
    int rainLen = scale;
    int yOff = scale + (isLarge ? 7 : 4); // below cloud center
    for (int i = 0; i < count; i++) {
        int sx = x - spread + i * (spread * 2 / (count - 1));
        int sy = y + yOff;
        epaper.drawLine(sx, sy, sx + rainLen / 2, sy + rainLen, TFT_BLACK);
        if (isLarge) {
            epaper.drawLine(sx + 1, sy, sx + rainLen / 2 + 1, sy + rainLen, TFT_BLACK);
        }
    }
}

void addsnow(int x, int y, int scale, bool isLarge) {
    int spacingY = scale + 4, spacingX = scale + 3;
    int w = scale * 3;
    int radius = isLarge ? 3 : 1;
    if (!isLarge) {
        spacingY = scale / 2 + 3;
        spacingX = scale / 2 + 2;
    }
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int sx = x - w + col * spacingX;
            int sy = y + row * spacingY;
            epaper.fillCircle(sx, sy, radius, TFT_BLACK);
        }
    }
}

void addtstorm(int x, int y, int scale, bool isLarge) {
    y = y + scale / 2;
    for (int i = 0; i < 5; i++) {
        epaper.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5,
                        x - scale * 3.5 + scale * i * 1.5, y + scale, TFT_BLACK);
        if (isLarge) {
            epaper.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5,
                            x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, TFT_BLACK);
            epaper.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5,
                            x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, TFT_BLACK);
        }
        epaper.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5,
                        x - scale * 3 + scale * i * 1.5, y + scale * 1.5, TFT_BLACK);
        if (isLarge) {
            epaper.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1,
                            x - scale * 3 + scale * i * 1.5, y + scale * 1.5 + 1, TFT_BLACK);
            epaper.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2,
                            x - scale * 3 + scale * i * 1.5, y + scale * 1.5 + 2, TFT_BLACK);
        }
        epaper.drawLine(x - scale * 3.5 + scale * i * 1.4, y + scale * 2.5,
                        x - scale * 3 + scale * i * 1.5, y + scale * 1.5, TFT_BLACK);
        if (isLarge) {
            epaper.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5,
                            x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, TFT_BLACK);
            epaper.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5,
                            x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, TFT_BLACK);
        }
    }
}

void addfog(int x, int y, int scale, int linesize, bool isLarge) {
    if (!isLarge) {
        y -= 10;
        linesize = 1;
    }
    for (int i = 0; i < 6; i++) {
        epaper.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, TFT_BLACK);
        epaper.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, TFT_BLACK);
        epaper.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, TFT_BLACK);
    }
}

// ============================================================
// Composite icon drawing functions
// ============================================================

void drawSunny(int x, int y, int scale, bool isNight, bool isLarge) {
    if (isNight)
        addmoon(x, y + 15, scale, isLarge);
    addsun(x, y, scale * 1.6, isLarge);
}

void drawPartlyCloudy(int x, int y, int scale, bool isNight, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    if (isNight)
        addmoon(x, y + 15, scale, isLarge);
    addsun(x - scale * 1.8, y - scale * 1.8, scale, isLarge);
    addcloud(x, y, scale, linesize);
}

void drawCloudy(int x, int y, int scale, bool isNight, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    if (isNight)
        addmoon(x, y + 15, scale, isLarge);
    addcloud(x + 15, y - 22, scale / 2, linesize);
    addcloud(x - 10, y - 18, scale / 2, linesize);
    addcloud(x, y, scale, linesize);
}

void drawOvercast(int x, int y, int scale, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    addcloud(x - scale / 2, y - scale / 2, scale * 2 / 3, linesize);
    addcloud(x + scale / 2, y + scale / 2, scale * 2 / 3, linesize);
    addcloud(x, y, scale, linesize);
}

void drawShowers(int x, int y, int scale, bool isNight, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    if (isNight)
        addmoon(x, y + 15, scale, isLarge);
    addsun(x - scale * 1.8, y - scale * 1.8, scale, isLarge);
    addcloud(x, y, scale, linesize);
    addrain(x, y, scale, isLarge);
}

void drawRainIcon(int x, int y, int scale, bool isNight, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    if (isNight)
        addmoon(x, y + 15, scale, isLarge);
    addcloud(x, y, scale, linesize);
    addrain(x, y, scale, isLarge);
}

void drawThunderstorm(int x, int y, int scale, bool isNight, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    if (isNight)
        addmoon(x, y + 15, scale, isLarge);
    addcloud(x, y, scale, linesize);
    addtstorm(x, y, scale, isLarge);
}

void drawSnowIcon(int x, int y, int scale, bool isNight, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    if (isNight)
        addmoon(x, y + 15, scale, isLarge);
    addcloud(x, y, scale, linesize);
    addsnow(x, y, scale, isLarge);
}

void drawHaze(int x, int y, int scale, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    addsun(x, y - 5, scale * 1.4, isLarge);
    addfog(x, y - 5, scale * 1.4, linesize, isLarge);
}

void drawFogIcon(int x, int y, int scale, bool isNight, bool isLarge) {
    int linesize = isLarge ? 5 : 2;
    if (isNight)
        addmoon(x, y + 15, scale, isLarge);
    addcloud(x, y - 5, scale, linesize);
    addfog(x, y - 5, scale, linesize, isLarge);
}

// ============================================================
// Unified icon dispatcher
// ============================================================
void drawOneIcon(int x, int y, int scale, bool isLarge, const char *icon) {
    bool isNight = (icon[2] == 'n');
    char prefix[3] = {icon[0], icon[1], '\0'};
    int code = atoi(prefix);

    switch (code) {
    case 1:
        drawSunny(x, y, scale, isNight, isLarge);
        break;
    case 2:
        drawPartlyCloudy(x, y, scale, isNight, isLarge);
        break;
    case 3:
        drawCloudy(x, y, scale, isNight, isLarge);
        break;
    case 4:
        drawOvercast(x, y, scale, isLarge);
        break;
    case 9:
        drawShowers(x, y, scale, isNight, isLarge);
        break;
    case 10:
        drawRainIcon(x, y, scale, isNight, isLarge);
        break;
    case 11:
        drawThunderstorm(x, y, scale, isNight, isLarge);
        break;
    case 13:
        drawSnowIcon(x, y, scale, isNight, isLarge);
        break;
    case 50:
        if (icon[2] == 'd')
            drawHaze(x, y, scale, isLarge);
        else
            drawFogIcon(x, y, scale, isNight, isLarge);
        break;
    default:
        epaper.setFreeFont(isLarge ? &FreeSansBold24pt7b : &FreeSans9pt7b);
        epaper.setTextColor(TFT_BLACK);
        int w = epaper.textWidth("?");
        epaper.drawString("?", x - w / 2, y - (isLarge ? 30 : 12));
        break;
    }
}

// ============================================================
// Main weather icon
// ============================================================
void drawWeatherIcon(const char *icon) {
    int scale = WX_ICON_SIZE / 8;
    int cx = WX_CENTER_X;
    int cy = WX_ICON_Y + WX_ICON_SIZE / 2;
    epaper.fillRect(WX_CENTER_X - WX_ICON_SIZE / 2, WX_ICON_Y,
                    WX_ICON_SIZE, WX_ICON_SIZE, TFT_WHITE);
    drawOneIcon(cx, cy, scale, true, icon);
}

// ============================================================
// 3-hourly forecast section
// ============================================================
void drawForecastSection(EPaper &epaperRef) {
    int cellW = WX_AREA_W / FORECAST_COUNT;
    int cellCenter = cellW / 2;

    for (int i = 0; i < FORECAST_COUNT; i++) {
        if (!forecastData[i].valid)
            continue;

        int cellX = WX_AREA_X + i * cellW;
        int cx = cellX + cellCenter;

        // Time
        epaperRef.setFreeFont(&FreeSans9pt7b);
        epaperRef.setTextColor(TFT_BLACK);
        int tw = epaperRef.textWidth(forecastData[i].timeStr);
        epaperRef.drawString(forecastData[i].timeStr, cx - tw / 2, FCST_Y);

        // Small icon
        int iconAreaSize = 44;
        int iconX = cx;
        int iconY = FCST_ICON_Y + 18;
        epaperRef.fillRect(cx - iconAreaSize / 2, iconY - iconAreaSize / 2,
                           iconAreaSize, iconAreaSize, TFT_WHITE);
        drawOneIcon(iconX, iconY, ICON_SMALL, false, forecastData[i].icon);

        // Temperature (with drawn degree circle since font lacks °)
        char tempHi[8], tempLo[8];
        snprintf(tempHi, sizeof(tempHi), "%.0f", forecastData[i].tempHigh);
        snprintf(tempLo, sizeof(tempLo), "%.0f", forecastData[i].tempLow);
        char tempText[16];
        snprintf(tempText, sizeof(tempText), "%s/%s", tempHi, tempLo);

        epaperRef.setFreeFont(&FreeSans9pt7b);
        epaperRef.setTextColor(TFT_BLACK);
        int tempTW = epaperRef.textWidth(tempText);
        int tempX = cx - tempTW / 2;
        int tempY = FCST_ICON_Y + 45;
        int tempH = epaperRef.fontHeight();

        epaperRef.drawString(tempText, tempX, tempY);
        // Small degree circle after the text
        int degX = tempX + tempTW + 5;
        int degY = tempY + 4;
        epaperRef.drawCircle(degX, degY, 3, TFT_BLACK);
    }
}

// ============================================================
// Draw complete weather section
// ============================================================
void drawWeather(EPaper &epaperRef) {
    struct tm t;
    if (!getLocalTime(&t))
        return;

    // Day name
    const char *dayName = FULL_DAY_NAMES[t.tm_wday];
    epaperRef.setFreeFont(&FreeSansBold24pt7b);
    epaperRef.setTextColor(TFT_BLACK);
    int tw = epaperRef.textWidth(dayName);
    int cx = WX_AREA_X + (WX_AREA_W - tw) / 2;
    epaperRef.drawString(dayName, cx, WX_DAY_Y);

    // Timestamp
    epaperRef.setFreeFont(&FreeSans9pt7b);
    epaperRef.setTextColor(TFT_BLACK);
    tw = epaperRef.textWidth(weatherData.fetchTime);
    cx = WX_AREA_X + (WX_AREA_W - tw) / 2;
    epaperRef.drawString(weatherData.fetchTime, cx, WX_DAY_Y + 50);

    // Location
    char cityName[64];
    strncpy(cityName, WX_LOCATION, sizeof(cityName) - 1);
    cityName[sizeof(cityName) - 1] = '\0';
    char *comma = strchr(cityName, ',');
    if (comma)
        *comma = '\0';
    epaperRef.setFreeFont(&FreeSans9pt7b);
    epaperRef.setTextColor(TFT_BLACK);
    tw = epaperRef.textWidth(cityName);
    cx = WX_AREA_X + (WX_AREA_W - tw) / 2;
    epaperRef.drawString(cityName, cx, WX_DAY_Y + 70);

    if (!weatherData.valid) {
        epaperRef.setFreeFont(&FreeSans18pt7b);
        epaperRef.drawString("No weather", WX_AREA_X + 60, WX_ICON_Y + 60);
        return;
    }

    // Weather icon
    drawWeatherIcon(weatherData.icon);

    // Description
    const char *label = weatherData.description;
    epaperRef.setFreeFont(&FreeSans18pt7b);
    epaperRef.setTextColor(TFT_BLACK);
    tw = epaperRef.textWidth(label);
    cx = WX_AREA_X + (WX_AREA_W - tw) / 2;
    epaperRef.drawString(label, cx, WX_DESC_Y);

    // Temperature line
    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%.1f", weatherData.temp);
    char restStr[32];
    snprintf(restStr, sizeof(restStr), "C  %d%%  %.1fmm",
             weatherData.humidity, weatherData.rain);

    epaperRef.setFreeFont(&FreeSans18pt7b);
    epaperRef.setTextColor(TFT_BLACK);
    int tempW = epaperRef.textWidth(tempStr);
    int restW = epaperRef.textWidth(restStr);
    int degGap = 12;
    int totalW = tempW + degGap + restW;
    int startX = WX_AREA_X + (WX_AREA_W - totalW) / 2;

    epaperRef.drawString(tempStr, startX, WX_DATA_Y);
    epaperRef.fillCircle(startX + tempW + 5, WX_DATA_Y + 5, 4, TFT_BLACK);
    epaperRef.fillCircle(startX + tempW + 5, WX_DATA_Y + 5, 3, TFT_WHITE);
    epaperRef.drawString(restStr, startX + tempW + degGap, WX_DATA_Y);

    // Forecast
    drawForecastSection(epaperRef);
}

// ============================================================
// Connect, fetch, disconnect
// ============================================================
bool updateWeather() {
    WiFi.begin(SSID, PASSWORD);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 100) {
        delay(100);
        retry++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Weather: WiFi connect failed");
        return false;
    }
    Serial.println("Weather: WiFi connected, fetching data...");
    bool ok = fetchWeatherData();
    if (ok) {
        Serial.println("Fetching forecast data...");
        fetchForecastData();
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return ok;
}