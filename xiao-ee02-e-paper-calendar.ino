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
// purpose:
// - picture frame with e-paper display 1600 x 1200
// - shows AI generated japanese anime (totoro) pictures in the upper left
// - shows day and current weather in the upper right
// - shows monthly calendar in the lower right
// - shows 5 days weather forecast at the left bottom

#include "TFT_eSPI.h"
#include "constants.h"
#include "credentials.h"
#include "calendar.h"
#include "picture.h"
#include "weather.h"

EPaper epaper;

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
    Serial.println("Weather data not available, skipping image generation.");
  }

  epaper.begin();
  epaper.setRotation(1);
  epaper.fillScreen(TFT_WHITE);

  drawPicture(epaper);
  epaper.drawLine(PIC_RIGHT_X, PIC_GAP, PIC_RIGHT_X,
                  epaper.height() - 2 * PIC_GAP, TFT_BLACK);

  epaper.drawLine(PIC_GAP, PIC_H + 2 * PIC_GAP, PIC_RIGHT_X - PIC_GAP,
                  PIC_H + 2 * PIC_GAP, TFT_BLACK);
  drawDailyForecastSection(epaper);

  int y = epaper.height() / 2;
  epaper.drawLine(PIC_RIGHT_X + PIC_GAP, y, epaper.width() - 2 * PIC_GAP, y,
                  TFT_BLACK);
  drawCalendar(epaper);

  drawWeather(epaper);

  epaper.update();
  epaper.sleep();

  if (DEEP_SLEEP_HOURS > 0) {
    esp_sleep_enable_timer_wakeup((uint64_t)DEEP_SLEEP_HOURS * 60 * 60 *
                                  1000000);
    Serial.printf("Deep sleep for %d hours...\n", DEEP_SLEEP_HOURS);
    Serial.flush();
    esp_deep_sleep_start();
  } else {
    Serial.println("Deep sleep disabled (DEEP_SLEEP_HOURS=0)");
  }
}

void loop() {}