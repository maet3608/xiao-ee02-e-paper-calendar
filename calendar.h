#pragma once

#define BOARD_SCREEN_COMBO 510
#define USE_XIAO_EPAPER_DISPLAY_BOARD_EE02
#include "TFT_eSPI.h"
#include "constants.h"
#include "credentials.h"
#include <WiFi.h>
#include <time.h>

// ============================================================
// Calendar utility functions
// ============================================================

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
    int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year))
        return 29;
    return days[month - 1];
}

// Returns day of week: 0=Monday, 1=Tuesday, ..., 6=Sunday
int dayOfWeek(int year, int month, int day) {
    tm timeinfo = {};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    mktime(&timeinfo);
    return (timeinfo.tm_wday + 6) % 7; // convert Sunday=0 to Monday=0
}

// Synchronize system time with an NTP server using configured timezone
void syncTime() {
    WiFi.begin(SSID, PASSWORD);
    for (int i = 0; i < 100 && WiFi.status() != WL_CONNECTED; i++) {
        delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
        configTzTime(TIMEZONE, NTP_SERVER);
        struct tm t;
        int retry = 0;
        while (!getLocalTime(&t) && retry < 20) {
            delay(500);
            retry++;
        }
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

// Draw the monthly calendar on the e-paper display
void drawCalendar(EPaper &epaper) {
    struct tm t;
    if (!getLocalTime(&t))
        return;

    int year = t.tm_year + 1900;
    int month = t.tm_mon + 1;
    int today = t.tm_mday;

    char buf[32];

    // --- Month & Year header (white text on black background) ---
    epaper.fillRect(CAL_HEADER_X, CAL_HEADER_Y, CAL_HEADER_W, CAL_HEADER_H, TFT_BLACK);
    epaper.setTextColor(TFT_WHITE);
    epaper.setFreeFont(&FreeSans18pt7b);
    sprintf(buf, "%s %d", MONTH_NAMES[month - 1], year);
    int tw = epaper.textWidth(buf);
    int th = epaper.fontHeight();
    epaper.drawString(buf, CAL_HEADER_X + (CAL_HEADER_W - tw) / 2,
                      CAL_HEADER_Y + (CAL_HEADER_H - th + 10) / 2);

    // --- Weekday headers ---
    epaper.setFreeFont(&FreeSansBold9pt7b);
    int dayLabelY = CAL_HEADER_Y + CAL_HEADER_H + 24;
    for (int i = 0; i < CAL_COLS; i++) {
        int cx = CAL_GRID_X + i * CAL_CELL_W;
        int dw = epaper.textWidth(DAY_NAMES[i]);
        epaper.setTextColor((i == 5 || i == 6) ? TFT_RED : TFT_BLACK);
        epaper.drawString(DAY_NAMES[i], cx + (CAL_CELL_W - dw) / 2, dayLabelY);
    }

    // --- Day grid ---
    int days = daysInMonth(year, month);
    int firstDay = dayOfWeek(year, month, 1); // 0=Monday

    int x = CAL_GRID_X + firstDay * CAL_CELL_W;
    int y = CAL_GRID_Y;

    epaper.setFreeFont(&FreeSans12pt7b);
    int dayFontH = epaper.fontHeight();
    for (int d = 1; d <= days; d++) {
        sprintf(buf, "%d", d);
        int dw = epaper.textWidth(buf);
        int cx = x + (CAL_CELL_W - dw) / 2;
        int cy = y + (CAL_CELL_H - dayFontH + 5) / 2;

        if (d == today) {
            epaper.fillRect(x + 2, y, CAL_CELL_W - 4, CAL_CELL_H, TFT_BLACK);
            epaper.setTextColor(TFT_WHITE);
        } else {
            int wday = (firstDay + d - 1) % 7;
            epaper.setTextColor((wday == 5 || wday == 6) ? TFT_RED : TFT_BLACK);
        }
        epaper.drawString(buf, cx, cy);

        x += CAL_CELL_W;
        if ((firstDay + d) % 7 == 0) {
            y += CAL_CELL_H + 4;
            x = CAL_GRID_X;
        }
    }

    epaper.setTextColor(TFT_BLACK);
}