// stock.cpp — Twelve Data stock ticker fetch & config persistence
// Free tier: 8 req/min, 800/day — fetches all 3 symbols in 1 batch call.

#include "stock.h"
#include "config.h"
#include "globals.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClient.h>

// ── Definitions ─────────────────────────────────────────────────────────────
StockData stockData[STOCK_MAX_SYMBOLS] = {};
bool      stockEnabled                 = false;
char      stockSymbols[STOCK_MAX_SYMBOLS][20] = {"AOT.BK", "AAPL", "XAU/USD"};
char      stockApiKey[48]              = "";
unsigned long lastStockUpdate          = 0;

// ── NVS persistence ──────────────────────────────────────────────────────────
void loadStockConfig() {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, true)) {
    Serial.println("[STOCK] NVS open (read) failed, using defaults");
    return;
  }
  stockEnabled = p.getBool("stock_en", false);
  String s0    = p.getString("stock_s0", "AOT.BK");
  String s1    = p.getString("stock_s1", "AAPL");
  String s2    = p.getString("stock_s2", "XAU/USD");
  String key   = p.getString("stock_key", "");
  p.end();

  strncpy(stockSymbols[0], s0.c_str(), sizeof(stockSymbols[0]) - 1);
  strncpy(stockSymbols[1], s1.c_str(), sizeof(stockSymbols[1]) - 1);
  strncpy(stockSymbols[2], s2.c_str(), sizeof(stockSymbols[2]) - 1);
  strncpy(stockApiKey,     key.c_str(), sizeof(stockApiKey) - 1);
  stockSymbols[0][sizeof(stockSymbols[0]) - 1] = '\0';
  stockSymbols[1][sizeof(stockSymbols[1]) - 1] = '\0';
  stockSymbols[2][sizeof(stockSymbols[2]) - 1] = '\0';
  stockApiKey[sizeof(stockApiKey) - 1]          = '\0';

  Serial.printf("[STOCK] Loaded: enabled=%d  s0=%s  s1=%s  s2=%s\n",
                stockEnabled, stockSymbols[0], stockSymbols[1], stockSymbols[2]);
}

void saveStockConfig() {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, false)) {
    Serial.println("[STOCK] NVS open (write) failed");
    return;
  }
  p.putBool("stock_en",  stockEnabled);
  p.putString("stock_s0", stockSymbols[0]);
  p.putString("stock_s1", stockSymbols[1]);
  p.putString("stock_s2", stockSymbols[2]);
  p.putString("stock_key", stockApiKey);
  p.end();
  Serial.println("[STOCK] Config saved to NVS");
}

// ── Fetch ────────────────────────────────────────────────────────────────────
void fetchStocks() {
  if (!isWifiConnected) return;
  if (strlen(stockApiKey) < 4) {
    Serial.println("[STOCK] No API key, skipping fetch");
    return;
  }

  // Build comma-separated symbol list; URL-encode '/' for metals (XAU/USD)
  String symbols = stockSymbols[0];
  for (int i = 1; i < STOCK_MAX_SYMBOLS; i++) {
    if (strlen(stockSymbols[i]) > 0) {
      symbols += ",";
      symbols += stockSymbols[i];
    }
  }
  symbols.replace("/", "%2F");

  String url = "http://api.twelvedata.com/quote?symbol=" + symbols +
               "&dp=2&apikey=" + String(stockApiKey);
  Serial.printf("[STOCK] GET %s\n", url.c_str());

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    Serial.println("[STOCK] http.begin failed");
    return;
  }
  // This runs on the network task, which is watchdog-supervised, and the GET
  // blocks it for up to HTTP_TIMEOUT_MS. Feeding either side keeps the request
  // itself the only thing inside the window.
  safe_wdt_reset();
  int code = http.GET();
  safe_wdt_reset();
  if (code != 200) {
    Serial.printf("[STOCK] HTTP %d\n", code);
    http.end();
    return;
  }
  String body = http.getString();
  http.end();
  safe_wdt_reset(); // getString() on a slow link is part of the same budget

  // Parse — batch response: {"SYM": {...}, ...}
  // Single-symbol fallback: {"symbol": "SYM", "close": "...", ...}
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[STOCK] JSON parse error: %s\n", err.c_str());
    return;
  }

  bool isSingle = doc["symbol"].is<const char *>();

  for (int i = 0; i < STOCK_MAX_SYMBOLS; i++) {
    if (strlen(stockSymbols[i]) == 0) {
      stockData[i].is_valid = false;
      continue;
    }
    JsonObject obj;
    if (isSingle) {
      obj = doc.as<JsonObject>();
    } else {
      obj = doc[stockSymbols[i]].as<JsonObject>();
    }
    if (obj.isNull() || (obj["code"].as<int>() != 0 && !obj["close"].is<const char *>())) {
      const char *msg = obj["message"] | "no data (check symbol/plan)";
      Serial.printf("[STOCK] %s: code=%d %s\n", stockSymbols[i], obj["code"].as<int>(), msg);
      stockData[i].is_valid = false;
      continue;
    }

    strncpy(stockData[i].symbol, stockSymbols[i], sizeof(stockData[i].symbol) - 1);
    stockData[i].symbol[sizeof(stockData[i].symbol) - 1] = '\0';
    stockData[i].price           = atof(obj["close"]           | "0");
    stockData[i].change          = atof(obj["change"]          | "0");
    stockData[i].percent_change  = atof(obj["percent_change"]  | "0");
    stockData[i].market_open     = obj["is_market_open"] | false;
    stockData[i].is_valid        = true;

    Serial.printf("[STOCK] %s: %.2f (%+.2f%%) market=%s\n",
                  stockData[i].symbol, stockData[i].price,
                  stockData[i].percent_change,
                  stockData[i].market_open ? "OPEN" : "CLOSED");
  }
}
