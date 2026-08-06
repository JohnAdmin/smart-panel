// weather.cpp
// Extracted from wifi_manager.cpp — all Open-Meteo weather logic lives here.

#include "config.h"
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

  // 1. Geocoding — resolve city name to lat/lon (cached)
  if (strlen(weatherCity) > 0) {
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
      int httpCode = http.GET();
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
  // Renamed to avoid shadowing the `httpCode` from the geocoding block above
  int weatherHttpCode = http.GET();

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
}
