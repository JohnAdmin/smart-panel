// weather.cpp
// Extracted from wifi_manager.cpp — all Open-Meteo weather logic lives here.

#include "config.h"
#include "globals.h" // safe_wdt_reset()
#include "lang.h"    // wmoToDesc() returns translated strings
#include "wifi_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>

// --- WMO weather code to human-readable description ---
// Looked up on every call so the text follows the active language, grouped the
// way Open-Meteo documents the ranges.
const char *wmoToDesc(int code) {
  if (code == 0)  return L(L_WX_CLEAR);
  if (code <= 3)  return L(L_WX_PARTLY);
  if (code <= 49) return L(L_WX_FOG);
  if (code <= 59) return L(L_WX_DRIZZLE);
  if (code <= 69) return L(L_WX_RAIN);
  if (code <= 79) return L(L_WX_SNOW);
  if (code <= 84) return L(L_WX_SHOWERS);
  if (code <= 94) return L(L_WX_THUNDER);
  return L(L_WX_STORM);
}

// Day of week for a Gregorian date, 0=Sunday (Sakamoto). The forecast dates
// are the location's own calendar days, so they are labelled from the date
// Open-Meteo returns rather than from the panel clock's GMT offset.
static int dayOfWeek(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 1 || m > 12) return 0;
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// Geocoding cache — avoids re-resolving same city every update
static float cached_lat = 0;
static float cached_lon = 0;
static char cached_city[32] = "";

// --- Weather Fetch (Open-Meteo + Geocoding API) ---
void fetchWeather() {
  if (!isWifiConnected)
    return;

  // DNS resolution for the calls below isn't reliably bounded by
  // HTTPClient::setTimeout() on this core — a slow or dead DNS server can
  // block this whole function well past HTTP_TIMEOUT_MS, overrunning the
  // network task's 5 s watchdog and panic-rebooting the panel (taking any
  // live MQTT connection down with it). Weather/AQI are cosmetic, so
  // unsubscribe network_task from the watchdog for the duration instead of
  // letting a bad DNS answer crash the panel. Single exit point below
  // re-subscribes; safe_wdt_reset() calls in between become no-ops.
  esp_task_wdt_delete(NULL);

  HTTPClient http;
  float lat = DEFAULT_LATITUDE;
  float lon = DEFAULT_LONGITUDE;

  // 0. Coordinates picked in the portal win outright. The city name alone is
  // ambiguous — a "Springfield" resolves somewhere, just not necessarily the
  // one you meant — and skipping geocoding also drops a blocking HTTP call
  // from every boot and every refresh.
  if (weatherLat != 0.0f || weatherLon != 0.0f) {
    lat = weatherLat;
    lon = weatherLon;
    strncpy(weatherCityName, weatherCity, sizeof(weatherCityName) - 1);
    weatherCityName[sizeof(weatherCityName) - 1] = '\0';
    Serial.printf("[WEATHER] Using saved coords for %s: %.4f,%.4f\n",
                  weatherCity, lat, lon);
  }
  // 1. Geocoding — resolve city name to lat/lon (cached)
  else if (strlen(weatherCity) > 0) {
    if (strcmp(weatherCity, cached_city) == 0 && cached_lat != 0) {
      // Use cached coordinates
      lat = cached_lat;
      lon = cached_lon;
      Serial.printf("[WEATHER] Using cached coords for %s: %.2f,%.2f\n", cached_city, lat, lon);
    } else {
      String encodedCity = weatherCity;
      encodedCity.replace(" ", "+");
      String geoUrl =
          "http://geocoding-api.open-meteo.com/v1/search?name=" + encodedCity +
          "&count=1&format=json";

      Serial.printf("[WEATHER] Geocoding: %s\n", geoUrl.c_str());
      http.setTimeout(HTTP_TIMEOUT_MS);
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.begin(geoUrl);
      safe_wdt_reset();
      int httpCode = http.GET();
      safe_wdt_reset();
      if (httpCode == 200) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getString());
        if (!err && doc["results"].size() > 0) {
          lat = doc["results"][0]["latitude"].as<float>();
          lon = doc["results"][0]["longitude"].as<float>();
          const char *resolved = doc["results"][0]["name"] | weatherCity;
          strncpy(weatherCityName, resolved, sizeof(weatherCityName) - 1);
          weatherCityName[sizeof(weatherCityName) - 1] = '\0';
          // Cache results
          cached_lat = lat;
          cached_lon = lon;
          strncpy(cached_city, weatherCity, sizeof(cached_city) - 1);
          cached_city[sizeof(cached_city) - 1] = '\0';
          Serial.printf("[WEATHER] Found %s at %.2f,%.2f\n", weatherCityName, lat,
                        lon);
        }
      } else {
        Serial.printf("[WEATHER] Geocoding failed: %d\n", httpCode);
      }
      http.end();
    }
  }

  // 2. Current conditions + hourly + daily forecast via lat/lon, in one
  // request. timezone=auto makes daily[0] "today" at the forecast location,
  // which is what the Weather screensaver labels it. forecast_hours starts at
  // the current hour, which the strip skips, so ask for two more than it
  // shows. The whole response is ~2 KB.
  String weatherUrl =
      "http://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
      "&longitude=" + String(lon, 4) +
      "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
      "weather_code,wind_speed_10m"
      "&hourly=temperature_2m,precipitation_probability"
      "&forecast_hours=" + String(WEATHER_HOURLY_SLOTS + 2) +
      "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
      "precipitation_probability_max"
      "&forecast_days=" + String(WEATHER_FORECAST_DAYS) + "&timezone=auto";

  Serial.printf("[WEATHER] Fetching: %s\n", weatherUrl.c_str());
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(weatherUrl);
  safe_wdt_reset();
  // Renamed to avoid shadowing the `httpCode` from the geocoding block above
  int weatherHttpCode = http.GET();
  safe_wdt_reset();

  if (weatherHttpCode == 200) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    if (!err && doc["current"]["temperature_2m"].is<float>()) {
      JsonObject cur = doc["current"];
      weatherTemp = cur["temperature_2m"].as<float>();
      weatherCode = cur["weather_code"] | 0;
      weatherFeels = cur["apparent_temperature"] | weatherTemp;
      weatherHumidity = cur["relative_humidity_2m"] | 0;
      weatherWind = cur["wind_speed_10m"] | 0.0f;

      JsonObject daily = doc["daily"];
      JsonArray days = daily["time"];
      int n = 0;
      for (int i = 0; i < (int)days.size() && n < WEATHER_FORECAST_DAYS; i++) {
        int y, m, d;
        const char *iso = days[i] | "";
        if (sscanf(iso, "%d-%d-%d", &y, &m, &d) != 3) continue;
        WeatherDay &w = weatherForecast[n++];
        w.code = daily["weather_code"][i] | 0;
        w.hi = (int)lroundf(daily["temperature_2m_max"][i] | 0.0f);
        w.lo = (int)lroundf(daily["temperature_2m_min"][i] | 0.0f);
        // null for a day the model has no precipitation figure for
        w.rainPct = daily["precipitation_probability_max"][i].is<int>()
                        ? daily["precipitation_probability_max"][i].as<int>()
                        : -1;
        w.wday = dayOfWeek(y, m, d);
      }
      weatherForecastDays = n;

      // Hourly, from the first full hour after "now". Both timestamps are the
      // location's local time in the same "YYYY-MM-DDTHH:MM" form, so a string
      // compare orders them without involving the panel's own GMT offset.
      const char *nowIso = cur["time"] | "";
      JsonObject hourly = doc["hourly"];
      JsonArray hours = hourly["time"];
      int h = 0;
      for (int i = 0; i < (int)hours.size() && h < WEATHER_HOURLY_SLOTS; i++) {
        const char *iso = hours[i] | "";
        if (strlen(iso) < 16 || strcmp(iso, nowIso) <= 0) continue;
        WeatherHour &w = weatherHourly[h++];
        w.hour = atoi(iso + 11);
        w.temp = (int)lroundf(hourly["temperature_2m"][i] | 0.0f);
        w.rainPct = hourly["precipitation_probability"][i].is<int>()
                        ? hourly["precipitation_probability"][i].as<int>()
                        : -1;
      }
      weatherHourlyCount = h;

      struct tm now;
      if (getLocalTime(&now, 0))
        strftime(weatherUpdatedAt, sizeof(weatherUpdatedAt), "%H:%M", &now);

      weatherValid = true;
      weatherGeneration = weatherGeneration + 1; // last — see the note on WeatherDay in globals.h
      Serial.printf("[WEATHER] %s %.1fC, %s, %d-day forecast\n",
                    weatherCityName, weatherTemp, wmoToDesc(weatherCode), n);
    } else {
      Serial.printf("[WEATHER] JSON error: %s\n", err.c_str());
      weatherValid = false;
    }
  } else {
    Serial.printf("[WEATHER] HTTP error: %d\n", weatherHttpCode);
    weatherValid = false;
  }
  http.end();

  // 3. Air quality — a separate Open-Meteo host, but the same coordinates the
  // geocoding step above already resolved and cached, so this adds a request
  // rather than a lookup. US AQI rather than European: its 0-50 "Good" band is
  // the scale most people have seen, and it is what the design's example used.
  String aqUrl =
      "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=" +
      String(lat, 4) + "&longitude=" + String(lon, 4) + "&current=us_aqi";

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(aqUrl);
  safe_wdt_reset();
  int aqHttpCode = http.GET();
  safe_wdt_reset();

  if (aqHttpCode == 200) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    // A station can be missing the reading even on a 200, in which case the
    // key is absent or null and the card stays hidden rather than showing 0.
    if (!err && doc["current"]["us_aqi"].is<int>()) {
      airQualityAqi = doc["current"]["us_aqi"].as<int>();
      airQualityValid = (airQualityAqi >= 0 && airQualityAqi <= 500);
      Serial.printf("[AIR] US AQI %d\n", airQualityAqi);
    } else {
      Serial.printf("[AIR] No us_aqi in response (%s)\n",
                    err ? err.c_str() : "missing key");
      airQualityValid = false;
    }
  } else {
    Serial.printf("[AIR] HTTP error: %d\n", aqHttpCode);
    airQualityValid = false;
  }
  http.end();

  esp_task_wdt_add(NULL);
}
