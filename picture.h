#pragma once

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PNGdec.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include <string.h>
#include <time.h>
#include "TFT_eSPI.h"
#include "constants.h"
#include "credentials.h"
#include "weather.h"

// ============================================================
// Spectra 6 palette (calibrated to physical display colors)
// ============================================================
struct PaletteColor {
  int r, g, b;
};

static const PaletteColor SPECTRA6[6] = {
    {25, 30, 33},    // 0: Black   → nibble 0xF
    {232, 232, 232}, // 1: White   → nibble 0x0
    {239, 222, 68},  // 2: Yellow  → nibble 0xB
    {178, 19, 24},   // 3: Red     → nibble 0x6
    {33, 87, 186},   // 4: Blue    → nibble 0xD
    {18, 95, 32},    // 5: Green   → nibble 0x2
};

static const uint8_t PALETTE_NIBBLE[6] = {0xF, 0x0, 0xB, 0x6, 0xD, 0x2};

// ============================================================
// Season derivation
// ============================================================
const char *getSeason(int month) {
  if (month >= 3 && month <= 5)
    return SEASON_NAMES[1];
  if (month >= 6 && month <= 8)
    return SEASON_NAMES[2];
  if (month >= 9 && month <= 11)
    return SEASON_NAMES[3];
  return SEASON_NAMES[0];
}

int getSeasonIndex(int month) {
  if (month >= 3 && month <= 5)
    return 1;
  if (month >= 6 && month <= 8)
    return 2;
  if (month >= 9 && month <= 11)
    return 3;
  return 0;
}

// ============================================================
// OpenAI API helpers
// ============================================================

static String gImagePrompt;

// Call OpenAI Chat Completions to generate an image prompt from weather data
bool generateImagePrompt() {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);

  int seasonIdx = 0;
  struct tm t;
  if (getLocalTime(&t)) {
    seasonIdx = getSeasonIndex(t.tm_mon + 1);
  }
  const char *seasonName = SEASON_NAMES[seasonIdx];
  const char *picTheme = PIC_THEMES[random(NUM_PIC_THEMES)];

  // Build wind description for the prompt
  char windDesc[16];
  float ws = weatherData.windSpeed;
  if (ws < 0.5f)
    strcpy(windDesc, "calm wind");
  else if (ws < 5.0f)
    strcpy(windDesc, "light breeze");
  else if (ws < 10.0f)
    strcpy(windDesc, "breezy wind");
  else if (ws < 20.0f)
    strcpy(windDesc, "windy");
  else
    strcpy(windDesc, "stormy wind");

  char weatherDesc[96];
  const char *wlabel =
      weatherData.valid ? weatherData.description : "unknown weather";
  snprintf(weatherDesc, sizeof(weatherDesc), "%s, %.1f°C, %s, %.1fmm rain",
           wlabel, weatherData.temp, windDesc, weatherData.rain);

  String body = "{";
  body += "\"model\":\"gpt-4o-mini\",";
  body += "\"messages\":[";
  body +=
      "{\"role\":\"system\",\"content\":\"You create image generation "
      "prompts for the following theme: " +
      String(picTheme) +
      ". "
      "Generate a short, single-paragraph image prompt (max 200 chars) "
      "that reflects the given weather and season. "
      "The weather conditions (rain, overcast, clear, snow, wind, etc.) must "
      "be the primary driver of the scene's atmosphere and sky. "
      "Season mostly influences background foliage and colors (e.g. cherry "
      "blossoms for spring, maple leaves for autumn, bare trees for winter) "
      "but weather supersedes season. "
      "Enrich the scene with details/accessories/activities that fit the "
      "weather and season if suitable "
      "(e.g. umbrella, raincoat, kite, icecream, snowflakes, puddles, "
      "wind-blown leaves, clouds, sun rays, reflections, etc.). "
      "Keep the overall image composition simple and show only one or two "
      "characters at most."
      "Output only the image prompt, without any explanation or additional "
      "text.\"},";
  "Output only the prompt text, nothing else.\"},";
  body += "{\"role\":\"user\",\"content\":\"Weather: ";
  body += weatherDesc;
  body += "\\nSeason: ";
  body += seasonName;
  body += "\\n\\nCreate an image prompt for a ";
  body += picTheme;
  body += "-related scene.\"}";
  body += "],";
  body += "\"max_tokens\":256,\"temperature\":0.9}";

  Serial.println("Chat prompt body sent to LLM:");
  Serial.println(body);

  HTTPClient http;
  http.begin(client, "https://api.openai.com/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);

  int code = http.POST(body);
  if (code != HTTP_CODE_OK) {
    Serial.printf("OpenAI Chat API returned %d\n", code);
    String errBody = http.getString();
    Serial.printf("Chat API error body: %s\n", errBody.c_str());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("Chat JSON error: %s\n", err.c_str());
    return false;
  }

  const char *prompt = doc["choices"][0]["message"]["content"];
  if (!prompt) {
    Serial.println("No prompt in Chat API response");
    return false;
  }

  gImagePrompt = String(prompt);
  gImagePrompt.trim();
  Serial.printf("Generated prompt: %s\n", gImagePrompt.c_str());
  return true;
}

// Forward declarations (defined later in this file)
String todayImagePath();
bool processAndSaveImage(const uint8_t *pngData, size_t pngLen,
                         const char *path);

// Call OpenAI Image Generation API, download PNG via URL, process & save to
// SPIFFS
bool generateAndStoreImage() {
  String escaped = gImagePrompt;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", "\\n");
  escaped.replace("\r", "");

  String body = "{";
  body += "\"model\":\"gpt-image-1-mini\",";
  body += "\"prompt\":\"";
  body += escaped;
  body += "\",";
  body += "\"size\":\"1024x1024\",";
  body += "\"n\":1";
  body += "}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(120);

  if (!client.connect("api.openai.com", 443)) {
    Serial.println("  FAILED: TLS connect");
    return false;
  }

  // Write HTTP request manually (raw socket — proven reliable)
  client.print("POST /v1/images/generations HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Content-Type: application/json\r\n");
  client.printf("Content-Length: %d\r\n", body.length());
  client.print("Authorization: Bearer ");
  client.print(OPENAI_API_KEY);
  client.print("\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");
  client.print(body);
  client.flush();

  Serial.println("Calling OpenAI Image Generation API...");

  // Read status line with timeout
  unsigned long start = millis();
  String statusLine;
  while (statusLine.length() == 0 && (millis() - start) < 120000) {
    if (client.available()) {
      statusLine = client.readStringUntil('\n');
      break;
    }
    delay(50);
  }
  statusLine.trim();
  Serial.printf("  Status: %s\n", statusLine.c_str());

  int httpCode = -1;
  if (statusLine.startsWith("HTTP")) {
    int sp1 = statusLine.indexOf(' ');
    if (sp1 > 0) {
      int sp2 = statusLine.indexOf(' ', sp1 + 1);
      String codeStr = sp2 > sp1 ? statusLine.substring(sp1 + 1, sp2)
                                 : statusLine.substring(sp1 + 1);
      httpCode = codeStr.toInt();
    }
  }

  // Skip headers, capture Content-Length
  int contentLength = -1;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      break;
    int clIdx = line.indexOf("Content-Length:");
    if (clIdx >= 0) {
      contentLength = line.substring(clIdx + 15).toInt();
    }
  }

  if (httpCode != 200) {
    Serial.printf("OpenAI Image API returned %d\n", httpCode);
    if (contentLength > 0 && contentLength < 4096) {
      static char errBuf[4096];
      memset(errBuf, 0, sizeof(errBuf));
      size_t readBytes = 0;
      while (client.available() && readBytes < (size_t)contentLength) {
        int avail = client.available();
        if (avail > 0) {
          size_t toRead = (size_t)avail;
          if (readBytes + toRead > (size_t)contentLength)
            toRead = contentLength - readBytes;
          readBytes +=
              client.readBytes((uint8_t *)(errBuf + readBytes), toRead);
        }
        delay(10);
      }
      errBuf[readBytes] = '\0';
      Serial.printf("Image API error body: %s\n", errBuf);
    } else {
      while (client.available())
        client.read();
    }
    client.stop();
    return false;
  }

  // Read entire response (b64_json response, up to 4 MiB)
  const size_t MAX_RESP = 4096 * 1024;

  uint8_t *respBuf = (uint8_t *)ps_malloc(MAX_RESP + 1);
  if (!respBuf) {
    Serial.println("Failed to allocate PSRAM for API response");
    client.stop();
    return false;
  }

  size_t totalRead = 0;
  unsigned long noDataSince = millis();
  while ((millis() - noDataSince) < 5000) {
    int avail = client.available();
    if (avail > 0) {
      if (totalRead + (size_t)avail > MAX_RESP) {
        Serial.println("Response exceeds max buffer, clamping");
        break;
      }
      totalRead += client.readBytes(respBuf + totalRead, avail);
      noDataSince = millis();
    } else if (!client.connected()) {
      delay(100);
      if (!client.available())
        break;
    } else {
      delay(10);
    }
  }
  client.stop();
  respBuf[totalRead] = '\0';

  if (totalRead == 0) {
    Serial.println("Empty response body");
    free(respBuf);
    return false;
  }

  Serial.printf("Image API response: %d bytes\n", totalRead);

  // Extract b64_json from raw response (skip JsonDocument — too large for heap)
  const char *resp = (const char *)respBuf;

  // Debug: show beginning of response
  if (totalRead > 200) {
    char snippet[201];
    memcpy(snippet, resp, 200);
    snippet[200] = '\0';
    Serial.printf("  Response head: %s\n", snippet);
  }

  const char *key = strstr(resp, "\"b64_json\"");
  if (!key) {
    Serial.println("No b64_json key in response");
    free(respBuf);
    return false;
  }
  // key+10 skips past the 10 chars of "b64_json"
  // strchr finds the next " (opening quote of value)
  const char *openQuote = strchr(key + 10, '\"');
  if (!openQuote) {
    Serial.printf("  Around key: %.50s\n", key);
    Serial.println("Malformed b64_json: no opening quote");
    free(respBuf);
    return false;
  }
  const char *b64Start = openQuote + 1;
  const char *b64End = strchr(b64Start, '\"');
  if (!b64End) {
    // Response was truncated — use rest of buffer, trim trailing JSON noise
    const char *end = resp + totalRead;
    while (end > b64Start &&
           (end[-1] == '\"' || end[-1] == '}' || end[-1] == ']' ||
            end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' ||
            end[-1] == '\t'))
      end--;
    b64End = end;
    Serial.printf("  Truncated b64_json: using %d remaining bytes\n",
                  b64End - b64Start);
  }
  size_t b64Len = b64End - b64Start;
  Serial.printf("  b64_json: %d chars, starts: %.40s\n", b64Len, b64Start);

  // Move b64 string to start of respBuf instead of allocating a second ~3 MB
  // buffer
  if (b64Start != (const char *)respBuf) {
    memmove(respBuf, b64Start, b64Len);
  }
  respBuf[b64Len] = '\0';

  // Base64 decode into PSRAM buffer
  size_t pngLen = 0;
  int rc = mbedtls_base64_decode(NULL, 0, &pngLen, respBuf, b64Len);
  if (rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
    Serial.printf("Base64 decode size calc failed: %d\n", rc);
    free(respBuf);
    return false;
  }

  uint8_t *pngBuf = (uint8_t *)ps_malloc(pngLen);
  if (!pngBuf) {
    Serial.println("Failed to allocate PSRAM for decoded PNG");
    free(respBuf);
    return false;
  }

  rc = mbedtls_base64_decode(pngBuf, pngLen, &pngLen, respBuf, b64Len);
  if (rc != 0) {
    Serial.printf("Base64 decode failed: %d\n", rc);
    free(respBuf);
    free(pngBuf);
    return false;
  }

  // Free b64 data — no longer needed
  free(respBuf);

  Serial.printf("Decoded PNG: %d bytes\n", pngLen);

  // Process PNG through dithering pipeline and save to SPIFFS
  String path = todayImagePath();
  bool ok = processAndSaveImage(pngBuf, pngLen, path.c_str());
  free(pngBuf);

  return ok;
}

// ============================================================
// Floyd-Steinberg dithering (row-by-row streaming)
// ============================================================

static float gErrorR[2][PIC_W];
static float gErrorG[2][PIC_W];
static float gErrorB[2][PIC_W];
static int gErrorRow = 0;
static fs::File gOutputFile;

static void initDither() {
  memset(gErrorR, 0, sizeof(gErrorR));
  memset(gErrorG, 0, sizeof(gErrorG));
  memset(gErrorB, 0, sizeof(gErrorB));
  gErrorRow = 0;
}

static uint8_t closestPalette(int r, int g, int b) {
  uint8_t best = 0;
  int bestDist = 999999999;
  for (uint8_t i = 0; i < 6; i++) {
    int dr = r - SPECTRA6[i].r;
    int dg = g - SPECTRA6[i].g;
    int db = b - SPECTRA6[i].b;
    int dist = dr * dr + dg * dg + db * db;
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best;
}

static void ditherRow(uint8_t *rowRgb565, int width) {
  int curRow = gErrorRow;
  int nextRow = 1 - gErrorRow;

  uint8_t outBuf[PIC_W / 2];
  int outIdx = 0;

  for (int x = 0; x < width; x++) {
    int px = x * 2;
    uint16_t rgb565 = ((uint16_t)rowRgb565[px] << 8) | rowRgb565[px + 1];
    float r =
        (float)((rgb565 >> 11) & 0x1F) * 255.0f / 31.0f + gErrorR[curRow][x];
    float gr =
        (float)((rgb565 >> 5) & 0x3F) * 255.0f / 63.0f + gErrorG[curRow][x];
    float b = (float)(rgb565 & 0x1F) * 255.0f / 31.0f + gErrorB[curRow][x];

    r = max(0.0f, min(255.0f, r));
    gr = max(0.0f, min(255.0f, gr));
    b = max(0.0f, min(255.0f, b));

    uint8_t idx =
        closestPalette((int)roundf(r), (int)roundf(gr), (int)roundf(b));
    const PaletteColor &pc = SPECTRA6[idx];

    float errR = r - (float)pc.r;
    float errGr = gr - (float)pc.g;
    float errB = b - (float)pc.b;

    if (x + 1 < width) {
      gErrorR[curRow][x + 1] += errR * 7.0f / 16.0f;
      gErrorG[curRow][x + 1] += errGr * 7.0f / 16.0f;
      gErrorB[curRow][x + 1] += errB * 7.0f / 16.0f;
    }
    if (x > 0) {
      gErrorR[nextRow][x - 1] += errR * 3.0f / 16.0f;
      gErrorG[nextRow][x - 1] += errGr * 3.0f / 16.0f;
      gErrorB[nextRow][x - 1] += errB * 3.0f / 16.0f;
    }
    gErrorR[nextRow][x] += errR * 5.0f / 16.0f;
    gErrorG[nextRow][x] += errGr * 5.0f / 16.0f;
    gErrorB[nextRow][x] += errB * 5.0f / 16.0f;
    if (x + 1 < width) {
      gErrorR[nextRow][x + 1] += errR * 1.0f / 16.0f;
      gErrorG[nextRow][x + 1] += errGr * 1.0f / 16.0f;
      gErrorB[nextRow][x + 1] += errB * 1.0f / 16.0f;
    }

    if (x % 2 == 0) {
      outBuf[outIdx] = PALETTE_NIBBLE[idx] << 4;
    } else {
      outBuf[outIdx] |= PALETTE_NIBBLE[idx];
      outIdx++;
    }
  }

  gOutputFile.write(outBuf, PIC_W / 2);

  memset(gErrorR[curRow], 0, PIC_W * sizeof(float));
  memset(gErrorG[curRow], 0, PIC_W * sizeof(float));
  memset(gErrorB[curRow], 0, PIC_W * sizeof(float));

  gErrorRow = nextRow;
}

// ============================================================
// PNG decoding (PNGdec callbacks)
// ============================================================

static PNG gPng;

static int pngDrawCallback(PNGDRAW *pDraw) {
  uint8_t rowBuf[PIC_W * 2];
  gPng.getLineAsRGB565(pDraw, (uint16_t *)rowBuf, PNG_RGB565_BIG_ENDIAN,
                       0xFFFFFF);
  ditherRow(rowBuf, PIC_W);
  return 1;
}

// ============================================================
// PNG IHDR header inspection (diagnostic)
// ============================================================
static void inspectPngHeader(const uint8_t *pngData, size_t pngLen) {
  if (pngLen < 33) {
    Serial.println("  PNG too small for IHDR inspection (<33 bytes)");
    return;
  }

  // PNG signature: 137 80 78 71 13 10 26 10
  const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  bool sigOk = true;
  for (int i = 0; i < 8; i++) {
    if (pngData[i] != sig[i]) {
      sigOk = false;
      break;
    }
  }
  if (!sigOk) {
    Serial.println("  Not a valid PNG (signature mismatch)");
    Serial.printf("  First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                  pngData[0], pngData[1], pngData[2], pngData[3], pngData[4],
                  pngData[5], pngData[6], pngData[7]);
    return;
  }

  // IHDR starts at offset 8
  // bytes 8-11: chunk length (big-endian, should be 13)
  uint32_t ihdrLen = ((uint32_t)pngData[8] << 24) |
                     ((uint32_t)pngData[9] << 16) |
                     ((uint32_t)pngData[10] << 8) | pngData[11];

  // bytes 12-15: "IHDR"
  char chunkType[5] = {0};
  memcpy(chunkType, &pngData[12], 4);

  // IHDR data (13 bytes at offset 16)
  uint32_t width = ((uint32_t)pngData[16] << 24) |
                   ((uint32_t)pngData[17] << 16) |
                   ((uint32_t)pngData[18] << 8) | pngData[19];
  uint32_t height = ((uint32_t)pngData[20] << 24) |
                    ((uint32_t)pngData[21] << 16) |
                    ((uint32_t)pngData[22] << 8) | pngData[23];
  uint8_t bitDepth = pngData[24];
  uint8_t colorType = pngData[25];
  uint8_t compression = pngData[26];
  uint8_t filter = pngData[27];
  uint8_t interlace = pngData[28];

  const char *colorTypeNames[] = {"0=Grayscale",
                                  "(unknown 1)",
                                  "2=Truecolor (RGB)",
                                  "3=Indexed (palette)",
                                  "4=Grayscale+Alpha",
                                  "(unknown 5)",
                                  "6=Truecolor+Alpha (RGBA)"};
  const char *colorName =
      (colorType <= 6) ? colorTypeNames[colorType] : "INVALID";

  Serial.printf("PNG_MAX_BUFFERED_PIXELS: %d\n", PNG_MAX_BUFFERED_PIXELS);

  Serial.printf("  PNG IHDR: %d x %d, bitDepth=%d, colorType=%s, "
                "compression=%d, filter=%d, interlace=%d, IHDRlen=%u\n",
                width, height, bitDepth, colorName, compression, filter,
                interlace, ihdrLen);
}

// ============================================================
// Process PNG data and save dithered 4bpp file to SPIFFS
// ============================================================
bool processAndSaveImage(const uint8_t *pngData, size_t pngLen,
                         const char *path) {
  Serial.printf("Processing PNG (%d bytes) -> %s\n", pngLen, path);

  inspectPngHeader(pngData, pngLen);

  gOutputFile = SPIFFS.open(path, FILE_WRITE);
  if (!gOutputFile) {
    Serial.println("Failed to open output file in SPIFFS");
    return false;
  }

  initDither();

  int rc = gPng.openRAM((uint8_t *)pngData, (int32_t)pngLen, pngDrawCallback);
  if (rc != PNG_SUCCESS) {
    Serial.printf("PNG decode failed: %d\n", rc);
    Serial.printf(
        "Use forked PNGdec lib from https://github.com/maet3608/PNGdec\n");
    gOutputFile.close();
    SPIFFS.remove(path);
    return false;
  }

  rc = gPng.decode(NULL, 0);
  if (rc != PNG_SUCCESS) {
    Serial.printf("PNG decode failed at decode: %d\n", rc);
    gOutputFile.close();
    SPIFFS.remove(path);
    return false;
  }

  gPng.close();
  gOutputFile.close();

  Serial.printf("Saved dithered image (%d x %d, 4bpp, %d bytes)\n",
                gPng.getWidth(), gPng.getHeight(), PIC_W * PIC_H / 2);
  return true;
}

// ============================================================
// Image file management
// ============================================================

String todayImagePath() {
  struct tm t;
  if (!getLocalTime(&t)) {
    return "/img_unknown.bin";
  }
  char buf[32];
  snprintf(buf, sizeof(buf), IMAGE_PATH, t.tm_year + 1900, t.tm_mon + 1,
           t.tm_mday);
  return String(buf);
}

bool imageExistsForToday() {
  String path = todayImagePath();
  return SPIFFS.exists(path);
}

void cleanupOldImages() {
  String today = todayImagePath();
  fs::File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) {
    return;
  }
  fs::File f = root.openNextFile();
  while (f) {
    String name = String("/") + f.name();
    if (name.startsWith("/img_") && name != today) {
      SPIFFS.remove(name);
      Serial.printf("Removed old image: %s\n", name.c_str());
    }
    f = root.openNextFile();
  }
}

// ============================================================
// Main entry point: fetch and store the daily image
// ============================================================
bool fetchAndStoreDailyImage() {
  if (PIC_SKIP_EXISTING_IMAGE && imageExistsForToday()) {
    Serial.println("Today's image already exists, skipping generation.");
    return true;
  }

  Serial.println("Generating new image for today...");

  WiFi.begin(SSID, PASSWORD);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 100) {
    delay(100);
    retry++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Picture: WiFi connect failed");
    return false;
  }

  if (!generateImagePrompt()) {
    Serial.println("Failed to generate image prompt");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  bool ok = generateAndStoreImage();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  if (ok) {
    cleanupOldImages();
  } else {
    Serial.println("Failed to generate and store image");
  }
  return ok;
}

// ============================================================
// Draw the persisted 4bpp image to the e-paper display
// ============================================================
void drawPicture(EPaper &epaperRef) {
  String path = todayImagePath();
  if (!SPIFFS.exists(path)) {
    // placeholder
    return;
  }

  fs::File f = SPIFFS.open(path, FILE_READ);
  if (!f) {
    Serial.println("Failed to open image file");
    return;
  }

  size_t fileSize = f.size();
  size_t expectedSize = (size_t)PIC_W * PIC_H / 2;
  if (fileSize != expectedSize) {
    Serial.printf("Image file size mismatch: %d vs expected %d\n", fileSize,
                  expectedSize);
    f.close();
    return;
  }

  Serial.printf("Drawing image from %s (%d bytes)\n", path.c_str(), fileSize);

  uint8_t rowBuf[PIC_W / 2];

  for (int y = 0; y < PIC_H; y++) {
    size_t read = f.read(rowBuf, PIC_W / 2);
    if (read != (size_t)PIC_W / 2) {
      Serial.printf("Read error at row %d\n", y);
      break;
    }

    for (int x = 0; x < PIC_W; x++) {
      uint8_t nibble;
      if (x % 2 == 0) {
        nibble = (rowBuf[x / 2] >> 4) & 0x0F;
      } else {
        nibble = rowBuf[x / 2] & 0x0F;
      }

      uint16_t color;
      switch (nibble) {
      case 0xF:
        color = TFT_BLACK;
        break;
      case 0x0:
        color = TFT_WHITE;
        break;
      case 0xB:
        color = TFT_YELLOW;
        break;
      case 0x6:
        color = TFT_RED;
        break;
      case 0xD:
        color = TFT_BLUE;
        break;
      case 0x2:
        color = TFT_GREEN;
        break;
      default:
        color = TFT_WHITE;
        break;
      }

      epaperRef.drawPixel(PIC_X + x, PIC_Y + y, color);
    }
  }

  f.close();
  Serial.println("Image drawn.");
}