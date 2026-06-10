// board: XIAO_ESP32S3_PLUS
// tool settings:
// - USB CDC on Boot: Enabled
// - PSRAM : OPI PSRAM
// - Partition Scheme: Default with spiffs (3MB APP/1.5MB SPIFFS)
// esp32 core: 4.0.0-alpha1
// libraries:
// - https://github.com/Seeed-Studio/Seeed_GFX/tree/master : 2.0.3
// - ArduinoJson: 7.4.3
// - PNGdec: 1.1.6

#include "TFT_eSPI.h"
#include "constants.h"
#include "credentials.h"
#include "battery.h"
#include "calendar.h"
#include "picture.h"
#include "weather.h"

EPaper epaper;

// ============================================================
// Calculate deep sleep duration taking the dead zone into account
// (00:00-04:59) to avoid waking the device when nobody is watching.
// Returns microseconds to sleep, or 0 if deep sleep is disabled.
// ============================================================
uint64_t getDeepSleepDuration() {
  if (DEEP_SLEEP_HOURS <= 0) {
    return 0;
  }

  struct tm t;
  if (!getLocalTime(&t)) {
    // RTC not synced yet — fall back to normal interval
    return (uint64_t)DEEP_SLEEP_HOURS * 3600 * 1000000ULL;
  }

  int secSinceMidnight = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;

  // Seconds until 05:00 AM today (wrapping around if past 05:00)
  int secTo5am = (5 * 3600 - secSinceMidnight + 86400) % 86400;
  if (secTo5am == 0) {
    secTo5am = 86400; // exactly 05:00 → sleep a full day
  }

  // Already in the dead zone (00:00-04:59)?
  if (t.tm_hour >= 0 && t.tm_hour < 5) {
    return (uint64_t)secTo5am * 1000000ULL;
  }

  // Would the normal wake time land in the dead zone?
  int wakeSec = (secSinceMidnight + DEEP_SLEEP_HOURS * 3600) % 86400;
  if (wakeSec < 5 * 3600) {
    return (uint64_t)secTo5am * 1000000ULL;
  }

  // Normal case: sleep for the configured duration
  return (uint64_t)DEEP_SLEEP_HOURS * 3600 * 1000000ULL;
}

void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  syncTime();
  updateWeather();

  if (weatherData.valid) {
    Serial.println("Checking/creating daily image...");
    fetchAndStoreDailyImage();
  } else {
    Serial.println("Weather data not available!");
  }

  epaper.begin();
  epaper.setRotation(1);
  epaper.fillScreen(TFT_WHITE);

  drawPicture(epaper);
  epaper.drawLine(PIC_RIGHT_X, PIC_GAP, PIC_RIGHT_X,
                  epaper.height() - 2 * PIC_GAP, TFT_BLACK);
  epaper.drawLine(PIC_GAP, PIC_H + 2 * PIC_GAP, PIC_RIGHT_X - PIC_GAP,
                  PIC_H + 2 * PIC_GAP, TFT_BLACK);
  int y = epaper.height() / 2 - 30;
  epaper.drawLine(PIC_RIGHT_X + PIC_GAP, y, epaper.width() - 2 * PIC_GAP, y,
                  TFT_BLACK);

  drawDailyForecastSection(epaper);
  drawCalendar(epaper);
  drawCalendarEvents(epaper);
  drawBattery(epaper);
  drawWeather(epaper);

  epaper.update();
  epaper.sleep();

  uint64_t sleepUs = getDeepSleepDuration();
  if (sleepUs > 0) {
    esp_sleep_enable_timer_wakeup(sleepUs);
    Serial.printf("Deep sleep for %.1f hours...\n",
                  sleepUs / (3600.0 * 1000000.0));
    Serial.flush();
    esp_deep_sleep_start();
  }
}

void loop() {}