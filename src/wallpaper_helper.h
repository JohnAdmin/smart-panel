// wallpaper_helper.h
// Forward declares wallpaper_decode_to_buf() which is implemented in hal.cpp
// (where the LGFX class and lcd instance are accessible).
#pragma once
#include <lvgl.h>

// Decodes /wallpaper.jpg from LittleFS into `buf` (w*h RGB565 pixels).
// Returns true on success. Implemented in hal.cpp.
bool wallpaper_decode_to_buf(lv_color_t *buf, int w, int h);

// Generic JPEG/PNG/BMP -> RGB565 decoder for any LittleFS path.
// Used for wallpaper preset thumbnails on the Settings screen.
bool image_decode_path_to_buf(const char *path, lv_color_t *buf, int w, int h);
