// mem_probe.cpp — temporary heap-leak probe (panel-stuck investigation).
//
// Compresses an "8-day slow leak" into minutes by churning the SAME outbound
// client alloc/free paths the stuck reconnect loop hits:
//   * HTTPClient + WiFiClient created per call  (as weather.cpp / stock.cpp do)
//   * one reused MQTTClient, connect()/disconnect()  (as reconnect_mqtt() does)
//
// Targets closed ports on the default gateway, so every connect gets an instant
// RST — fast-fail, full alloc path exercised, no multi-second blocking.
//
// Read [MEM probe-settle bN nN] lines: `internal` / `intMin` / `largest`
// marching down and NOT recovering between rounds == a real leak. Flat == not
// the culprit; the blocking-DNS mechanism stands alone.
//
// Disabled unless the build sets MEM_PROBE=1.

#include "mem_probe.h"

#if MEM_PROBE

#include "globals.h" // log_mem(), isWifiConnected
#include <Arduino.h>
#include <HTTPClient.h>
#include <MQTT.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void probe_http_once(const String &url) {
  WiFiClient c;
  HTTPClient http;
  http.setConnectTimeout(800);
  http.setTimeout(800);
  if (http.begin(c, url)) {
    http.GET();
    http.end();
  }
}

static void probe_task(void *) {
  while (!isWifiConnected)
    vTaskDelay(pdMS_TO_TICKS(500));
  vTaskDelay(pdMS_TO_TICKS(3000)); // let the normal boot settle first

  IPAddress gw = WiFi.gatewayIP();
  String httpUrl = String("http://") + gw.toString() + ":9/"; // discard port

  WiFiClient mqc;
  MQTTClient m(512);
  m.begin(gw, 1883, mqc); // nothing listens on the gateway's 1883
  m.setTimeout(800);

  Serial.printf("[PROBE] gw=%s  http=%s  mqtt=%s:1883\n", gw.toString().c_str(),
                httpUrl.c_str(), gw.toString().c_str());
  log_mem("probe-boot");

  const int batches[] = {20, 20, 50, 50, 100, 100, 200, 200, 400};
  const int nb = sizeof(batches) / sizeof(batches[0]);
  for (int b = 0; b < nb; b++) {
    int n = batches[b];
    char tag[40];
    snprintf(tag, sizeof(tag), "probe-start b%d n%d", b, n);
    log_mem(tag);
    for (int i = 0; i < n; i++) {
      probe_http_once(httpUrl);
      m.connect("mem-probe"); // fast RST -> fail, like err=-3
      m.disconnect();
      vTaskDelay(pdMS_TO_TICKS(15));
    }
    snprintf(tag, sizeof(tag), "probe-end b%d n%d", b, n);
    log_mem(tag);
    vTaskDelay(pdMS_TO_TICKS(20000)); // settle window
    snprintf(tag, sizeof(tag), "probe-settle b%d n%d", b, n);
    log_mem(tag);
  }
  log_mem("probe-done");
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(30000));
    log_mem("probe-idle");
  }
}

void mem_probe_start() {
  xTaskCreatePinnedToCore(probe_task, "mem_probe", 12288, nullptr, 1, nullptr, 0);
}

#else
void mem_probe_start() {}
#endif
