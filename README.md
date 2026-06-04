# xiao-ee02-e-paper-calendar

Calendar, Weather and AI picture on XIAO EE02 E-Paper

## Overview

A battery-powered e-paper dashboard built on the **Seeed Studio XIAO ESP32S3 Plus** with the **EE02 6-color e-paper display** (1600×1200, Spectra 6 palette). It wakes periodically to refresh the screen with:

- **AI-generated artwork** — Weather- and season-aware images in a Japanese anime style (Studio Ghibli / Totoro theme) generated via OpenAI's API, Floyd-Steinberg dithered for the 6-color display
- **Current weather** — Temperature, humidity, rainfall, wind speed, and a hand-drawn weather icon fetched from OpenWeatherMap
- **5-day forecast** — Daily high/low temperatures, weather icons, and descriptions grouped from 3-hourly forecast data
- **Monthly calendar** — Full month grid with today highlighted and weekend days in red
- **Calendar events** — Today's events pulled from a Google Calendar via a Google Apps Script endpoint
- **Battery indicator** — Battery charge percentage drawn in the corner

The device enters deep sleep between updates to conserve power, with smart wake scheduling that avoids the "dead zone" (00:00–04:59) when nobody is watching.

## Hardware

| Component | Detail |
|-----------|--------|
| **Board** | Seeed Studio XIAO ESP32S3 Plus |
| **Display** | XIAO EE02 Expansion Board (6-color e-paper, 1600×1200) |
| **Battery** | Li-Po battery (connected via XIAO battery header) |

## How It Works

1. **Wake from deep sleep** — Every `DEEP_SLEEP_HOURS` (default: 3 hours), or at 05:00 if the wake time would fall in the dead zone
2. **Connect to Wi-Fi** and sync time via NTP
3. **Fetch weather data** from OpenWeatherMap (current, 3-hourly forecast, and daily grouped forecast)
4. **Fetch calendar events** from a Google Apps Script web app
5. **Generate daily AI image** — If no image exists for today:
   - Sends current weather + season to OpenAI GPT (`gpt-4o-mini`) to create an image prompt
   - Sends the prompt to OpenAI Image Generation (`gpt-image-1-mini`) returning a 1024×1024 PNG
   - Decodes the PNG and applies Floyd-Steinberg dithering to the Spectra 6 palette
   - Stores the 4bpp raw image in SPIFFS
6. **Render the display** — Draws all sections (picture, weather, calendar, events, battery) to the e-paper
7. **Deep sleep** until the next cycle

## Layout

```
┌───────────────────────────────────────────────┐
│                    ┌─────────────────────────┐│
│                    │      Weather Panel      ││
│   AI Picture       │                         ││
│   (1024×1024)      │  Day name, temperature  ││
│                    │  Weather icon (large)   ││
│                    │  Humidity, rain, wind   ││
│                    │                         ││
│                    │  3-hourly forecast (5×) ││
├────────────────────┼─────────────────────────┤│
│                    │    Monthly Calendar     ││
│   5-Day Forecast   │                         ││
│                    │    Calendar Events      ││
│                    │               [Bat]     ││
└───────────────────────────────────────────────┘
```

## Configuration

All settings are in `constants.h`:

| Constant | Description |
|----------|-------------|
| `WX_LOCATION` | City and country for weather (e.g. `"Lohr am Main,DE"`) |
| `TIMEZONE` | POSIX timezone string for NTP sync |
| `PIC_THEME` | AI image generation theme/style description |
| `PIC_THEME_SHORT` | Short label used in prompts |
| `PIC_SKIP_EXISTING_IMAGE` | Skip generation if today's image already exists |
| `DEEP_SLEEP_HOURS` | Hours between wake cycles (0 = disabled, for debugging) |

API credentials go in a `credentials.h` file (not tracked in git):

- `SSID` / `PASSWORD` — Wi-Fi credentials
- `OPENWEATHER_API_KEY` — OpenWeatherMap API key
- `OPENAI_API_KEY` — OpenAI API key
- `GOOGLE_CALENDAR_URL` / `CALENDAR_API_KEY` — Google Apps Script endpoint and key for calendar events

## Arduino IDE Settings

| Setting | Value |
|---------|-------|
| Board | XIAO_ESP32S3_PLUS |
| USB CDC on Boot | Enabled |
| PSRAM | OPI PSRAM |
| Partition Scheme | Default with spiffs (3MB APP/1.5MB SPIFFS) |
| ESP32 Core | 4.0.0-alpha1 |

## Dependencies

| Library | Version |
|---------|---------|
| [Seeed_GFX](https://github.com/Seeed-Studio/Seeed_GFX) (TFT_eSPI fork) | 2.0.3 |
| ArduinoJson | 7.4.3 |
| PNGdec | 1.1.6 |

## Google Calendar Setup

The `google-calendar-api.gs` file contains a Google Apps Script that exposes today's calendar events as a JSON endpoint. Deploy it as a web app and set the URL and API key in `credentials.h`.

## License

See [LICENSE](LICENSE).