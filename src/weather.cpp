// weather.cpp
// Extracted from wifi_manager.cpp — all Open-Meteo weather logic lives here.

#include "config.h"
#include "globals.h" // safe_wdt_reset()
#include "wifi_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// --- WMO weather code to human-readable description ---
static const char *wmoToDesc(int code) {
  if (code == 0)
    return "Clear";
  if (code <= 3)
    return "Partly Cloudy";
  if (code <= 49)
    return "Foggy";
  if (code <= 59)
    return "Drizzle";
  if (code <= 69)
    return "Rain";
  if (code <= 79)
    return "Snow";
  if (code <= 84)
    return "Showers";
  if (code <= 94)
    return "Thunderstorm";
  return "Storm";
}

// Geocoding cache — avoids re-resolving same city every update
static float cached_lat = 0;
static float cached_lon = 0;
static char cached_city[32] = "";

// --- Weather Fetch (Open-Meteo + Geocoding API) ---
void fetchWeather() {
  if (!isWifiConnected)
    return;

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

  // 2. Weather forecast via lat/lon
  String weatherUrl =
      "http://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
      "&longitude=" + String(lon, 4) + "&current_weather=true";

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
    if (!err) {
      weatherTemp = doc["current_weather"]["temperature"].as<float>();
      int wmoCode = doc["current_weather"]["weathercode"].as<int>();
      strncpy(weatherDesc, wmoToDesc(wmoCode), sizeof(weatherDesc) - 1);
      weatherDesc[sizeof(weatherDesc) - 1] = '\0';
      weatherValid = true;
      Serial.printf("[WEATHER] %s %.1fC, %s\n", weatherCityName, weatherTemp,
                    weatherDesc);
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

  // Shorter than the other two on purpose: air quality is the least important
  // reading here, and this call is the one that pushed the cumulative time in
  // this function past the network task's 5 s watchdog.
  http.setTimeout(4000);
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
}
