#pragma once
// ui_wallpaper.h
// Loads /wallpaper.jpg from LittleFS, decodes it with TJpgDec,
// and returns an LVGL image object to use as screen background.

#include <lvgl.h>

extern lv_img_dsc_t wallpaper_dsc;

// Decode the wallpaper from LittleFS and draw it as a child of parent.
// Returns the lv_obj_t* image, or NULL if no wallpaper exists.
// Call this ONCE during ui_init(), BEFORE adding the header/tiles on top.
lv_obj_t *ui_wallpaper_load(lv_obj_t *parent);
