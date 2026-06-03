
// board: XIAO_ESP32S3_PLUS
// tool settings:
// - USB CDC on Boot: Enabled
// - PSRAM : OPI PSRAM
// - Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
// esp32 core: 4.0.0-alpha1
// libraries:
// - https://github.com/Seeed-Studio/Seeed_GFX/tree/master : 2.0.3
// - Arduinojson: 7.4.3
// purpose:
// - picture frame with e-paper display 1600 x 1200
//   - https://www.seeedstudio.com/XIAO-ePaper-DIY-Kit-EE02-for-13-3-Spectratm-6-E-Ink.html
//   - https://files.seeedstudio.com/wiki/Epaper/EE02/13_3_E6_eInk_Display_module_Datasheet.pdf
//   - Colors: TFT_WHITE, TFT_BLACK, TFT_YELLOW, TFT_GREEN, TFT_BLUE, TFT_RED
// - shows AI generated japanese anime (totoro) pictures in the upper left
// - shows day and current weather in the upper right
// - shows monthly calendar in the lower right
// - shows 5 days weather forecast at the left bottom

#include "TFT_eSPI.h"
#include "calendar.h"
#include "constants.h"
#include "credentials.h"
#include "picture.h"
#include "weather.h"

EPaper epaper;

void setup() {
    Serial.begin(115200);

    syncTime();
    updateWeather(); // fetch current weather via OpenWeather API

    epaper.begin();
    epaper.setRotation(1);
    epaper.fillScreen(TFT_WHITE);

    // Picture section
    int gap = 10;
    int iw = 1024; // image width
    int ih = 1024; // image height
    int x = iw + 2 * gap;
    epaper.drawLine(x, gap, x, epaper.height() - 2 * gap, TFT_BLACK);
    epaper.drawString("Picture", x / 2, epaper.height() / 2);
    epaper.drawRect(gap, gap, iw, ih, TFT_RED); // picture placeholder

    // Something section
    epaper.drawLine(gap, ih + 2 * gap, x - gap, ih + 2 * gap, TFT_BLACK);
    epaper.drawString("Something", x / 2, ih + 2 * gap + 20);

    // Calendar section
    int y = epaper.height() / 2;
    epaper.drawLine(x + gap, y, epaper.width() - 2 * gap, y, TFT_BLACK);
    drawCalendar(epaper);

    // Weather section
    drawWeather(epaper);

    epaper.update();
    epaper.sleep();

    // Deep sleep for 3 hours (in microseconds)
    // 3 hours = 3 * 60 * 60 * 1000000 = 10800000000
    // esp_sleep_enable_timer_wakeup(3ULL * 60 * 60 * 1000000);

    // Serial.println("Deep sleep for 3 hours...");
    // Serial.flush();
    // esp_deep_sleep_start();
}

void loop() {
    // unreachable due to deep sleep
}
