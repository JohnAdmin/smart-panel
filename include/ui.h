#pragma once
#include <lvgl.h>

void ui_init();
void ui_update_header();
void ui_refresh_lang();
void ui_update_device_status(int index, bool state);
void rebuild_grid();
void show_screensaver();
void update_screensaver();
void ui_show_toast(const char *msg); // Q8: brief on-screen feedback

extern lv_obj_t *ui_ScreenMain;
extern lv_obj_t *ui_ScreenSettings;
extern lv_obj_t *ui_ScreenDevices;
extern lv_obj_t *ui_ScreenEditDevice;
extern lv_obj_t *ui_ScreenSaver;
extern lv_obj_t *ui_DimmerModal;

extern lv_obj_t *header_label_time;
extern lv_obj_t *grid_container;
extern lv_obj_t *device_tiles[];
extern lv_obj_t *device_icon_containers[];
extern lv_obj_t *device_icons[];
extern lv_obj_t *device_labels[];
extern lv_obj_t *device_status_labels[];
extern lv_obj_t *device_level_bars[];
