#include "ui.h"
#include "config.h"
#include "lang.h"
#include "scene.h"
#include "ui/ui_helpers.h"
#include "ui/ui_screens.h"
#include "ui/ui_wallpaper.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <WiFi.h>

// Material Symbols font generated from Google Fonts
extern const lv_font_t material_icons_font;

// --- Screen objects ---
lv_obj_t *ui_ScreenMain;
lv_obj_t *ui_ScreenSettings;
lv_obj_t *ui_ScreenDevices;
lv_obj_t *ui_ScreenEditDevice;
lv_obj_t *ui_ScreenSaver;
// lv_obj_t *ui_DimmerModal; // Already defined in ui_dimmer_modal.cpp

// --- Main Screen UI ---
lv_obj_t *header_label_time;
lv_obj_t *header_label_wifi;
lv_obj_t *header_label_mqtt;
lv_obj_t *header_label_date  = NULL;
lv_obj_t *header_label_count = NULL;
lv_obj_t *main_body_container;
lv_obj_t *device_tiles[MAX_DEVICES];
lv_obj_t *device_icon_containers[MAX_DEVICES];
lv_obj_t *device_icons[MAX_DEVICES];
lv_obj_t *device_labels[MAX_DEVICES];
lv_obj_t *device_status_labels[MAX_DEVICES];
lv_obj_t *device_level_bars[MAX_DEVICES]; // brightness bar; NULL if not dimmable

// Favorite tile mirrors
lv_obj_t *fav_tiles[MAX_DEVICES];
lv_obj_t *fav_icon_containers[MAX_DEVICES];
lv_obj_t *fav_icons[MAX_DEVICES];
lv_obj_t *fav_labels[MAX_DEVICES];
lv_obj_t *fav_status_labels[MAX_DEVICES];
lv_obj_t *fav_level_bars[MAX_DEVICES];

// --- Settings ---
lv_obj_t *set_container = NULL;
lv_obj_t *ta_ssid = NULL, *ta_pass = NULL, *ta_mqtt_srv = NULL,
         *ta_mqtt_usr = NULL, *ta_mqtt_pwd = NULL, *ta_city = NULL, *kb = NULL;

// --- Edit Device ---
lv_obj_t *ta_dev_name = NULL, *ta_dev_stat = NULL, *ta_dev_cmnd = NULL,
         *ta_dev_dimmer = NULL, *dd_icon = NULL, *kb_edit = NULL;

// Device list
lv_obj_t *device_list_container = NULL;

const char *icon_names = nullptr; // set at runtime from L(L_ICON_NAMES)

// Status-bar chip indicator (colour tracks the active-device count)
static lv_obj_t *s_hdr_count_dot = NULL;
// Clock block: AM/PM flag plus the rule that separates clock from date. Both
// shift depending on whether the meridiem is showing.
static lv_obj_t *s_hdr_meridiem = NULL;
static lv_obj_t *s_hdr_rule = NULL;

// Positions the rule and date for the current time format. 12-hour mode needs
// ~28 px more room after the clock for the AM/PM flag.
static void hdr_layout_time_block() {
  if (!s_hdr_rule || !header_label_date)
    return;
  const bool ampm = (currentMeridiem[0] != '\0');
  lv_obj_align(s_hdr_rule, LV_ALIGN_LEFT_MID, ampm ? 114 : 92, 0);
  lv_obj_align(header_label_date, LV_ALIGN_LEFT_MID, ampm ? 126 : 104, 1);
}

// Settings header labels (need refresh on language change)
static lv_obj_t *s_lbl_settings_title = NULL;
static lv_obj_t *s_lbl_btn_dev = NULL;
static lv_obj_t *s_lbl_btn_scenes = NULL;
static lv_obj_t *s_lbl_btn_sched = NULL;
static lv_obj_t *s_lbl_btn_save = NULL;

// ========================================================
//  UI INIT
// ========================================================
void ui_init() {
  // ─────────────────────────────────────
  //  MAIN SCREEN
  // ─────────────────────────────────────
  icon_names = L(L_ICON_NAMES);
  ui_ScreenMain = lv_obj_create(NULL);
  // Wallpaper canvas is drawn as a child; keep screen bg transparent when
  // wallpaper is used. Without wallpaper the bg stays solid black (set below
  // after the load attempt).
  lv_obj_set_style_bg_color(ui_ScreenMain, CLR_BG_DEEP, 0);
  lv_obj_set_style_bg_opa(ui_ScreenMain, LV_OPA_COVER, 0);

  // Use palette macros — Q4: avoid raw hex in ui.cpp
  lv_color_t text_dim = lv_color_hex(CLR_HEX_TEXT_MID);
  // Load wallpaper from LittleFS (decoded JPEG placed as background)
  // Must be called BEFORE header and tiles so it sits behind them.
  ui_wallpaper_load(ui_ScreenMain);

  // ── STATUS BAR ──
  // Left: clock + date, separated by a hairline rule.
  // Right: connectivity glyphs, the active-device chip, then settings.
  lv_obj_t *header = lv_obj_create(ui_ScreenMain);
  lv_obj_set_size(header, SCREEN_WIDTH, UI_HEADER_HEIGHT);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_80, 0); // frosted over the wallpaper
  lv_obj_set_style_border_color(header, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(header, LV_OPA_60, 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_shadow_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // Time — the anchor of the whole screen: largest type, highest contrast.
  // Amber is reserved for device state, so the clock stays neutral white.
  header_label_time = lv_label_create(header);
  lv_label_set_text(header_label_time, "00:00");
  lv_obj_set_style_text_font(header_label_time, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(header_label_time, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_set_style_text_letter_space(header_label_time, 1, 0);
  lv_obj_align(header_label_time, LV_ALIGN_LEFT_MID, 14, 0);

  // AM/PM — only present in 12-hour mode, sits tight to the clock
  s_hdr_meridiem = lv_label_create(header);
  lv_label_set_text(s_hdr_meridiem, currentMeridiem);
  lv_obj_set_style_text_font(s_hdr_meridiem, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_hdr_meridiem, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(s_hdr_meridiem, LV_ALIGN_LEFT_MID, 86, 3);

  // Hairline separating clock from date
  s_hdr_rule = ui_create_divider(header, 1);
  lv_obj_set_size(s_hdr_rule, 1, 18);

  // Date — secondary weight beside the clock
  header_label_date = lv_label_create(header);
  lv_label_set_text(header_label_date, currentDate);
  lv_obj_set_style_text_font(header_label_date, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(header_label_date, lv_color_hex(CLR_HEX_TEXT_MID), 0);

  // Rule and date shift right to make room for AM/PM; ui_update_header()
  // re-runs this whenever the time format changes.
  hdr_layout_time_block();

  // MQTT + WiFi glyphs — tinted by ui_update_header()
  header_label_mqtt = lv_label_create(header);
  lv_label_set_text(header_label_mqtt, LV_SYMBOL_UPLOAD);
  lv_obj_set_style_text_color(header_label_mqtt, text_dim, 0);
  lv_obj_align(header_label_mqtt, LV_ALIGN_RIGHT_MID, -170, 0);

  header_label_wifi = lv_label_create(header);
  lv_label_set_text(header_label_wifi, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(header_label_wifi, text_dim, 0);
  lv_obj_align(header_label_wifi, LV_ALIGN_RIGHT_MID, -144, 0);

  // Active-device chip — dot + "n/n ON" (sized for the 100-device maximum)
  lv_obj_t *count_chip = ui_create_chip(header, 86, 26);
  lv_obj_align(count_chip, LV_ALIGN_RIGHT_MID, -50, 0);

  s_hdr_count_dot = ui_create_dot(count_chip, 7, lv_color_hex(CLR_HEX_TEXT_LOW));
  lv_obj_align(s_hdr_count_dot, LV_ALIGN_LEFT_MID, 9, 0);

  header_label_count = lv_label_create(count_chip);
  lv_label_set_text(header_label_count, "0/0 ON");
  lv_obj_set_style_text_font(header_label_count, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(header_label_count, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_align(header_label_count, LV_ALIGN_LEFT_MID, 22, 0);

  // Settings — quiet square glass button; it should never outshine the tiles
  lv_obj_t *btn_set = ui_create_pill_btn(header, 36, 30,
                                          LV_SYMBOL_SETTINGS,
                                          lv_color_hex(CLR_HEX_TEXT_MID),
                                          btn_settings_event_cb);
  lv_obj_align(btn_set, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_set_style_bg_opa(btn_set, LV_OPA_60, 0);

  // ── MAIN BODY CONTAINER ──
  main_body_container = lv_obj_create(ui_ScreenMain);
  lv_obj_set_size(main_body_container, SCREEN_WIDTH, SCREEN_HEIGHT - UI_HEADER_HEIGHT);
  lv_obj_align(main_body_container, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(main_body_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(main_body_container, 0, 0);
  lv_obj_set_style_pad_all(main_body_container, 0, 0);
  lv_obj_clear_flag(main_body_container, LV_OBJ_FLAG_SCROLLABLE);
  rebuild_grid();

  // ─────────────────────────────────────
  //  SETTINGS SCREEN
  ui_ScreenSettings = lv_obj_create(NULL);
  if (wallpaper_dsc.data) {
    lv_obj_set_style_bg_img_src(ui_ScreenSettings, &wallpaper_dsc, 0);
    lv_obj_set_style_bg_opa(ui_ScreenSettings, LV_OPA_COVER, 0);

    // Scrim over the wallpaper. The dashboard wants the photo to show through
    // — a settings form does not: labels, inputs and dropdowns have to stay
    // readable no matter what image the user uploaded. Created before the
    // header and body so it sits behind both.
    lv_obj_t *scrim = lv_obj_create(ui_ScreenSettings);
    lv_obj_set_size(scrim, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(scrim, 0, 0);
    lv_obj_set_style_bg_color(scrim, lv_color_hex(CLR_HEX_SURFACE_0), 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_80, 0);
    lv_obj_set_style_border_width(scrim, 0, 0);
    lv_obj_set_style_radius(scrim, 0, 0);
    lv_obj_set_style_pad_all(scrim, 0, 0);
    lv_obj_clear_flag(scrim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_set_style_bg_color(ui_ScreenSettings, CLR_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(ui_ScreenSettings, LV_OPA_COVER, 0);
  }

  // Settings header — frosted glass (using shared helper)
  lv_obj_t *shdr = ui_create_frosted_header(ui_ScreenSettings, UI_SETTINGS_HDR_HEIGHT);

  // Back button — pill
  lv_obj_t *btn_back = ui_create_pill_btn(shdr, UI_PILL_BTN_W, UI_PILL_BTN_H,
                                           LV_SYMBOL_LEFT, CLR_PRIMARY,
                                           btn_back_to_main_cb);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 8, 0);

  // Devices button — pill.
  // Label colour comes from the token ramp, not CLR_TEXT_TITLE: the pill fill
  // is CLR_HEX_PILL_BG in both themes, so a theme-aware label flipped to near
  // black in light mode and left dark text on a dark pill. Same below.
  lv_obj_t *btn_dev = ui_create_pill_btn(shdr, 90, UI_PILL_BTN_H,
                                          "", lv_color_hex(CLR_HEX_TEXT_HI),
                                          btn_goto_devices_cb);
  s_lbl_btn_dev = lv_obj_get_child(btn_dev, 0);
  lv_label_set_text_fmt(s_lbl_btn_dev, LV_SYMBOL_LIST " %s", L(L_DEVICES));
  lv_obj_align(btn_dev, LV_ALIGN_RIGHT_MID, -290, 0);
  lv_obj_set_style_text_font(s_lbl_btn_dev, &lv_font_montserrat_12, 0);

  // Scenes button — pill
  lv_obj_t *btn_scenes = ui_create_pill_btn(shdr, 80, UI_PILL_BTN_H,
                                             "", lv_color_hex(CLR_HEX_TEXT_HI),
                                             [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      build_scene_list_screen();
      if (ui_ScreenScenes)
        lv_scr_load_anim(ui_ScreenScenes, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
    }
  });
  s_lbl_btn_scenes = lv_obj_get_child(btn_scenes, 0);
  lv_label_set_text_fmt(s_lbl_btn_scenes, LV_SYMBOL_VIDEO " %s", L(L_SCENES));
  lv_obj_align(btn_scenes, LV_ALIGN_RIGHT_MID, -200, 0);
  lv_obj_set_style_text_font(s_lbl_btn_scenes, &lv_font_montserrat_12, 0);

  // Schedules button — pill
  lv_obj_t *btn_sched = ui_create_pill_btn(shdr, 90, UI_PILL_BTN_H,
                                            "", lv_color_hex(CLR_HEX_TEXT_HI),
                                            [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      build_schedule_list_screen();
    }
  });
  s_lbl_btn_sched = lv_obj_get_child(btn_sched, 0);
  lv_label_set_text_fmt(s_lbl_btn_sched, LV_SYMBOL_LOOP " %s", L(L_SCHED));
  lv_obj_align(btn_sched, LV_ALIGN_RIGHT_MID, -108, 0);
  lv_obj_set_style_text_font(s_lbl_btn_sched, &lv_font_montserrat_12, 0);

  // Save button — accent
  lv_obj_t *btn_sv = ui_create_accent_btn(shdr, 90, UI_PILL_BTN_H, "",
                                          btn_save_settings_cb);
  lv_obj_align(btn_sv, LV_ALIGN_RIGHT_MID, -8, 0);
  s_lbl_btn_save = lv_obj_get_child(btn_sv, 0);
  lv_label_set_text_fmt(s_lbl_btn_save, LV_SYMBOL_OK " %s", L(L_SAVE));

  // Settings body
  set_container = lv_obj_create(ui_ScreenSettings);
  lv_obj_set_size(set_container, SCREEN_WIDTH, SCREEN_HEIGHT - UI_SETTINGS_HDR_HEIGHT);
  lv_obj_align(set_container, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(set_container, LV_OPA_TRANSP,
                          0); // Transparent for wallpaper
  lv_obj_set_style_border_width(set_container, 0, 0);
  build_settings_screen();

  // Init nullable screens
  ui_ScreenDevices = NULL;
  ui_ScreenEditDevice = NULL;
  ui_ScreenScenes = NULL;
  ui_ScreenEditScene = NULL;
  ui_ScreenSchedules = NULL;
  ui_ScreenSaver = NULL;
  ui_DimmerModal = NULL;

  lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

// ========================================================
//  HEADER UPDATE
// ========================================================
void ui_update_header() {
  if (header_label_time == NULL)
    return;
  lv_label_set_text(header_label_time, currentTime);
  if (s_hdr_meridiem &&
      strcmp(lv_label_get_text(s_hdr_meridiem), currentMeridiem) != 0) {
    lv_label_set_text(s_hdr_meridiem, currentMeridiem);
    hdr_layout_time_block(); // only when the format actually flips
  }
  // Connectivity glyphs are tinted, not recoloured with markup — muted when
  // healthy so they read as ambient status, loud only when something is wrong.
  if (isWifiConnected) {
    lv_color_t wc;
    if (wifiRssi >= -65)      wc = lv_color_hex(CLR_HEX_OK);      // good
    else if (wifiRssi >= -75) wc = lv_color_hex(CLR_HEX_ACCENT);  // fair
    else                      wc = lv_color_hex(CLR_HEX_ACCENT_HI); // weak
    lv_obj_set_style_text_color(header_label_wifi, wc, 0);
  } else {
    lv_obj_set_style_text_color(header_label_wifi, lv_color_hex(CLR_HEX_DANGER), 0);
  }
  lv_obj_set_style_text_color(header_label_mqtt,
                              isMqttConnected ? lv_color_hex(CLR_HEX_OK)
                                              : lv_color_hex(CLR_HEX_DANGER), 0);

  // Date in header
  if (header_label_date) {
    lv_label_set_text(header_label_date, currentDate);
  }
  // Active device count in header (excludes the panel's own status entry)
  if (header_label_count) {
    int cnt = 0, total = 0;
    ui_count_visible_devices(&cnt, &total);
    char cbuf[24];
    snprintf(cbuf, sizeof(cbuf), "%d/%d ON", cnt, total);
    lv_label_set_text(header_label_count, cbuf);
    lv_color_t cc = cnt > 0 ? lv_color_hex(CLR_HEX_ACCENT)
                            : lv_color_hex(CLR_HEX_TEXT_LOW);
    lv_obj_set_style_text_color(header_label_count,
                                cnt > 0 ? lv_color_hex(CLR_HEX_TEXT_HI)
                                        : lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    if (s_hdr_count_dot) lv_obj_set_style_bg_color(s_hdr_count_dot, cc, 0);
  }
}

// ========================================================
//  REFRESH UI LABELS ON LANGUAGE CHANGE
// ========================================================
void ui_refresh_lang() {
  icon_names = L(L_ICON_NAMES);
  if (s_lbl_btn_dev)
    lv_label_set_text_fmt(s_lbl_btn_dev, LV_SYMBOL_LIST " %s", L(L_DEVICES));
  if (s_lbl_btn_scenes)
    lv_label_set_text_fmt(s_lbl_btn_scenes, LV_SYMBOL_VIDEO " %s", L(L_SCENES));
  if (s_lbl_btn_sched)
    lv_label_set_text_fmt(s_lbl_btn_sched, LV_SYMBOL_LOOP " %s", L(L_SCHED));
  if (s_lbl_btn_save)
    lv_label_set_text_fmt(s_lbl_btn_save, LV_SYMBOL_OK " %s", L(L_SAVE));
  rebuild_grid();
}

// ========================================================
//  TOAST NOTIFICATION   (Q8)
// ========================================================
static lv_obj_t *s_toast_obj = NULL;
static lv_timer_t *s_toast_tmr = NULL;

static void _toast_timer_cb(lv_timer_t *tmr) {
  (void)tmr;
  if (s_toast_obj) {
    lv_obj_del(s_toast_obj);
    s_toast_obj = NULL;
  }
  if (s_toast_tmr) {
    lv_timer_del(s_toast_tmr);
    s_toast_tmr = NULL;
  }
}

void ui_show_toast(const char *msg) {
  // Delete any existing toast first
  if (s_toast_obj) {
    lv_obj_del(s_toast_obj);
    s_toast_obj = NULL;
  }
  if (s_toast_tmr) {
    lv_timer_del(s_toast_tmr);
    s_toast_tmr = NULL;
  }

  // Create toast pill on top of the active screen
  lv_obj_t *scr = lv_scr_act();
  s_toast_obj = lv_obj_create(scr);
  lv_obj_set_size(s_toast_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(s_toast_obj, 8, 0);
  lv_obj_set_style_pad_hor(s_toast_obj, 18, 0);
  lv_obj_align(s_toast_obj, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_bg_color(s_toast_obj, lv_color_hex(CLR_HEX_SURFACE_2), 0);
  lv_obj_set_style_bg_opa(s_toast_obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(s_toast_obj, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(s_toast_obj, 1, 0);
  lv_obj_set_style_border_opa(s_toast_obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_toast_obj, LV_RADIUS_CIRCLE, 0);
  // Depth comes from a black drop shadow, not a coloured glow — the accent
  // stays reserved for device state.
  lv_obj_set_style_shadow_color(s_toast_obj, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(s_toast_obj, 18, 0);
  lv_obj_set_style_shadow_ofs_y(s_toast_obj, 4, 0);
  lv_obj_set_style_shadow_opa(s_toast_obj, LV_OPA_50, 0);
  lv_obj_clear_flag(s_toast_obj, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl = lv_label_create(s_toast_obj);
  lv_label_set_text(lbl, msg);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);

  // Auto-dismiss after 2 seconds
  s_toast_tmr = lv_timer_create(_toast_timer_cb, UI_TOAST_DISMISS_MS, NULL);
  lv_timer_set_repeat_count(s_toast_tmr, 1);
}

// ========================================================
//  ICON SYMBOL MAPPING — Material Symbols Rounded (Filled)
// ========================================================
const char *getIconSymbol(int icon_type) {
  switch (icon_type) {
  case ICON_LAMP:
    return "\xEE\x83\xB0"; // E0F0 lightbulb
  case ICON_FAN:
    return "\xEF\x85\xA8"; // F168 mode_fan (ceiling fan)
  case ICON_SWITCH:
    return "\xEE\xA2\xAC"; // E8AC power_settings_new
  case ICON_PLUG:
    return "\xEE\xA8\x8B"; // EA0B bolt (lightning)
  case ICON_THERMOSTAT:
    return "\xEE\x87\xBF"; // E1FF device_thermostat
  case ICON_LOCK:
    return "\xEE\xA2\x97"; // E897 lock
  case ICON_TV:
    return "\xEE\x8C\xB3"; // E333 tv
  case ICON_GARAGE:
    return "\xEE\xBF\x92"; // EFD2 garage_home
  case ICON_STRIP:
    return "\xEE\x94\x98"; // E518 light_mode (sun/ambient)
  case ICON_GENERIC:
  default:
    return "\xEE\xA6\xB0"; // E9B0 grid_view
  }
}
