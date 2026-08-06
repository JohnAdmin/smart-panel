#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>

// Hardware Initialization
void hal_init();
void hal_lcd_set_brightness(uint8_t brightness);

// LVGL Display & Input Driver Setup
void hal_lvgl_init();

// Factory Reset (touch & hold during boot)
void hal_check_factory_reset();

// Wallpaper Decode (uses LGFX sprite internally)
bool wallpaper_decode_to_buf(lv_color_t *buf, int w, int h);

// Haptic Feedback
void hal_haptic_init();
void hal_haptic_buzz();

// Access to the raw LCD driver (e.g., for creating sprites via LovyanGFX)
lgfx::LGFX_Device* hal_get_lcd();

// LVGL Driver Callbacks
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

// Touch-to-tile fallback coordination
void hal_note_tile_event();
