#pragma once
// stock.h — Twelve Data stock ticker module
// Fetches up to 3 configurable symbols (SET, US, Forex/Metals) every 5 min.

#include <Arduino.h>

#define STOCK_MAX_SYMBOLS 3

struct StockData {
  char symbol[20];       // symbol string (e.g. "AOT.BK", "AAPL", "XAU/USD")
  float price;           // last close price
  float change;          // absolute change
  float percent_change;  // percent change (e.g. +1.23 or -0.45)
  bool market_open;
  bool is_valid;
};

// Runtime globals — defined in stock.cpp
extern StockData stockData[STOCK_MAX_SYMBOLS];
extern bool      stockEnabled;
extern char      stockSymbols[STOCK_MAX_SYMBOLS][20];
extern char      stockApiKey[48];
extern unsigned long lastStockUpdate;

void loadStockConfig();
void saveStockConfig();
void fetchStocks();
