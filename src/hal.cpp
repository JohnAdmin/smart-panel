#include "hal.h"
#include "config.h"
#include "globals.h"
#include "mqtt_manager.h"
#include "ui.h"
#include "ui/ui_screens.h"

// Haptic feedback global
bool hapticEnabled = true;

// =========================
// LGFX Driver Configuration
// =========================

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_FT5x06 _touch_instance;

public:
  LGFX(void) {
    // Bus
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = LGFX_BUS_FREQ_WRITE;
      cfg.pin_wr = LGFX_BUS_PIN_WR;
      cfg.pin_rd = LGFX_BUS_PIN_RD;
      cfg.pin_rs = LGFX_BUS_PIN_RS;
      cfg.pin_d0 = LGFX_BUS_PIN_D0;
      cfg.pin_d1 = LGFX_BUS_PIN_D1;
      cfg.pin_d2 = LGFX_BUS_PIN_D2;
      cfg.pin_d3 = LGFX_BUS_PIN_D3;
      cfg.pin_d4 = LGFX_BUS_PIN_D4;
      cfg.pin_d5 = LGFX_BUS_PIN_D5;
      cfg.pin_d6 = LGFX_BUS_PIN_D6;
      cfg.pin_d7 = LGFX_BUS_PIN_D7;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // Panel
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = LGFX_PANEL_PIN_CS;
      cfg.pin_rst = LGFX_PANEL_PIN_RST;
      cfg.panel_width = LGFX_PANEL_WIDTH;
      cfg.panel_height = LGFX_PANEL_HEIGHT;
      cfg.invert = true;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }

    // Backlight
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = LGFX_LIGHT_PIN_BL;
      cfg.freq = LGFX_LIGHT_FREQ;
      cfg.pwm_channel = LGFX_LIGHT_CHANNEL;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    // Touch
    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0;
      cfg.x_max = LGFX_PANEL_WIDTH - 1;
      cfg.y_min = 0;
      cfg.y_max = LGFX_PANEL_HEIGHT - 1;
      cfg.pin_int = LGFX_TOUCH_PIN_INT;
      cfg.i2c_port = LGFX_TOUCH_I2C_PORT;
      cfg.i2c_addr = LGFX_TOUCH_I2C_ADDR;
      cfg.pin_sda = LGFX_TOUCH_PIN_SDA;
      cfg.pin_scl = LGFX_TOUCH_PIN_SCL;
      cfg.freq = LGFX_TOUCH_FREQ;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

static LGFX lcd;

static volatile uint32_t g_last_tile_event_ms = 0;

void hal_note_tile_event() { g_last_tile_event_ms = millis(); }

static int hit_test_device_tile(uint16_t x, uint16_t y) {
  lv_area_t area;
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    lv_obj_t *obj = fav_tiles[i];
    if (obj && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_get_coords(obj, &area);
      if (x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2) {
        return i;
      }
    }
  }

  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    lv_obj_t *obj = device_tiles[i];
    if (obj && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_get_coords(obj, &area);
      if (x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2) {
        return i;
      }
    }
  }

  return -1;
}

void hal_init() {
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(255);
}

void hal_lcd_set_brightness(uint8_t brightness) {
  lcd.setBrightness(brightness);
}

lgfx::LGFX_Device* hal_get_lcd() {
  return &lcd;
}

// =========================
// Haptic Feedback
// =========================

#if HAPTIC_PIN >= 0
static volatile bool s_haptic_active = false;
static TimerHandle_t s_haptic_timer = NULL;

static void haptic_timer_cb(TimerHandle_t t) {
  ledcWrite(HAPTIC_PIN, 0); // stop
  s_haptic_active = false;
}

void hal_haptic_init() {
  ledcAttach(HAPTIC_PIN, HAPTIC_PWM_FREQ, 8);
  ledcWrite(HAPTIC_PIN, 0);
  s_haptic_timer = xTimerCreate("haptic", pdMS_TO_TICKS(HAPTIC_BUZZ_MS),
                                 pdFALSE, NULL, haptic_timer_cb);
}

void hal_haptic_buzz() {
  if (!hapticEnabled || s_haptic_active) return;
  s_haptic_active = true;
  ledcWrite(HAPTIC_PIN, 180); // ~70% duty
  if (s_haptic_timer) xTimerReset(s_haptic_timer, 0);
}
#else
void hal_haptic_init() {}
void hal_haptic_buzz() {}
#endif

// =========================
// LVGL Driver Callbacks
// =========================

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
  lcd.endWrite();

  lv_disp_flush_ready(disp_drv);
}

void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  uint16_t touchX, touchY;
  bool touched = lcd.getTouch(&touchX, &touchY);
  static bool was_pressed = false;
  static uint32_t last_touch_log_ms = 0;
  static uint16_t press_start_x = 0;
  static uint16_t press_start_y = 0;
  static uint32_t press_start_ms = 0;

  if (!touched) {
    if (was_pressed) {
      Serial.println("[TOUCH] Release");

      uint16_t dx = (touchX > press_start_x) ? (touchX - press_start_x)
                                             : (press_start_x - touchX);
      uint16_t dy = (touchY > press_start_y) ? (touchY - press_start_y)
                                             : (press_start_y - touchY);
      bool short_tap = (millis() - press_start_ms) < 450;
      bool low_movement = dx < 24 && dy < 24;
      bool lvgl_tile_seen = (millis() - g_last_tile_event_ms) < 300;

      if (!screensaverActive && short_tap && low_movement && !lvgl_tile_seen) {
        int idx = hit_test_device_tile(press_start_x, press_start_y);
        if (idx >= 0 && idx < deviceCount) {
          Serial.printf("[TOUCH-FALLBACK] hit idx=%d x=%u y=%u -> toggle\n", idx,
                        press_start_x, press_start_y);
          toggle_device(idx);
        }
      }
    }
    data->state = LV_INDEV_STATE_REL;
    was_pressed = false;
  } else {
    uint32_t now = millis();
    if (!was_pressed || now - last_touch_log_ms > 250) {
      Serial.printf("[TOUCH] Press x=%u y=%u screensaver=%d\n", touchX, touchY,
                    screensaverActive ? 1 : 0);
      last_touch_log_ms = now;
    }

    // Haptic buzz on initial press only
    if (!was_pressed) {
      was_pressed = true;
      press_start_x = touchX;
      press_start_y = touchY;
      press_start_ms = now;
      hal_haptic_buzz();
    }

    // If screensaver is active, wake up immediately.
    if (screensaverActive) {
      Serial.println("[TOUCH] Waking from screensaver");
      screensaverActive = false;
      lastTouchTime = millis();
      // Animate back to main screen
      if (ui_ScreenMain)
        lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
      // We still pass the touch event momentarily so LVGL knows an interaction
      // happened, but the screen transition has already been triggered.
    }

    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;
    lastTouchTime = millis(); 
    lv_disp_trig_activity(NULL); // Force LVGL idle timer reset on any drag/swipe
  }
}
