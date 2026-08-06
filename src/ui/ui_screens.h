#pragma once
#include "../../include/ui.h" // from the project root include folder
#include "../config.h"
#include <lvgl.h>

// --- Global Premium Colors ---
extern bool themeDark; // Pulls from globals.h without circular inclusion

#define CLR_BG_DEEP                                                            \
  (themeDark ? ((lv_color_t)LV_COLOR_MAKE(0x00, 0x00, 0x00))                   \
             : ((lv_color_t)LV_COLOR_MAKE(                                     \
                   0xF3, 0xF4, 0xF6))) // Pitch Black vs Light Gray BG

#define CLR_BG_MID                                                             \
  (themeDark ? ((lv_color_t)LV_COLOR_MAKE(0x1C, 0x1C, 0x1E))                   \
             : ((lv_color_t)LV_COLOR_MAKE(                                     \
                   0xFF, 0xFF, 0xFF))) // Solid Dark Card vs Pure White Card

#define CLR_PRIMARY                                                            \
  ((lv_color_t)LV_COLOR_MAKE(0xF5, 0x9E,                                       \
                             0x0B)) // Premium Amber for HomeKit theme

#define CLR_TEXT_TITLE                                                         \
  (themeDark                                                                   \
       ? ((lv_color_t)LV_COLOR_MAKE(0xFF, 0xFF, 0xFF))                         \
       : ((lv_color_t)LV_COLOR_MAKE(0x11, 0x18, 0x27))) // White vs Near Black

// DEPRECATED — nothing uses these two any more. Every surface they sat on
// (frosted header, pill button, glass card) is dark in both themes, so the
// theme-aware branch flipped the label to near black in light mode and left
// dark text on a dark fill. Use CLR_HEX_TEXT_HI / _MID from ui_helpers.h.
//
// R and G held equal so the panel's weak blue channel can't leave a green
// cast at low brightness — see the neutral ramp note in ui_helpers.h.
#define CLR_TEXT_DIM                                                           \
  (themeDark ? ((lv_color_t)LV_COLOR_MAKE(0xA6, 0xA6, 0xB0))                   \
             : ((lv_color_t)LV_COLOR_MAKE(0x74, 0x74,                          \
                                          0x80))) // Gray 400 vs Gray 500

#define CLR_GLASS_H CLR_BG_MID
#define CLR_GLASS_G CLR_BG_MID

// Icon name list for dropdowns
extern const char *icon_names;

// Global Fonts
extern const lv_font_t material_icons_font;
LV_FONT_DECLARE(lv_font_arial_120);

// UI Shared Globals
extern lv_obj_t *header_label_time;
extern lv_obj_t *header_label_wifi;
extern lv_obj_t *header_label_mqtt;
extern lv_obj_t *header_label_date;
extern lv_obj_t *header_label_count;
extern lv_obj_t *main_body_container;
extern lv_obj_t *set_container;
extern lv_obj_t *device_list_container;
extern lv_obj_t *ui_DimmerModal;

// Home Dashboard Globals
extern lv_obj_t *home_time_label;
extern lv_obj_t *home_date_label;
extern lv_obj_t *home_weather_label;

// Main Screen
void rebuild_grid();
void btn_toggle_event_cb(lv_event_t *e);
void update_home_dashboard();

// Dashboard visibility — the panel's own status entry is not a controllable
// device, so it is excluded from tiles, room tabs and every ON/total count.
bool ui_device_is_visible(int idx);
void ui_count_visible_devices(int *on_count, int *total);

// Settings
void build_settings_screen();
void btn_settings_event_cb(lv_event_t *e);
void btn_back_to_main_cb(lv_event_t *e);
void btn_goto_devices_cb(lv_event_t *e);
void btn_save_settings_cb(lv_event_t *e);

// Device Manager
void build_device_list_screen();
void build_edit_device_screen();
void btn_back_to_settings_cb(lv_event_t *e);

// Scene Manager
extern lv_obj_t *ui_ScreenScenes;
extern lv_obj_t *ui_ScreenEditScene;
void build_scene_list_screen();
void build_edit_scene_screen(int index); // -1 = new scene
void create_scene_tiles(lv_obj_t *parent); // builds scene cards on main screen

// Schedule Manager
extern lv_obj_t *ui_ScreenSchedules;
void build_schedule_list_screen();

// Sub-screen cleanup helpers (free LVGL heap before creating another)
void cleanup_scene_screen();
void cleanup_schedule_screen();
void cleanup_device_screen();

// Screensaver
void build_screensaver();
void update_screensaver();
void show_screensaver();
void invalidate_screensaver_build();
void screensaver_touch_cb(lv_event_t *e);
