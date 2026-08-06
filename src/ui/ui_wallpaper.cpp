// ui_wallpaper.cpp
// Loads /wallpaper.jpg via wallpaper_decode_to_buf() (implemented in hal.cpp
// where the LGFX lcd instance is accessible) then sets it as the LVGL screen
// background image using the bg_img_src style property.

#include "ui_wallpaper.h"
#include "globals.h"
#include "wallpaper_helper.h"
#include <Arduino.h>
#include <lvgl.h>

// Persistent storage — must outlive ui_init()
static lv_color_t *canvas_buf = nullptr;
lv_img_dsc_t wallpaper_dsc;

lv_obj_t *ui_wallpaper_load(lv_obj_t *parent) {

  // 1. Allocate pixel buffer in PSRAM
  size_t buf_bytes = (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);
  canvas_buf = (lv_color_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM |
                                                             MALLOC_CAP_8BIT);
  if (!canvas_buf)
    canvas_buf = (lv_color_t *)malloc(buf_bytes);
  if (!canvas_buf) {
    Serial.println("[WP] OOM for pixel buffer");
    return nullptr;
  }
  memset(canvas_buf, 0x00, buf_bytes);

  // 2. Decode JPEG using lcd-parented sprite (in main.cpp)
  if (!wallpaper_decode_to_buf(canvas_buf, SCREEN_WIDTH, SCREEN_HEIGHT)) {
    Serial.println("[WP] Decode failed — using solid background");
    heap_caps_free(canvas_buf);
    canvas_buf = nullptr;
    return nullptr;
  }

  // 3. Build LVGL image descriptor
  memset(&wallpaper_dsc, 0, sizeof(wallpaper_dsc));
  wallpaper_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  wallpaper_dsc.header.always_zero = 0;
  wallpaper_dsc.header.reserved = 0;
  wallpaper_dsc.header.w = SCREEN_WIDTH;
  wallpaper_dsc.header.h = SCREEN_HEIGHT;
  wallpaper_dsc.data_size = (uint32_t)buf_bytes;
  wallpaper_dsc.data = (const uint8_t *)canvas_buf;

  // 4. Set as screen background image — drawn before all children
  lv_obj_set_style_bg_img_src(parent, &wallpaper_dsc, 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_img_opa(parent, LV_OPA_COVER, 0);
  lv_obj_invalidate(parent);

  Serial.println("[WP] Wallpaper applied as screen background");
  return parent;
}
