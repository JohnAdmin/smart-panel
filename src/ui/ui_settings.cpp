#include "../config.h"
// globals.h is transitively included via wifi_manager.h below.
#include "../lang.h"
#include "../wallpaper_helper.h"
#include "../wifi_manager.h"
#include "extra/libs/qrcode/lv_qrcode.h"
#include "ui_helpers.h"
#include "ui_nav_rail.h" // ui_nav_rail_refresh_logo() after a name change
#include "ui_screens.h"
#include <LittleFS.h>
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "../hal.h"

// These textarea/keyboard widgets are defined in ui.cpp (global scope) and
// shared across the settings callbacks below. Externs are correct here.
extern lv_obj_t *ta_ssid;
extern lv_obj_t *ta_pass;
extern lv_obj_t *ta_mqtt_srv;
extern lv_obj_t *ta_mqtt_usr;
extern lv_obj_t *ta_mqtt_pwd;
extern lv_obj_t *ta_city;
extern lv_obj_t *kb;

// Forward declare brightness slider callback
void brightness_slider_event_cb(lv_event_t *e);

// ── Settings tabs ───────────────────────────────────────
// Four destinations down a 116 px sidebar, replacing the row of pills that
// used to share the header with the back button. The header only has room for
// a title and Save at 38 px, and the pills were how Devices, Scenes and
// Schedules were reached — those are rail destinations and sidebar rows now.
enum SettingsTab {
  SET_TAB_PORTAL = 0,
  SET_TAB_DISPLAY,
  SET_TAB_DEVICES,
  SET_TAB_SYSTEM,
  SET_TAB_COUNT
};
static int s_settings_tab = SET_TAB_PORTAL;
static lv_obj_t *s_tab_btns[SET_TAB_COUNT] = {NULL};
static lv_obj_t *s_tab_lbls[SET_TAB_COUNT] = {NULL};

static const char *settings_tab_name(int tab) {
  switch (tab) {
  case SET_TAB_PORTAL:  return L(L_WEB_PORTAL);
  case SET_TAB_DISPLAY: return L(L_DISPLAY);
  case SET_TAB_DEVICES: return L(L_DEVICES);
  default:              return L(L_SYSTEM);
  }
}

// Repaints the selected row. Same treatment as the nav rail: an accent tint
// rather than a solid fill, so a selected tab never reads like a lit device.
static void settings_tab_paint() {
  for (int i = 0; i < SET_TAB_COUNT; i++) {
    if (!s_tab_btns[i]) continue;
    const bool on = (i == s_settings_tab);
    lv_obj_set_style_bg_opa(s_tab_btns[i], on ? (lv_opa_t)31 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(
        s_tab_lbls[i],
        lv_color_hex(on ? CLR_HEX_ACCENT : CLR_HEX_TEXT_LOW), 0);
  }
}

static void settings_tab_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  s_settings_tab = (int)(intptr_t)lv_event_get_user_data(e);
  settings_tab_paint();
  build_settings_screen();
}

// The sidebar is built once with the screen, so unlike the tab bodies it is
// not re-created on a language switch and its labels would keep whatever
// language was active at boot.
void ui_settings_refresh_chrome() {
  for (int i = 0; i < SET_TAB_COUNT; i++)
    if (s_tab_lbls[i]) {
      static const char *icons[SET_TAB_COUNT] = {
          LV_SYMBOL_WIFI, LV_SYMBOL_IMAGE, LV_SYMBOL_LIST, LV_SYMBOL_SETTINGS};
      lv_label_set_text_fmt(s_tab_lbls[i], "%s  %s", icons[i],
                            settings_tab_name(i));
    }
}

void build_settings_sidebar(lv_obj_t *screen) {
  lv_obj_t *bar = lv_obj_create(screen);
  lv_obj_set_size(bar, UI_SIDEBAR_W, UI_CONTENT_H);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, UI_RAIL_W, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_70, 0);
  lv_obj_set_style_border_color(bar, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(bar, LV_OPA_60, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_shadow_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 8, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  static const char *icons[SET_TAB_COUNT] = {
      LV_SYMBOL_WIFI,     // portal — reached over the network
      LV_SYMBOL_IMAGE,    // display
      LV_SYMBOL_LIST,     // devices
      LV_SYMBOL_SETTINGS, // system
  };

  for (int i = 0; i < SET_TAB_COUNT; i++) {
    lv_obj_t *btn = lv_btn_create(bar);
    lv_obj_set_size(btn, UI_SIDEBAR_W - 16, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, i * 34);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_HEX_ACCENT), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, (lv_opa_t)56, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, settings_tab_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)i);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text_fmt(lbl, "%s  %s", icons[i], settings_tab_name(i));
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, UI_SIDEBAR_W - 26);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

    s_tab_btns[i] = btn;
    s_tab_lbls[i] = lbl;
  }
  settings_tab_paint();
}

// ── Wallpaper preset thumbnail cache ─────────────────────
// Decoded once on first Settings open and reused across rebuilds. Buffers
// live in PSRAM so they don't pressure internal RAM.
#define WP_THUMB_W 60
#define WP_THUMB_H 40
static lv_color_t *wp_thumb_buf[3] = {nullptr, nullptr, nullptr};
static lv_img_dsc_t wp_thumb_dsc[3] = {};
static bool wp_thumb_decoded[3] = {false, false, false};

static void wp_build_thumb(int idx) {
  if (idx < 0 || idx >= 3) return;
  if (wp_thumb_decoded[idx]) return;
  size_t bytes = (size_t)WP_THUMB_W * WP_THUMB_H * sizeof(lv_color_t);
  if (!wp_thumb_buf[idx]) {
    wp_thumb_buf[idx] = (lv_color_t *)heap_caps_malloc(
        bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!wp_thumb_buf[idx]) wp_thumb_buf[idx] = (lv_color_t *)malloc(bytes);
  }
  if (!wp_thumb_buf[idx]) return;
  memset(wp_thumb_buf[idx], 0, bytes);
  char path[24];
  snprintf(path, sizeof(path), "/default%d.jpg", idx + 1);
  bool ok = LittleFS.exists(path) &&
            image_decode_path_to_buf(path, wp_thumb_buf[idx], WP_THUMB_W,
                                     WP_THUMB_H);
  wp_thumb_dsc[idx].header.cf = LV_IMG_CF_TRUE_COLOR;
  wp_thumb_dsc[idx].header.always_zero = 0;
  wp_thumb_dsc[idx].header.reserved = 0;
  wp_thumb_dsc[idx].header.w = WP_THUMB_W;
  wp_thumb_dsc[idx].header.h = WP_THUMB_H;
  wp_thumb_dsc[idx].data_size = bytes;
  wp_thumb_dsc[idx].data = (const uint8_t *)wp_thumb_buf[idx];
  wp_thumb_decoded[idx] = ok;
  Serial.printf("[UI] WP thumb %d decode: %s\n", idx + 1, ok ? "OK" : "FAIL");
}

// Handle Keyboard focus
void ta_wifi_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  if (code == LV_EVENT_FOCUSED) {
    if (kb) {
      lv_keyboard_set_textarea(kb, ta);
      lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (code == LV_EVENT_DEFOCUSED) {
    if (kb) {
      lv_keyboard_set_textarea(kb, NULL);
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void build_wifi_setup_screen() {
  if (kb) {
    lv_obj_del(kb);
    kb = NULL;
  }
  lv_obj_clean(set_container);

  const int card_w = UI_SETTINGS_W - 20;
  lv_obj_t *card = ui_create_glass_card(set_container, card_w, 190);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_pad_all(card, 12, 0);

  lv_obj_t *lbl_title = lv_label_create(card);
  lv_label_set_text_fmt(lbl_title, LV_SYMBOL_WIFI "  %s", L(L_WIFI_SETUP));
  lv_obj_set_style_text_color(lbl_title, CLR_PRIMARY, 0);
  lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_ssid = ui_create_textarea(card, card_w - 24, L(L_WIFI_SSID),
                               wifi_ssid.c_str(), ta_wifi_event_cb);
  lv_obj_align(ta_ssid, LV_ALIGN_TOP_MID, 0, 30);

  ta_pass = ui_create_textarea(card, card_w - 24, L(L_WIFI_PASSWORD),
                               wifi_pass.c_str(), ta_wifi_event_cb);
  lv_obj_align(ta_pass, LV_ALIGN_TOP_MID, 0, 82);
  lv_textarea_set_password_mode(ta_pass, false);

  lv_obj_t *hint = lv_label_create(card);
  lv_label_set_text(hint, L(L_WEB_SAVE_RESTART));
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);

  // Hidden placeholders
  ta_mqtt_srv = NULL;
  ta_mqtt_usr = NULL;
  ta_mqtt_pwd = NULL;
  ta_city = NULL;

  // The keyboard belongs to the screen, not to the 312 px content area — it
  // needs the full width to be typeable, and it has to draw over the rail and
  // the sidebar rather than be squeezed between them. build_settings_screen()
  // deletes it, which is why it is tracked in the shared `kb` pointer.
  kb = lv_keyboard_create(lv_obj_get_screen(set_container));
  ui_style_keyboard(kb);
  lv_obj_set_size(kb, SCREEN_WIDTH, 136);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

void btn_wifi_config_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    build_wifi_setup_screen();
  }
}

// ── Content building blocks ─────────────────────────────
// The content area is 312 px wide now that the rail and the tab sidebar have
// taken their columns, so settings are a scrolling column of rows rather than
// the 444 px cards that fitted when this screen owned the whole display.

static lv_obj_t *settings_page(lv_obj_t *parent) {
  lv_obj_t *page = lv_obj_create(parent);
  lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_pad_all(page, 10, 0);
  lv_obj_set_style_pad_row(page, 6, 0);
  lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_scroll_dir(page, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_AUTO);
  ui_style_scrollbar(page);
  return page;
}

// One labelled setting. Returns the row so the caller can hang its control off
// the right edge with LV_ALIGN_RIGHT_MID.
static lv_obj_t *settings_row(lv_obj_t *page, const char *text, int h = 36) {
  lv_obj_t *row = lv_obj_create(page);
  lv_obj_set_size(row, LV_PCT(100), h);
  lv_obj_set_style_bg_color(row, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_dir(row, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_90, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(row, 10, 0);
  lv_obj_set_style_shadow_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 8, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  if (text) {
    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, 100); // leaves 168 px + an 8 px gap for the control
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
  }
  return row;
}

// Read-only status line: coloured dot, name, value.
static void settings_status_row(lv_obj_t *page, const char *name,
                                const char *value, lv_color_t dot_color) {
  lv_obj_t *row = settings_row(page, NULL);
  lv_obj_t *dot = ui_create_dot(row, 7, dot_color);
  lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *lbl = lv_label_create(row);
  lv_label_set_text(lbl, name);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 14, 0);

  lv_obj_t *val = lv_label_create(row);
  lv_label_set_text(val, value);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(val, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_label_set_long_mode(val, LV_LABEL_LONG_DOT);
  lv_obj_set_width(val, 150);
  lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
}

// Used / total with a fill bar. The bar turns amber then red as it fills:
// these are the numbers you look at when the panel has started misbehaving,
// and "82 %" means nothing until you can see it against the whole.
static void settings_meter_row(lv_obj_t *page, const char *name, size_t used,
                               size_t total, bool as_kb) {
  lv_obj_t *row = settings_row(page, NULL, 46);
  const int pct = total ? (int)(((uint64_t)used * 100) / total) : 0;

  lv_obj_t *lbl = lv_label_create(row);
  lv_label_set_text(lbl, name);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *val = lv_label_create(row);
  if (as_kb)
    lv_label_set_text_fmt(val, "%u / %u KB  %d%%", (unsigned)(used / 1024),
                          (unsigned)(total / 1024), pct);
  else
    lv_label_set_text_fmt(val, "%.1f / %.1f MB  %d%%", used / 1048576.0f,
                          total / 1048576.0f, pct);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(val, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_align(val, LV_ALIGN_TOP_RIGHT, 0, 0);

  lv_obj_t *bar = lv_bar_create(row);
  lv_obj_set_size(bar, UI_SETTINGS_W - 40, 6);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar,
                            lv_color_hex(pct >= 90   ? CLR_HEX_DANGER
                                         : pct >= 80 ? CLR_HEX_ACCENT_HI
                                                     : CLR_HEX_ACCENT),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
}

// A full-width row that acts as a button and opens something else.
static void settings_link_row(lv_obj_t *page, const char *glyph,
                              const char *text, lv_event_cb_t cb) {
  lv_obj_t *row = settings_row(page, NULL);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_color(row, lv_color_hex(CLR_HEX_ACCENT),
                                LV_STATE_PRESSED);
  lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *lbl = lv_label_create(row);
  lv_label_set_text_fmt(lbl, "%s  %s", glyph, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *chev = lv_label_create(row);
  lv_label_set_text(chev, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(chev, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(chev, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);
}

// ╔═══════════════════════════════════════════╗
// ║  TAB — Web portal                         ║
// ╚═══════════════════════════════════════════╝
static void build_tab_portal(lv_obj_t *page) {
  if (isWifiConnected) {
    String url = "http://" + WiFi.localIP().toString();

    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, LV_PCT(100), 214);
    ui_style_surface(card, UI_CARD_RADIUS);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // QR stacked above the text rather than beside it — 292 px will not hold
    // a 120 px code and a readable URL side by side.
    lv_obj_t *qr = lv_qrcode_create(card, 120, lv_color_black(), lv_color_white());
    lv_qrcode_update(qr, url.c_str(), url.length());
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(qr, 3, 0);
    lv_obj_set_style_radius(qr, 8, 0);

    lv_obj_t *lbl_ip = lv_label_create(card);
    lv_label_set_text(lbl_ip, url.c_str());
    lv_obj_set_style_text_color(lbl_ip, CLR_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_TOP_MID, 0, 132);

    lv_obj_t *lbl_scan = lv_label_create(card);
    lv_label_set_text(lbl_scan, L(L_SCAN_QR));
    lv_obj_set_style_text_color(lbl_scan, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_set_style_text_font(lbl_scan, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_scan, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_scan, UI_SETTINGS_W - 48);
    lv_obj_align(lbl_scan, LV_ALIGN_BOTTOM_MID, 0, 0);

    settings_status_row(page, "Wi-Fi", WiFi.SSID().c_str(),
                        lv_color_hex(CLR_HEX_OK));
  } else {
    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, LV_PCT(100), 150);
    ui_style_surface(card, UI_CARD_RADIUS);
    lv_obj_set_style_border_color(card, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_40, 0); // red tint, not a red frame
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_err = lv_label_create(card);
    lv_label_set_text(lbl_err, L(L_DISCONNECTED));
    lv_obj_set_style_text_color(lbl_err, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_text_font(lbl_err, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_err, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *lbl_msg = lv_label_create(card);
    lv_label_set_text(lbl_msg, L(L_WIFI_NOT_CONNECTED));
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_font(lbl_msg, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(lbl_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_msg, UI_SETTINGS_W - 44);
    lv_obj_set_style_text_align(lbl_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_msg, LV_ALIGN_CENTER, 0, 4);

    lv_obj_t *btn_wifi = ui_create_accent_btn(card, 160, 32, "",
                                              btn_wifi_config_cb);
    lv_obj_align(btn_wifi, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text_fmt(lv_obj_get_child(btn_wifi, 0), LV_SYMBOL_WIFI " %s",
                          L(L_SETUP_WIFI));
  }
}

// ╔═══════════════════════════════════════════╗
// ║  TAB — Display                            ║
// ╚═══════════════════════════════════════════╝
// Every choice here is a segmented control rather than a dropdown. LVGL
// parents a dropdown's open list to the screen and sizes it to its own
// content, so in this 312 px column the list rendered wider than the row it
// belonged to and spilled across the card.
//
// The maps are static because lv_btnmatrix stores the pointer instead of
// copying. That is safe across a language switch: build_settings_screen() runs
// again after lang_load(), which refills these with the new strings before any
// widget can read them.
#define SEG_CTRL_W 168
// Design calls for 21 px; 22 keeps the row an even number of pixels tall
// around the label without dropping below the spec's touch height.
#define SEG_CTRL_H 22

static void build_tab_display(lv_obj_t *page) {
  // Brightness
  lv_obj_t *row_br = settings_row(page, L(L_BRIGHTNESS));
  lv_obj_t *slider = lv_slider_create(row_br);
  lv_obj_set_size(slider, SEG_CTRL_W, 8);
  ui_style_slider(slider);
  lv_slider_set_range(slider, 10, 255);
  lv_slider_set_value(slider, displayBrightness, LV_ANIM_OFF);
  lv_obj_align(slider, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_ALL, NULL);

  // Time format
  static const char *map_tf[4];
  map_tf[0] = L(L_TIME_12H); map_tf[1] = L(L_TIME_24H); map_tf[2] = "";
  lv_obj_t *row_tf = settings_row(page, L(L_TIME_FORMAT));
  lv_obj_t *seg_tf = ui_create_segmented(
      row_tf, map_tf, SEG_CTRL_W, SEG_CTRL_H, use24HourFormat ? 1 : 0,
      [](lv_event_t *e) {
        use24HourFormat =
            (lv_btnmatrix_get_selected_btn(lv_event_get_target(e)) == 1);
        Preferences p; p.begin(NVS_NAMESPACE, false);
        p.putBool("time_24h", use24HourFormat);
        p.end();
      });
  lv_obj_align(seg_tf, LV_ALIGN_RIGHT_MID, 0, 0);

  // Language
  static const char *map_lang[LANG_OPTIONS_COUNT + 1];
  for (int i = 0; i < LANG_OPTIONS_COUNT; i++) map_lang[i] = lang_names[i];
  map_lang[LANG_OPTIONS_COUNT] = "";
  int lang_sel = 0;
  for (int i = 0; i < LANG_OPTIONS_COUNT; i++)
    if (strcmp(currentLang, lang_codes[i]) == 0) { lang_sel = i; break; }
  lv_obj_t *row_lang = settings_row(page, L(L_LANGUAGE));
  lv_obj_t *seg_lang = ui_create_segmented(
      row_lang, map_lang, SEG_CTRL_W, SEG_CTRL_H, lang_sel,
      [](lv_event_t *e) {
        int sel = lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
        if (sel < 0 || sel >= LANG_OPTIONS_COUNT) return;
        strncpy(currentLang, lang_codes[sel], sizeof(currentLang) - 1);
        currentLang[sizeof(currentLang) - 1] = '\0';
        Preferences p; p.begin(NVS_NAMESPACE, false);
        p.putString("lang", currentLang);
        p.end();
        lang_load(currentLang);
        ui_refresh_lang();       // Home, header, rail and the sidebar tabs
        build_settings_screen(); // this tab, in the new language
      });
  lv_obj_align(seg_lang, LV_ALIGN_RIGHT_MID, 0, 0);

  // Screensaver style
  static const char *map_ss[4];
  map_ss[0] = L(L_FLIP_CLOCK); map_ss[1] = L(L_MINIMAL);
  map_ss[2] = L(L_SCREEN_OFF); map_ss[3] = "";
  lv_obj_t *row_ss = settings_row(page, L(L_SCREENSAVER));
  lv_obj_t *seg_ss = ui_create_segmented(
      row_ss, map_ss, SEG_CTRL_W, SEG_CTRL_H, screensaverStyle,
      [](lv_event_t *e) {
        screensaverStyle = lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
        Preferences p; p.begin(NVS_NAMESPACE, false);
        p.putInt("ss_style", screensaverStyle);
        p.end();
        invalidate_screensaver_build(); // style changed — rebuild on next show
      });
  lv_obj_align(seg_ss, LV_ALIGN_RIGHT_MID, 0, 0);

  // Screen timeout
  static const char *map_to[5];
  map_to[0] = L(L_1_MIN); map_to[1] = L(L_2_MIN);
  map_to[2] = L(L_5_MIN); map_to[3] = L(L_NEVER); map_to[4] = "";
  lv_obj_t *row_to = settings_row(page, L(L_SCREEN_TIMEOUT));
  lv_obj_t *seg_to = ui_create_segmented(
      row_to, map_to, SEG_CTRL_W, SEG_CTRL_H,
      screensaverTimeoutMs == 60000    ? 0
      : screensaverTimeoutMs == 120000 ? 1
      : screensaverTimeoutMs == 300000 ? 2
                                       : 3,
      [](lv_event_t *e) {
        switch (lv_btnmatrix_get_selected_btn(lv_event_get_target(e))) {
        case 0:  screensaverTimeoutMs = 60000; break;
        case 1:  screensaverTimeoutMs = 120000; break;
        case 2:  screensaverTimeoutMs = 300000; break;
        default: screensaverTimeoutMs = 0; break;
        }
        Preferences p; p.begin(NVS_NAMESPACE, false);
        p.putULong("ss_timeout", screensaverTimeoutMs);
        p.end();
      });
  lv_obj_align(seg_to, LV_ALIGN_RIGHT_MID, 0, 0);

  // Home layout
  static const char *map_lay[3];
  map_lay[0] = L(L_LAYOUT_GRID); map_lay[1] = L(L_LAYOUT_LIST); map_lay[2] = "";
  lv_obj_t *row_lay = settings_row(page, L(L_HOME_LAYOUT));
  lv_obj_t *seg_lay = ui_create_segmented(
      row_lay, map_lay, SEG_CTRL_W, SEG_CTRL_H, homeLayoutStyle,
      [](lv_event_t *e) {
        homeLayoutStyle = lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
        Preferences p; p.begin(NVS_NAMESPACE, false);
        p.putInt("home_layout", homeLayoutStyle);
        p.end();
        rebuild_grid(); // apply immediately behind this screen
      });
  lv_obj_align(seg_lay, LV_ALIGN_RIGHT_MID, 0, 0);

  // Haptic
  lv_obj_t *row_hap = settings_row(page, L(L_HAPTIC));
  lv_obj_t *sw_haptic = lv_switch_create(row_hap);
  lv_obj_set_size(sw_haptic, 40, 22);
  lv_obj_align(sw_haptic, LV_ALIGN_RIGHT_MID, 0, 0);
  ui_style_switch(sw_haptic);
  if (hapticEnabled) lv_obj_add_state(sw_haptic, LV_STATE_CHECKED);
  lv_obj_add_event_cb(
      sw_haptic,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          hapticEnabled =
              lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
          Preferences p; p.begin(NVS_NAMESPACE, false);
          p.putBool("haptic", hapticEnabled);
          p.end();
          if (hapticEnabled) hal_haptic_buzz(); // test buzz
        }
      },
      LV_EVENT_ALL, NULL);

  // Wallpaper — three presets plus a clear button
  lv_obj_t *row_wp = settings_row(page, L(L_WALLPAPER), 82);
  for (int i = 0; i < 3; i++) wp_build_thumb(i);
  for (int i = 0; i < 4; i++) {
    lv_obj_t *frame = lv_obj_create(row_wp);
    lv_obj_set_size(frame, 62, 42);
    lv_obj_align(frame, LV_ALIGN_BOTTOM_LEFT, i * 68, 0);
    lv_obj_set_style_bg_color(frame, lv_color_hex(CLR_HEX_PILL_BG), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(frame, i == 3 ? lv_color_hex(CLR_HEX_DANGER)
                                                : lv_color_hex(CLR_HEX_PILL_BORDER), 0);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_radius(frame, 8, 0);
    lv_obj_set_style_pad_all(frame, 1, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(frame, LV_OBJ_FLAG_CLICKABLE);

    if (i < 3 && wp_thumb_decoded[i]) {
      lv_obj_t *img = lv_img_create(frame);
      lv_img_set_src(img, &wp_thumb_dsc[i]);
      lv_obj_center(img);
      lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    } else {
      lv_obj_t *bl = lv_label_create(frame);
      lv_label_set_text(bl, i == 3 ? LV_SYMBOL_TRASH
                                   : (i == 0 ? "1" : i == 1 ? "2" : "3"));
      lv_obj_set_style_text_color(bl, i == 3 ? lv_color_hex(CLR_HEX_DANGER)
                                             : lv_color_hex(CLR_HEX_TEXT_HI), 0);
      lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
      lv_obj_center(bl);
      lv_obj_add_flag(bl, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    intptr_t id = (i == 3) ? 0 : (i + 1);
    lv_obj_add_event_cb(
        frame,
        [](lv_event_t *e) {
          if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            extern void ui_apply_wallpaper_preset(int);
            ui_apply_wallpaper_preset((int)(intptr_t)lv_event_get_user_data(e));
          }
        },
        LV_EVENT_ALL, (void *)id);
  }
}

// ╔═══════════════════════════════════════════╗
// ║  TAB — Devices                            ║
// ╚═══════════════════════════════════════════╝
// Device, scene and schedule editing still live on their own screens; this tab
// is where the header pills that used to reach them ended up.
static void build_tab_devices(lv_obj_t *page) {
  int on = 0, total = 0;
  ui_count_visible_devices(&on, &total);

  char buf[32];
  snprintf(buf, sizeof(buf), "%d / %d", on, total);
  settings_status_row(page, L(L_DEVICES), buf,
                      lv_color_hex(on > 0 ? CLR_HEX_ACCENT : CLR_HEX_TEXT_LOW));

  snprintf(buf, sizeof(buf), "%d", roomCount);
  settings_status_row(page, "Rooms", buf, lv_color_hex(CLR_HEX_TEXT_LOW));

  settings_link_row(page, LV_SYMBOL_LIST, L(L_DEVICES), [](lv_event_t *e) {
    build_device_list_screen();
    if (ui_ScreenDevices)
      lv_scr_load_anim(ui_ScreenDevices, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  });

  settings_link_row(page, LV_SYMBOL_VIDEO, L(L_SCENES), [](lv_event_t *e) {
    build_scene_list_screen();
    if (ui_ScreenScenes)
      lv_scr_load_anim(ui_ScreenScenes, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  });

  settings_link_row(page, LV_SYMBOL_LOOP, L(L_SCHEDULES), [](lv_event_t *e) {
    // Schedules are the second tab of the Scenes destination now.
    ui_show_main_view(UI_VIEW_SCHEDULE);
    lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  });
}

// ── Panel name editor ───────────────────────────────────
// A small form in place of the tab body, same shape as the Wi-Fi one: the
// keyboard needs the full screen width, so it is parented to the screen and
// torn down by build_settings_screen().
static lv_obj_t *ta_panel_name = NULL;

static void panel_name_save_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (!ta_panel_name) return;

  String name = lv_textarea_get_text(ta_panel_name);
  name.trim();
  if (name.isEmpty()) {
    ui_show_toast(L(L_PANEL_NAME_EMPTY));
    return;
  }

  strncpy(panelTitle, name.c_str(), sizeof(panelTitle) - 1);
  panelTitle[sizeof(panelTitle) - 1] = '\0';
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putString("panel_title", panelTitle);
  p.end();

  // The name reaches three places: the header title on Home, the rail's
  // monogram, and the screensaver's eyebrow. The first two update live; the
  // screensaver is rebuilt next time it shows.
  ui_nav_rail_refresh_logo();
  invalidate_screensaver_build();
  rebuild_grid();

  ta_panel_name = NULL;
  ui_show_toast(L(L_SAVE));
  build_settings_screen();
}

static void build_panel_name_form() {
  if (kb) { lv_obj_del(kb); kb = NULL; }
  lv_obj_clean(set_container);
  ta_ssid = ta_pass = ta_mqtt_srv = ta_mqtt_usr = ta_mqtt_pwd = ta_city = NULL;

  const int card_w = UI_SETTINGS_W - 20;
  lv_obj_t *card = ui_create_glass_card(set_container, card_w, 150);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_pad_all(card, 12, 0);

  lv_obj_t *title = lv_label_create(card);
  lv_label_set_text(title, L(L_PANEL_NAME));
  lv_obj_set_style_text_color(title, CLR_PRIMARY, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_panel_name = ui_create_textarea(card, card_w - 24, L(L_PANEL_NAME),
                                     panelTitle, ta_wifi_event_cb);
  lv_obj_align(ta_panel_name, LV_ALIGN_TOP_MID, 0, 30);

  lv_obj_t *btn = ui_create_accent_btn(card, 100, 30, "", panel_name_save_cb,
                                       NULL, LV_EVENT_CLICKED);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_label_set_text_fmt(lv_obj_get_child(btn, 0), LV_SYMBOL_OK " %s", L(L_SAVE));

  lv_obj_t *cancel = ui_create_pill_btn(card, 92, 30, "",
                                        lv_color_hex(CLR_HEX_TEXT_MID),
                                        [](lv_event_t *e) {
                                          ta_panel_name = NULL;
                                          build_settings_screen();
                                        },
                                        NULL, LV_EVENT_CLICKED);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_label_set_text(lv_obj_get_child(cancel, 0), L(L_CANCEL));

  kb = lv_keyboard_create(lv_obj_get_screen(set_container));
  ui_style_keyboard(kb);
  lv_obj_set_size(kb, SCREEN_WIDTH, 136);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

// ── Restart / factory reset ─────────────────────────────
// Both are irreversible from the panel's point of view, so both confirm. The
// factory reset used to be reachable only by holding the screen during boot,
// which nobody discovers by accident — or on purpose.
static bool s_pending_factory_reset = false;

static void danger_msgbox_cb(lv_event_t *e) {
  lv_obj_t *mbox = lv_event_get_current_target(e);
  const bool yes = (lv_msgbox_get_active_btn(mbox) == 0);
  const bool wipe = s_pending_factory_reset;
  s_pending_factory_reset = false;
  lv_msgbox_close(mbox);
  if (!yes) return;
  Serial.flush();
  if (wipe) factory_reset_now(); // wipes, then restarts
  else      ESP.restart();
}

static void confirm_danger(const char *title, const char *msg, bool wipe) {
  s_pending_factory_reset = wipe;
  static const char *btns[3];
  btns[0] = L(L_YES);
  btns[1] = L(L_NO);
  btns[2] = "";
  lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), title, msg, btns, false);
  ui_style_msgbox(mbox);
  lv_obj_center(mbox);
  lv_obj_add_event_cb(mbox, danger_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// A full-width outlined danger row. Never a filled button — a destructive
// action should not be the loudest thing on the page.
static void danger_row(lv_obj_t *page, const char *glyph, const char *text,
                       lv_event_cb_t cb) {
  lv_obj_t *row = settings_row(page, NULL);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_color(row, lv_color_hex(CLR_HEX_DANGER), 0);
  lv_obj_set_style_border_opa(row, LV_OPA_70, 0);
  lv_obj_set_style_bg_color(row, lv_color_hex(CLR_HEX_DANGER), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(row, LV_OPA_30, LV_STATE_PRESSED);
  lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *lbl = lv_label_create(row);
  lv_label_set_text_fmt(lbl, "%s  %s", glyph, text);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_DANGER_HI), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
}

// ╔═══════════════════════════════════════════╗
// ║  TAB — System                             ║
// ╚═══════════════════════════════════════════╝
static void build_tab_system(lv_obj_t *page) {
  char buf[64];

  if (isWifiConnected) {
    snprintf(buf, sizeof(buf), "%s  %d dBm", WiFi.SSID().c_str(), wifiRssi);
    settings_status_row(page, "Wi-Fi", buf, lv_color_hex(CLR_HEX_OK));
  } else {
    settings_status_row(page, "Wi-Fi", L(L_DISCONNECTED),
                        lv_color_hex(CLR_HEX_DANGER));
  }

  snprintf(buf, sizeof(buf), "%s:%d", mqtt_server_ip.c_str(), mqtt_port);
  settings_status_row(page, "MQTT", isMqttConnected ? buf : L(L_DISCONNECTED),
                      lv_color_hex(isMqttConnected ? CLR_HEX_OK : CLR_HEX_DANGER));

  const unsigned long up = millis() / 1000;
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu", up / 86400,
           (up % 86400) / 3600, (up % 3600) / 60);
  settings_status_row(page, "Uptime", buf, lv_color_hex(CLR_HEX_OK));

  // ESP32-S3 internal die sensor — not the room temperature. It runs well
  // above ambient behind a backlit panel, so the thresholds are set against
  // what the silicon cares about, not what a room would.
  {
    const float die_c = temperatureRead();
    snprintf(buf, sizeof(buf), "%.1f\xC2\xB0" "C", die_c);
    settings_status_row(page, "Chip", buf,
                        lv_color_hex(die_c >= 80.0f   ? CLR_HEX_DANGER
                                     : die_c >= 65.0f ? CLR_HEX_ACCENT
                                                      : CLR_HEX_OK));
  }

  // Internal RAM is the one that runs out — LVGL objects, the MQTT buffers and
  // the web server all live there, while PSRAM holds the draw buffers and the
  // wallpaper. Worth seeing separately.
  const size_t heap_total = ESP.getHeapSize();
  settings_meter_row(page, "Memory", heap_total - ESP.getFreeHeap(), heap_total,
                     true);

  const size_t psram_total = ESP.getPsramSize();
  if (psram_total)
    settings_meter_row(page, "PSRAM", psram_total - ESP.getFreePsram(),
                       psram_total, false);

  // LittleFS — wallpaper, translations and every config file.
  settings_meter_row(page, "Storage", LittleFS.usedBytes(),
                     LittleFS.totalBytes(), true);

  // Panel name — opens the keyboard form
  settings_link_row(page, LV_SYMBOL_EDIT, panelTitle, [](lv_event_t *e) {
    build_panel_name_form();
  });

  // Firmware. There is no update *source* to pull from, so this is not a
  // "check for updates" button — the panel is flashed by pushing a .bin at the
  // web portal or over espota. What it can do is show the version, and show
  // progress while an upload is landing.
  if (otaActive) {
    lv_obj_t *row = settings_row(page, L(L_UPDATING), 46);
    lv_obj_t *bar = lv_bar_create(row);
    lv_obj_set_size(bar, 168, 8);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, otaProgressPct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(CLR_HEX_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);

    lv_obj_t *pct = lv_label_create(row);
    lv_label_set_text_fmt(pct, "%d%%", otaProgressPct);
    lv_obj_set_style_text_font(pct, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pct, lv_color_hex(CLR_HEX_ACCENT), 0);
    lv_obj_align(pct, LV_ALIGN_TOP_RIGHT, 0, 0);
  } else {
    settings_status_row(page, "Firmware", FW_VERSION,
                        lv_color_hex(CLR_HEX_TEXT_LOW));
  }

  settings_link_row(page, LV_SYMBOL_WIFI, L(L_WIFI_SETUP), btn_wifi_config_cb);

  // Reset web auth back to admin/admin
  danger_row(page, LV_SYMBOL_WARNING, L(L_RESET_PASS), [](lv_event_t *e) {
    applySettings(wifi_ssid.c_str(), wifi_pass.c_str(), mqtt_server_ip.c_str(),
                  mqtt_username.c_str(), mqtt_password.c_str(), weatherCity,
                  panelTitle, themeDark, useLargeTiles, displayBrightness,
                  use24HourFormat, "admin", "admin");
  });

  danger_row(page, LV_SYMBOL_REFRESH, L(L_RESTART), [](lv_event_t *e) {
    confirm_danger(L(L_RESTART), L(L_CONFIRM_RESTART), false);
  });

  danger_row(page, LV_SYMBOL_TRASH, L(L_FACTORY_RESET), [](lv_event_t *e) {
    confirm_danger(L(L_FACTORY_RESET), L(L_CONFIRM_FACTORY_RESET), true);
  });
}

// ========================================================
//  SETTINGS BODY
// ========================================================
void build_settings_screen() {
  if (set_container == NULL)
    return;

  // The keyboard is parented to the screen, not to set_container, so it can
  // span the full width over the rail and sidebar. That also means cleaning
  // the container does not take it with us.
  if (kb) {
    lv_obj_del(kb);
    kb = NULL;
  }
  lv_obj_clean(set_container);

  // Only the Wi-Fi form creates these; btn_save_settings_cb falls back to the
  // stored values when they are NULL, so clearing them here is what keeps a
  // rebuilt tab from handing Save a dangling textarea.
  ta_ssid = ta_pass = ta_mqtt_srv = ta_mqtt_usr = ta_mqtt_pwd = ta_city = NULL;

  lv_obj_t *page = settings_page(set_container);

  switch (s_settings_tab) {
  case SET_TAB_PORTAL:  build_tab_portal(page); break;
  case SET_TAB_DISPLAY: build_tab_display(page); break;
  case SET_TAB_DEVICES: build_tab_devices(page); break;
  default:              build_tab_system(page); break;
  }
}

// File-scope helper invoked by the wallpaper preset buttons. Lives outside
// build_settings_screen() so its address is stable across rebuilds.
void ui_apply_wallpaper_preset(int id) {
  if (LittleFS.exists("/wallpaper.jpg")) LittleFS.remove("/wallpaper.jpg");
  if (id >= 1 && id <= 3) {
    char src_path[24];
    snprintf(src_path, sizeof(src_path), "/default%d.jpg", id);
    File src = LittleFS.open(src_path, "r");
    File dst = LittleFS.open("/wallpaper.jpg", "w");
    if (src && dst) {
      uint8_t buf[512];
      while (src.available()) {
        size_t n = src.read(buf, sizeof(buf));
        dst.write(buf, n);
      }
      dst.close(); src.close();
      Serial.printf("[UI] Wallpaper preset %d applied\n", id);
    } else {
      if (src) src.close();
      if (dst) dst.close();
      Serial.println("[UI] Wallpaper preset apply failed");
      return;
    }
  } else {
    Serial.println("[UI] Wallpaper removed");
  }
  delay(300);
  ESP.restart();
}

// Brightness control — lcd is already declared at the top of this file.

void brightness_slider_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *slider = lv_event_get_target(e);

  if (code == LV_EVENT_VALUE_CHANGED) {
    int val = lv_slider_get_value(slider);
    hal_lcd_set_brightness(val); // Update hardware backlight instantly
  } else if (code == LV_EVENT_RELEASED) {
    displayBrightness = lv_slider_get_value(slider);
    // Persist immediately using local preferences to avoid cross-core race
    Preferences p; p.begin(NVS_NAMESPACE, false);
    p.putInt("brightness", displayBrightness);
    p.end();
  }
}

void btn_settings_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    build_settings_screen();
    lv_scr_load_anim(ui_ScreenSettings, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0,
                     false);
  }
}

void btn_back_to_main_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    rebuild_grid();
    lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  }
}

void btn_save_settings_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED || code == LV_EVENT_SHORT_CLICKED ||
      code == LV_EVENT_RELEASED) {
    const char *s_ssid =
        ta_ssid ? lv_textarea_get_text(ta_ssid) : wifi_ssid.c_str();
    const char *s_pass =
        ta_pass ? lv_textarea_get_text(ta_pass) : wifi_pass.c_str();
    const char *s_srv = ta_mqtt_srv ? lv_textarea_get_text(ta_mqtt_srv)
                                    : mqtt_server_ip.c_str();
    const char *s_usr =
        ta_mqtt_usr ? lv_textarea_get_text(ta_mqtt_usr) : mqtt_username.c_str();
    const char *s_pwd =
        ta_mqtt_pwd ? lv_textarea_get_text(ta_mqtt_pwd) : mqtt_password.c_str();
    const char *s_city = ta_city ? lv_textarea_get_text(ta_city) : weatherCity;

    Serial.printf("[UI] Save Settings event=%d\n", (int)code);
    applySettings(s_ssid, s_pass, s_srv, s_usr, s_pwd, s_city, panelTitle,
                   themeDark, useLargeTiles, displayBrightness, use24HourFormat,
                   web_user.c_str(), web_pass.c_str());

    // Force a hard reboot directly from the UI task. We can't rely on the
    // main task's pending_ota_reboot handler because it may still be blocked
    // inside network_setup() (e.g. WiFi.begin retry loop) and never service
    // the flag. Restart from here always works because the UI task runs on
    // a different core/task than the network setup blocking call.
    Serial.println("[UI] Restarting to apply settings...");
    Serial.flush();
    delay(300); // let serial flush + give user visual feedback
    ESP.restart();
  }
}

void btn_goto_devices_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    build_device_list_screen();
    lv_scr_load_anim(ui_ScreenDevices, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  }
}
