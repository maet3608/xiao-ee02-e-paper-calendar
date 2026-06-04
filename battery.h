#pragma once

#include "TFT_eSPI.h"
#include "constants.h"

// ============================================================
// Battery voltage measurement pins
// ============================================================
#define VOLTAGE_PIN A0    // GPIO1  — ADC input from voltage divider
#define ADC_ENABLE_PIN A5 // GPIO6  — pull HIGH to enable measurement

// ============================================================
// LiPo discharge curve: voltage (mV) → charge percentage
// Typical 3.7V LiPo; linear interpolation is used between points.
// ============================================================
struct VoltagePoint {
  int mv;
  int pct;
};

static const VoltagePoint DISCHARGE_CURVE[] = {
    {4200, 100}, {4030, 90}, {3950, 80}, {3870, 70}, {3800, 60}, {3740, 50},
    {3680, 40},  {3610, 30}, {3520, 20}, {3430, 10}, {3300, 0},
};
static const int CURVE_POINTS =
    sizeof(DISCHARGE_CURVE) / sizeof(DISCHARGE_CURVE[0]);

// Map a measured millivolt value to a charge percentage (0–100)
int voltageToPercent(int mv) {
  if (mv >= DISCHARGE_CURVE[0].mv)
    return 100;
  if (mv <= DISCHARGE_CURVE[CURVE_POINTS - 1].mv)
    return 0;

  for (int i = 0; i < CURVE_POINTS - 1; i++) {
    int vHi = DISCHARGE_CURVE[i].mv;
    int vLo = DISCHARGE_CURVE[i + 1].mv;
    if (mv <= vHi && mv >= vLo) {
      int pctHi = DISCHARGE_CURVE[i].pct;
      int pctLo = DISCHARGE_CURVE[i + 1].pct;
      float t = (float)(vHi - mv) / (float)(vHi - vLo);
      return pctHi - (int)(t * (pctHi - pctLo) + 0.5f);
    }
  }
  return 0;
}

// Read battery voltage in millivolts.
// Returns 0 if the measurement cannot be taken.
int readBatteryMillivolts() {
  pinMode(ADC_ENABLE_PIN, OUTPUT);
  digitalWrite(ADC_ENABLE_PIN, HIGH); // enable voltage divider
  delay(5);                           // let the signal settle

  int raw = analogRead(VOLTAGE_PIN);

  digitalWrite(ADC_ENABLE_PIN, LOW); // disable to save power

  // XIAO ESP32S3 Plus: 2:1 divider (R1=R2=2MΩ) → VBAT = V_ADC * 2
  // ADC reference = 3.3 V, 12-bit ⇒ 0–4095 → 0–3300 mV
  float vAdc = (float)raw * 3300.0f / 4095.0f;
  int vBat = (int)(vAdc * 2.0f + 0.5f);
  return vBat;
}

// ============================================================
// Draw battery charge indicator in the lower right corner
// ============================================================
void drawBattery(EPaper &epaperRef) {
  int pct = voltageToPercent(readBatteryMillivolts());

  // Battery icon geometry defined in constants.h
  int termW = 4;  // terminal bump width
  int termH = 12; // terminal bump height
  int radius = 5; // corner radius for rounded rect

  // Terminal bump (right side)
  int termX = BAT_ICON_X + BAT_ICON_W;
  int termY = BAT_ICON_Y + (BAT_ICON_H - termH) / 2;
  epaperRef.fillRect(termX, termY, termW, termH, TFT_BLACK);

  // Battery body outline (rounded rectangle)
  epaperRef.drawRoundRect(BAT_ICON_X, BAT_ICON_Y, BAT_ICON_W, BAT_ICON_H,
                          radius, TFT_BLACK);

  // Fill level inside the body
  int margin = 3;
  int fillW = BAT_ICON_W - 2 * margin;
  int fillH = BAT_ICON_H - 2 * margin;
  int fillActualW = (fillW * pct) / 100;
  int fillX = BAT_ICON_X + margin;
  int fillY = BAT_ICON_Y + margin;

  if (fillActualW > 0) {
    epaperRef.fillRect(fillX, fillY, fillActualW, fillH, TFT_BLACK);
  }

  // Percentage label (centered over the battery body)
  epaperRef.setFreeFont(&FreeSans9pt7b);
  epaperRef.setTextColor(TFT_BLACK);
  char pctText[6];
  snprintf(pctText, sizeof(pctText), "%d%%", pct);
  int tw = epaperRef.textWidth(pctText);
  int th = epaperRef.fontHeight();

  // Position text to the left of the battery icon
  int textX = BAT_ICON_X - tw - BAT_TEXT_GAP;
  int textY = BAT_ICON_Y + (BAT_ICON_H - th) / 2 + 3;

  // Clear the text background area first
  epaperRef.fillRect(textX - 2, BAT_ICON_Y, tw + 4, BAT_ICON_H, TFT_WHITE);
  epaperRef.drawString(pctText, textX, textY);

  Serial.printf("Battery: %dmV — %d%%\n", readBatteryMillivolts(), pct);
}
