
// board: XIAO_ESP32S3_PLUS
// tool settings:
// - USB CDC on Boot: Enabled
// - PSRAM : OPI PSRAM
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
#include "credentials.h"
#include "picture.h"
#include "weather.h"

EPaper epaper;

void setup() {
    Serial.begin(115200);
    syncTime();

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

    // Forecast section
    epaper.drawLine(gap, ih + 2 * gap, x - gap, ih + 2 * gap, TFT_BLACK);
    epaper.drawString("Forecast", x / 2, ih + 2 * gap + 20);

    // Calendar section
    int y = epaper.height() / 2;
    epaper.drawLine(x + gap, y, epaper.width() - 2 * gap, y, TFT_BLACK);
    drawCalendar(epaper);

    // Weather section
    epaper.drawString("Weather", epaper.width() - x / 2, y / 2);

    epaper.update();
    epaper.sleep();
}

void loop() {
    // put your main code here, to run repeatedly:
}