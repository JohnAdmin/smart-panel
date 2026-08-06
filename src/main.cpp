// ESP32-S3 SC01 Plus Smart Panel
// Serial output goes to hardware UART0 via CH343 (COM4)

#include "config.h"
#include "lang.h"
#include "mqtt_manager.h"
#include "scene.h"
#include "schedule.h"
#include "ui.h"
#include "ui/ui_screens.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>


#include "hal.h"

// ── Wallpaper JPEG decode helper
// ────────────────────────────────────────────── Called from ui_wallpaper.cpp.
// Must live here because LGFX class is local. Uses lcd as Sprite parent so
// drawJpg() is properly initialised.
bool image_decode_path_to_buf(const char *path, lv_color_t *buf, int w, int h);
bool wallpaper_decode_to_buf(lv_color_t *buf, int w, int h) {
  return image_decode_path_to_buf(FS_WALLPAPER, buf, w, h);
}

// Generic JPEG/PNG/BMP -> LVGL RGB565 buffer decoder. Used for the active
// wallpaper and for the small thumbnails on the Settings wallpaper preset
// picker.
bool image_decode_path_to_buf(const char *path, lv_color_t *buf, int w, int h) {
  if (!LittleFS.exists(path)) {
    Serial.printf("[WP] File not found: %s\n", path);
    return false;
  }

  File fhdr = LittleFS.open(path, "r");
  if (!fhdr) {
    Serial.printf("[WP] Cannot open: %s\n", path);
    return false;
  }
  uint8_t magic[8] = {0};
  fhdr.read(magic, 8);
  fhdr.close();

  bool isJpeg = (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF);
  bool isPng = (magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E &&
                magic[3] == 0x47);
  bool isBmp = (magic[0] == 0x42 && magic[1] == 0x4D);
  if (!isJpeg && !isPng && !isBmp) {
    Serial.printf("[WP] Unknown format: %s\n", path);
    return false;
  }

  lgfx::LGFX_Sprite spr(hal_get_lcd());
  spr.setPsram(true);
  spr.setColorDepth(16);
  if (!spr.createSprite(w, h)) {
    Serial.println("[WP] Sprite alloc failed");
    return false;
  }
  spr.fillScreen(TFT_BLACK);

  bool ok = false;
  if (isPng) {
    File fimg = LittleFS.open(path, "r");
    uint8_t ihdr[24] = {0};
    if (fimg) { fimg.read(ihdr, 24); fimg.close(); }
    uint32_t img_w = ((uint32_t)ihdr[16] << 24) | ((uint32_t)ihdr[17] << 16) |
                     ((uint32_t)ihdr[18] << 8) | ihdr[19];
    uint32_t img_h = ((uint32_t)ihdr[20] << 24) | ((uint32_t)ihdr[21] << 16) |
                     ((uint32_t)ihdr[22] << 8) | ihdr[23];
    float sx = (img_w > 0) ? (float)w / img_w : 1.0f;
    float sy = (img_h > 0) ? (float)h / img_h : 1.0f;
    ok = spr.drawPngFile(LittleFS, path, 0, 0, 0, 0, 0, 0, sx, sy);
  } else if (isJpeg) {
    ok = spr.drawJpgFile(LittleFS, path, 0, 0, w, h);
  } else if (isBmp) {
    ok = spr.drawBmpFile(LittleFS, path, 0, 0, w, h);
  }

  if (ok) {
    uint16_t *src = (uint16_t *)spr.getBuffer();
    for (int i = 0; i < w * h; i++) {
      buf[i].full = __builtin_bswap16(src[i]);
    }
  }
  spr.deleteSprite();
  return ok;
}

/* =========================
   LVGL
   ========================= */

static lv_disp_draw_buf_t draw_buf;
// Allocate LVGL line buffers in PSRAM at runtime to free ~76KB of
// internal SRAM for WiFi/MQTT/HTTP heap. Falls back to internal RAM
// if PSRAM is unavailable.
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

#include "globals.h"

// Safe Watchdog Reset Wrapper.
// Feeds the task watchdog from inside long-running sections (saving devices,
// building 100 tiles) that would otherwise overrun the 5 s timeout. The status
// check makes it a no-op on tasks that were never subscribed, so it is safe to
// call from anywhere — including before esp_task_wdt_init() runs in setup().
void safe_wdt_reset() {
  if (esp_task_wdt_status(NULL) == ESP_OK) {
    esp_task_wdt_reset();
  }
}

bool pending_ota_reboot = false;
unsigned long ota_reboot_time = 0;

// =============================================
// Factory Reset — hold touch 3s during boot
// =============================================
#define FACTORY_RESET_HOLD_MS 3000

void check_factory_reset() {
  lgfx::LGFX_Device *lcd = hal_get_lcd();

  // Show instruction on LCD (raw LovyanGFX, no LVGL yet)
  lcd->fillScreen(0x0000);
  lcd->setTextColor(0xFFFF, 0x0000);
  lcd->setTextDatum(lgfx::middle_center);
  lcd->setFont(&lgfx::fonts::DejaVu18);
  lcd->drawString("Touch & hold 3s to Factory Reset", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20);
  lcd->setFont(&lgfx::fonts::DejaVu12);
  lcd->setTextColor(0x7BEF, 0x0000); // gray
  lcd->drawString("Release or wait to boot normally...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15);

  // Wait up to 1s for initial touch
  unsigned long waitStart = millis();
  uint16_t tx, ty;
  bool touched = false;
  while (millis() - waitStart < 1000) {
    if (lcd->getTouch(&tx, &ty)) { touched = true; break; }
    delay(20);
  }

  if (!touched) {
    lcd->fillScreen(0x0000);
    return; // No touch — normal boot
  }

  // Touch detected — show progress bar
  Serial.println("[RESET] Touch detected — hold 3s to factory reset...");
  lcd->fillScreen(0x0000);
  lcd->setTextColor(0xF800, 0x0000); // red
  lcd->setFont(&lgfx::fonts::DejaVu18);
  lcd->drawString("!! FACTORY RESET !!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 40);
  lcd->setTextColor(0xFFFF, 0x0000);
  lcd->setFont(&lgfx::fonts::DejaVu12);
  lcd->drawString("Keep holding to erase all settings...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 10);

  int barX = 90, barY = SCREEN_HEIGHT / 2 + 20, barW = 300, barH = 20;
  lcd->drawRect(barX, barY, barW, barH, 0xFFFF);

  unsigned long holdStart = millis();
  while (millis() - holdStart < FACTORY_RESET_HOLD_MS) {
    if (!lcd->getTouch(&tx, &ty)) {
      // Released early — cancel
      Serial.println("[RESET] Touch released — cancelled.");
      lcd->fillScreen(0x0000);
      lcd->setTextColor(0x07E0, 0x0000); // green
      lcd->setFont(&lgfx::fonts::DejaVu18);
      lcd->drawString("Cancelled. Booting normally...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
      delay(800);
      lcd->fillScreen(0x0000);
      return;
    }
    // Update progress bar
    float progress = (float)(millis() - holdStart) / FACTORY_RESET_HOLD_MS;
    int fillW = (int)(progress * (barW - 4));
    lcd->fillRect(barX + 2, barY + 2, fillW, barH - 4, 0xF800);
    delay(30);
  }

  // === FACTORY RESET ===
  Serial.println("[RESET] Factory reset triggered!");
  lcd->fillScreen(0x0000);
  lcd->setTextColor(0xF800, 0x0000);
  lcd->setFont(&lgfx::fonts::DejaVu18);
  lcd->drawString("Erasing all data...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

  // 1. Clear NVS
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  Serial.println("[RESET] NVS cleared.");

  // 2. Delete LittleFS data files
  if (LittleFS.begin(true)) {
    if (LittleFS.exists(FS_DEVICES_JSON))   LittleFS.remove(FS_DEVICES_JSON);
    if (LittleFS.exists(FS_DEVICES_BIN))    LittleFS.remove(FS_DEVICES_BIN);
    if (LittleFS.exists(FS_SCENES_JSON))    LittleFS.remove(FS_SCENES_JSON);
    if (LittleFS.exists(FS_SCHEDULES_JSON)) LittleFS.remove(FS_SCHEDULES_JSON);
    if (LittleFS.exists(FS_WALLPAPER))      LittleFS.remove(FS_WALLPAPER);
    LittleFS.end();
    Serial.println("[RESET] LittleFS data files deleted.");
  }

  lcd->fillScreen(0x0000);
  lcd->setTextColor(0x07E0, 0x0000); // green
  lcd->drawString("Factory Reset Complete!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 10);
  lcd->setFont(&lgfx::fonts::DejaVu12);
  lcd->setTextColor(0xFFFF, 0x0000);
  lcd->drawString("Rebooting...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15);
  delay(2000);
  ESP.restart();
}

SemaphoreHandle_t lvgl_mux = NULL;
SemaphoreHandle_t devices_mux = NULL;

// Network Task (Core 0)
void network_task(void *pvParameters) {
  // Re-enable watchdog for this task
  esp_task_wdt_add(NULL);
  
  while (1) {
    network_loop();
    
    // Handle pending OTA reboot here so it's not blocked by UI
    if (pending_ota_reboot && millis() - ota_reboot_time > 1000) {
      Serial.println("[MAIN] Rebooting for OTA update...");
      vTaskDelay(pdMS_TO_TICKS(100)); // give serial time to flush
      ESP.restart();
    }
    
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks on Core 0
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // SURGICAL WDT DISABLE
  // We temporarily disable it during Boot/WiFi connect, but we will re-enable it 
  // for the specific tasks later.
  esp_task_wdt_deinit();
  esp_log_level_set("task_wdt", ESP_LOG_NONE);

  Serial.println("\n--- SC01 Plus Booting [VER: FRTOS_V1] ---");

  lvgl_mux = xSemaphoreCreateMutex();
  devices_mux = xSemaphoreCreateMutex();
  
  network_load_persistence(); // MUST load devices before UI init
  loadScenes();               // Load scene configurations from LittleFS
  loadSchedules();            // Load schedule configurations from LittleFS

  // Load language translations from LittleFS (uses currentLang from NVS)
  lang_load(currentLang);

  hal_init();
  hal_haptic_init();

  // Factory Reset check — touch & hold screen for 3s during boot
  check_factory_reset();

  lv_init();
  size_t lv_buf_bytes = SCREEN_WIDTH * 20 * sizeof(lv_color_t);
  buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf1 || !buf2) {
    Serial.println("[LVGL] PSRAM alloc failed, falling back to internal heap");
    if (!buf1) buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf2) buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  Serial.printf("[LVGL] Draw buffers allocated (%u bytes each), free heap=%u\n", lv_buf_bytes, ESP.getFreeHeap());
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_WIDTH * 20);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  ui_init(); // Init UI first so objects exist for network callbacks
  network_setup();

  // Re-initialize WDT with a 5 second timeout (ESP-IDF v5 compatible)
  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = 5000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true,
  };
  esp_task_wdt_init(&twdt_config);
  
  // Register the loop task (UI task) to WDT
  esp_task_wdt_add(NULL);

  // Start Network Task on Core 0
  xTaskCreatePinnedToCore(
      network_task,
      "network_task",
      8192,
      NULL,
      1,
      NULL,
      0); // Core 0
}

/* =========================
   Loop (UI Task - Core 1)
   ========================= */

void loop() {

  static uint32_t last_tick = millis();
  uint32_t now = millis();

  if (now - last_tick >= 10) { // Relaxed to 10ms to allow other tasks more air
    lv_tick_inc(now - last_tick);
    last_tick = now;
    
    // Protect LVGL rendering with mutex
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(10)) == pdTRUE) {
      lv_timer_handler();
      xSemaphoreGive(lvgl_mux);
    }
  }

  // Network loop moved to Core 0

  // Bridge async web activity to LVGL safely on the UI loop thread.
  if (webActivityDetected) {
    webActivityDetected = false;
    lv_disp_trig_activity(NULL);
  }

  // Screensaver: only activate when STA is connected.
  // This avoids the panel going dark during captive-portal WiFi setup.
  uint32_t inactive_time = lv_disp_get_inactive_time(NULL);
  bool allowScreensaver = WiFi.status() == WL_CONNECTED;
  if (!screensaverActive && allowScreensaver && screensaverTimeoutMs > 0 &&
      (inactive_time > screensaverTimeoutMs)) {
    Serial.println("[SCREENSAVER] Activating!");
    hal_lcd_set_brightness(80); // dim backlight
    show_screensaver();
  }

  // Global Time/Date Update (runs every second — uses the global lastTimeUpdate
  // from wifi_manager.cpp; do NOT declare a static local here)
  if (now - lastTimeUpdate > 1000) {
    lastTimeUpdate = now;
    time_t now_ts;
    time(&now_ts);
    struct tm timeinfo;
    localtime_r(&now_ts, &timeinfo);

    if (timeinfo.tm_year > 100) { // Valid time received (after year 2000)
      if (use24HourFormat) {
        snprintf(currentTime, sizeof(currentTime), "%02d:%02d", timeinfo.tm_hour,
                 timeinfo.tm_min);
        currentMeridiem[0] = '\0';
      } else {
        int h = timeinfo.tm_hour % 12;
        if (h == 0)
          h = 12;
        // We use %02d to keep the string length at 5 for the flip clock's digit
        // extraction. AM/PM rides along in currentMeridiem instead of being
        // appended here — without it, 12-hour mode can't tell 09:30 morning
        // from 09:30 evening.
        snprintf(currentTime, sizeof(currentTime), "%02d:%02d", h,
                 timeinfo.tm_min);
        strcpy(currentMeridiem, timeinfo.tm_hour < 12 ? "AM" : "PM");
      }
      strftime(currentDate, sizeof(currentDate), "%a, %d %b %Y", &timeinfo);
    }

    // Check scheduled scene triggers (once per minute internally)
    checkSchedules();

    // Always update time/wifi/mqtt indicator in the header
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(10)) == pdTRUE) {
      ui_update_header();

      // Update active screens
      if (screensaverActive) {
        update_screensaver();
      } else {
        update_home_dashboard();
      }
      xSemaphoreGive(lvgl_mux);
    }
  }

  // Restore brightness when screensaver dismissed
  static bool wasSS = false;
  if (wasSS && !screensaverActive) {
    hal_lcd_set_brightness(255);
    request_network_state_sync(); // Let the network task do MQTT resubscribe work
    wasSS = false;
  }
  if (screensaverActive)
    wasSS = true;

  esp_task_wdt_reset(); // Feed WDT for UI Task
  vTaskDelay(pdMS_TO_TICKS(5)); // Yield
}