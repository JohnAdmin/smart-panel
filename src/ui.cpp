#include "ui.h"
#include "config.h"
#include "lang.h"
#include "scene.h"
#include "ui/ui_helpers.h"
#include "ui/ui_nav_rail.h"
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

// Header title — the panel name today, the page name once the rail's other
// destinations get their own headers (Phase 3).
lv_obj_t *header_label_title = NULL;
// Status-bar chip indicator (colour tracks the active-device count)
static lv_obj_t *s_hdr_count_dot = NULL;
// AM/PM flag. It lives in the header's right-hand flex row, so it can be shown
// and hidden without anything around it needing to be re-aligned — the old
// 44 px header positioned every element absolutely and had to shuffle the rule
// and date by hand whenever the time format changed.
static lv_obj_t *s_hdr_meridiem = NULL;
// Signal strength in dBm, beside the Wi-Fi glyph
static lv_obj_t *s_hdr_rssi = NULL;

// Settings header labels (need refresh on language change)
static lv_obj_t *s_lbl_settings_title = NULL;
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

  // ── NAV RAIL ──
  // Built before the header so the header can sit flush against it. It owns
  // the left 52 px of every screen that carries one.
  ui_nav_rail_create(ui_ScreenMain, UI_NAV_HOME);

  // ── STATUS BAR ──
  // A 38 px strip filling the width right of the rail.
  //   Left  — the panel name.
  //   Right — one flex row: device count, Wi-Fi + RSSI, MQTT, clock.
  // The right side is a flex row rather than a set of absolute offsets because
  // the AM/PM flag comes and goes with the time format; flex re-packs the row
  // on its own instead of every neighbour needing a hand-tuned x.
  //
  // The date came off the header when it shrank from 44 px to 38 px: at this
  // height only one line fits, and clock + date + status will not share it.
  // The screensaver still shows the full date, and Phase 2's home screen
  // brings it back into the body.
  lv_obj_t *header = lv_obj_create(ui_ScreenMain);
  lv_obj_set_size(header, UI_CONTENT_W, UI_HEADER_HEIGHT);
  lv_obj_align(header, LV_ALIGN_TOP_RIGHT, 0, 0);
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

  // Panel name — the only thing on the left of the bar
  header_label_title = lv_label_create(header);
  lv_label_set_text(header_label_title, panelTitle);
  lv_obj_set_style_text_font(header_label_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(header_label_title, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_label_set_long_mode(header_label_title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(header_label_title, UI_HDR_TITLE_W);
  lv_obj_align(header_label_title, LV_ALIGN_LEFT_MID, 12, 0);

  // Right-hand status row, packed against the right edge by flex so the AM/PM
  // flag can come and go without any neighbour needing a hand-tuned x.
  //
  // Fixed width, not LV_SIZE_CONTENT: children are clipped to their parent's
  // box, so if the shrink-wrap comes out short the row loses whichever items
  // are furthest from the END edge — the count chip and the Wi-Fi glyph — and
  // does it silently. Height is one less than the header so the 1 px bottom
  // border isn't inside the row's content area.
  lv_obj_t *hdr_right = lv_obj_create(header);
  lv_obj_set_size(hdr_right, UI_HDR_STATUS_W, UI_HEADER_HEIGHT - 1);
  lv_obj_align(hdr_right, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_set_style_bg_opa(hdr_right, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(hdr_right, 0, 0);
  lv_obj_set_style_pad_all(hdr_right, 0, 0);
  lv_obj_set_style_pad_column(hdr_right, 7, 0);
  lv_obj_clear_flag(hdr_right, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(hdr_right, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hdr_right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  // Active-device chip — dot + "n/n" (sized for the 100-device maximum)
  lv_obj_t *count_chip = ui_create_chip(hdr_right, 60, 22);
  s_hdr_count_dot = ui_create_dot(count_chip, 6, lv_color_hex(CLR_HEX_TEXT_LOW));
  lv_obj_align(s_hdr_count_dot, LV_ALIGN_LEFT_MID, 8, 0);

  header_label_count = lv_label_create(count_chip);
  lv_label_set_text(header_label_count, "0/0");
  lv_obj_set_style_text_font(header_label_count, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(header_label_count, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_align(header_label_count, LV_ALIGN_LEFT_MID, 19, 0);

  // WiFi glyph + dBm — tinted by ui_update_header(). The font is set
  // explicitly rather than inherited: LV_FONT_DEFAULT is the only build where
  // these symbols exist, and a future change to it should not quietly turn
  // these two into boxes.
  header_label_wifi = lv_label_create(hdr_right);
  lv_label_set_text(header_label_wifi, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(header_label_wifi, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(header_label_wifi, text_dim, 0);

  s_hdr_rssi = lv_label_create(hdr_right);
  lv_label_set_text(s_hdr_rssi, "--");
  lv_obj_set_style_text_font(s_hdr_rssi, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_hdr_rssi, lv_color_hex(CLR_HEX_TEXT_LOW), 0);

  header_label_mqtt = lv_label_create(hdr_right);
  lv_label_set_text(header_label_mqtt, LV_SYMBOL_UPLOAD);
  lv_obj_set_style_text_font(header_label_mqtt, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(header_label_mqtt, text_dim, 0);

  // Clock. It is no longer the anchor of the screen — the screensaver, which
  // is what the panel shows most of the time, carries the large clock.
  header_label_time = lv_label_create(hdr_right);
  lv_label_set_text(header_label_time, "00:00");
  lv_obj_set_style_text_font(header_label_time, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(header_label_time, lv_color_hex(CLR_HEX_TEXT_HI), 0);

  // AM/PM — only present in 12-hour mode; hidden (not just blank) in 24-hour
  // so flex reclaims its column gap too.
  s_hdr_meridiem = lv_label_create(hdr_right);
  lv_label_set_text(s_hdr_meridiem, currentMeridiem);
  lv_obj_set_style_text_font(s_hdr_meridiem, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_hdr_meridiem, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  if (currentMeridiem[0] == '\0')
    lv_obj_add_flag(s_hdr_meridiem, LV_OBJ_FLAG_HIDDEN);

  // The date label no longer exists on this screen. ui_update_header() already
  // guards on it, so leaving it NULL is the whole change.
  header_label_date = NULL;


  // ── MAIN BODY CONTAINER ──
  main_body_container = lv_obj_create(ui_ScreenMain);
  lv_obj_set_size(main_body_container, UI_CONTENT_W, UI_CONTENT_H);
  lv_obj_align(main_body_container, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
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

  // Rail — Settings is one of its four destinations, so it no longer needs a
  // back button of its own.
  ui_nav_rail_create(ui_ScreenSettings, UI_NAV_SETTINGS);

  // Settings header — same 38 px bar as Home: title left, Save right. The row
  // of Devices/Scenes/Schedules pills that used to live here is gone; those
  // are rail destinations and sidebar tabs now.
  lv_obj_t *shdr = ui_create_frosted_header(ui_ScreenSettings, UI_HEADER_HEIGHT);

  lv_obj_t *shdr_title = lv_label_create(shdr);
  lv_label_set_text(shdr_title, L(L_SETTINGS));
  lv_obj_set_style_text_font(shdr_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(shdr_title, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_align(shdr_title, LV_ALIGN_LEFT_MID, 12, 0);
  s_lbl_settings_title = shdr_title;

  // Save button — accent
  lv_obj_t *btn_sv = ui_create_accent_btn(shdr, 84, 26, "",
                                          btn_save_settings_cb);
  lv_obj_align(btn_sv, LV_ALIGN_RIGHT_MID, -10, 0);
  s_lbl_btn_save = lv_obj_get_child(btn_sv, 0);
  lv_label_set_text_fmt(s_lbl_btn_save, LV_SYMBOL_OK " %s", L(L_SAVE));

  // Tab sidebar, then the content area it drives
  build_settings_sidebar(ui_ScreenSettings);

  set_container = lv_obj_create(ui_ScreenSettings);
  lv_obj_set_size(set_container, UI_SETTINGS_W, UI_CONTENT_H);
  lv_obj_align(set_container, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_opa(set_container, LV_OPA_TRANSP,
                          0); // Transparent for wallpaper
  lv_obj_set_style_border_width(set_container, 0, 0);
  lv_obj_set_style_pad_all(set_container, 0, 0);
  build_settings_screen();

  // Init nullable screens
  ui_ScreenDevices = NULL;
  ui_ScreenEditDevice = NULL;
  ui_ScreenScenes = NULL;
  ui_ScreenEditScene = NULL;
  ui_ScreenSaver = NULL;
  ui_DimmerModal = NULL;

  lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

// ========================================================
//  HEADER TITLE
// ========================================================
// The header names whichever view main_body_container is showing — the panel
// on Home, the room inside a room. Views call this as they build.
void ui_set_header_title(const char *title) {
  if (!header_label_title || !title) return;
  if (strcmp(lv_label_get_text(header_label_title), title) == 0) return;
  lv_label_set_text(header_label_title, title);
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
    // Hide rather than blank it: an empty label still occupies a flex column
    // gap, which would leave a 7 px hole beside the clock in 24-hour mode.
    if (currentMeridiem[0] == '\0')
      lv_obj_add_flag(s_hdr_meridiem, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_clear_flag(s_hdr_meridiem, LV_OBJ_FLAG_HIDDEN);
  }
  // Connectivity glyphs are tinted, not recoloured with markup — muted when
  // healthy so they read as ambient status, loud only when something is wrong.
  if (isWifiConnected) {
    lv_color_t wc;
    if (wifiRssi >= -65)      wc = lv_color_hex(CLR_HEX_OK);      // good
    else if (wifiRssi >= -75) wc = lv_color_hex(CLR_HEX_ACCENT);  // fair
    else                      wc = lv_color_hex(CLR_HEX_ACCENT_HI); // weak
    lv_obj_set_style_text_color(header_label_wifi, wc, 0);
    if (s_hdr_rssi) lv_label_set_text_fmt(s_hdr_rssi, "%d", wifiRssi);
  } else {
    lv_obj_set_style_text_color(header_label_wifi, lv_color_hex(CLR_HEX_DANGER), 0);
    if (s_hdr_rssi) lv_label_set_text(s_hdr_rssi, "--");
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
    // "ON" dropped with the narrower header — the amber dot beside the numbers
    // already says which of them is the live count.
    snprintf(cbuf, sizeof(cbuf), "%d/%d", cnt, total);
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
  // The panel name reaches the UI in two places now — the header title and the
  // rail's monogram — and the web portal can change it without a reboot.
  if (header_label_title)
    lv_label_set_text(header_label_title, panelTitle);
  ui_nav_rail_refresh_logo();
  if (s_lbl_settings_title)
    lv_label_set_text(s_lbl_settings_title, L(L_SETTINGS));
  // The settings sidebar is built once with its screen, so it does not get
  // re-created the way the tab bodies do.
  ui_settings_refresh_chrome();
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
