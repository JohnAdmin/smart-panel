#include "../config.h"
// globals.h is transitively included via wifi_manager.h below.
#include "../lang.h"
#include "../wallpaper_helper.h"
#include "../wifi_manager.h"
#include "extra/libs/qrcode/lv_qrcode.h"
#include "ui_helpers.h"
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
  lv_obj_clean(set_container);

  // Glass card for WiFi form
  lv_obj_t *card = ui_create_glass_card(set_container, 440, 240);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *lbl_title = lv_label_create(card);
  lv_label_set_text_fmt(lbl_title, LV_SYMBOL_WIFI "  %s", L(L_WIFI_SETUP));
  lv_obj_set_style_text_color(lbl_title, CLR_PRIMARY, 0);
  lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
  lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_ssid = ui_create_textarea(card, 400, L(L_WIFI_SSID), wifi_ssid.c_str(),
                               ta_wifi_event_cb);
  lv_obj_align(ta_ssid, LV_ALIGN_TOP_MID, 0, 32);

  ta_pass = ui_create_textarea(card, 400, L(L_WIFI_PASSWORD),
                               wifi_pass.c_str(), ta_wifi_event_cb);
  lv_obj_align(ta_pass, LV_ALIGN_TOP_MID, 0, 84);
  lv_textarea_set_password_mode(ta_pass, false);

  // Hidden placeholders
  ta_mqtt_srv = NULL;
  ta_mqtt_usr = NULL;
  ta_mqtt_pwd = NULL;
  ta_city = NULL;

  kb = lv_keyboard_create(set_container);
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

void build_settings_screen() {
  if (set_container == NULL)
    return;
  lv_obj_clean(set_container);

  // ── TabView ────────────────────────────────────────────
  lv_obj_t *settings_tabview = lv_tabview_create(set_container, LV_DIR_TOP, 40);
  lv_obj_set_size(settings_tabview, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(settings_tabview, LV_OPA_TRANSP, 0);

  lv_obj_t *tab_content = lv_tabview_get_content(settings_tabview);
  lv_obj_set_style_bg_opa(tab_content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tab_content, 0, 0);

  // Tab Buttons — segmented pill bar
  lv_obj_t *tab_btns = lv_tabview_get_tab_btns(settings_tabview);
  lv_obj_set_style_bg_color(tab_btns, lv_color_hex(CLR_HEX_PILL_BG), 0);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(tab_btns, 12, 0);
  // Token ramp, not CLR_TEXT_DIM — the bar's fill is CLR_HEX_PILL_BG in both
  // themes, so the theme-aware grey went unreadable against it in light mode.
  lv_obj_set_style_text_color(tab_btns, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_14, 0);
  lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_NONE, LV_PART_ITEMS);
  lv_style_selector_t checked = (lv_style_selector_t)LV_PART_ITEMS |
                                (lv_style_selector_t)LV_STATE_CHECKED;
  lv_obj_set_style_text_color(tab_btns, lv_color_hex(CLR_HEX_ON_ACCENT), checked);
  lv_obj_set_style_bg_color(tab_btns, CLR_PRIMARY, checked);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, checked);
  lv_obj_set_style_radius(tab_btns, 10, checked);
  lv_obj_set_style_pad_ver(tab_btns, 4, 0);

  // Create Tabs
  lv_obj_t *tab_web = lv_tabview_add_tab(settings_tabview, L(L_WEB_PORTAL));
  lv_obj_t *tab_dev = lv_tabview_add_tab(settings_tabview, L(L_PANEL_SETTINGS));

  // Clean tab padding
  lv_obj_set_scrollbar_mode(tab_web, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(tab_web, 0, 0);
  lv_obj_set_style_border_width(tab_web, 0, 0);
  lv_obj_set_scrollbar_mode(tab_dev, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(tab_dev, 0, 0);
  lv_obj_set_style_border_width(tab_dev, 0, 0);

  // ╔═══════════════════════════════════════════╗
  // ║         TAB 1 — Web Portal                ║
  // ╚═══════════════════════════════════════════╝
  if (isWifiConnected) {
    // Glass card for QR + info
    lv_obj_t *qr_card = ui_create_glass_card(tab_web, 440, 210);
    lv_obj_align(qr_card, LV_ALIGN_TOP_MID, 0, 4);

    // QR code — left side
    String ipStr = WiFi.localIP().toString();
    String url = "http://" + ipStr;
    lv_obj_t *qr =
        lv_qrcode_create(qr_card, 130, lv_color_black(), lv_color_white());
    lv_qrcode_update(qr, url.c_str(), url.length());
    lv_obj_align(qr, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(qr, 3, 0);
    lv_obj_set_style_radius(qr, 8, 0);

    // Info — right side
    lv_obj_t *lbl_scan = lv_label_create(qr_card);
    lv_label_set_text(lbl_scan, L(L_SCAN_QR));
    lv_obj_set_style_text_color(lbl_scan, lv_color_hex(CLR_HEX_TEXT_HI), 0);
    lv_obj_set_style_text_font(lbl_scan, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(lbl_scan, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_scan, LV_ALIGN_RIGHT_MID, -70, -30);

    // Status dot + Connected label
    lv_obj_t *dot = lv_obj_create(qr_card);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_bg_color(dot, lv_color_hex(CLR_HEX_OK), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_shadow_color(dot, lv_color_hex(CLR_HEX_OK), 0);
    lv_obj_set_style_shadow_width(dot, 8, 0);
    lv_obj_set_style_shadow_opa(dot, LV_OPA_60, 0);

    lv_obj_t *lbl_status = lv_label_create(qr_card);
    lv_label_set_text(lbl_status, L(L_CONNECTED));
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(CLR_HEX_OK), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -88, 18);

    // Anchor the dot to the label instead of giving it its own right-aligned
    // offset. Both were pinned to the card's right edge, so the gap between
    // them was whatever the translated string left over — "Connected" is wide
    // enough to close it to a pixel, and a longer word would overlap the dot.
    lv_obj_update_layout(qr_card);
    lv_obj_align_to(dot, lbl_status, LV_ALIGN_OUT_LEFT_MID, -8, 0);

    // IP address — amber accent
    lv_obj_t *lbl_ip = lv_label_create(qr_card);
    lv_label_set_text(lbl_ip, url.c_str());
    lv_obj_set_style_text_color(lbl_ip, CLR_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_RIGHT_MID, -60, 48);

  } else {
    // Disconnected state — glass card with warning
    lv_obj_t *dc_card = ui_create_glass_card(tab_web, 440, 210);
    lv_obj_align(dc_card, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_border_color(dc_card, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_border_opa(dc_card, LV_OPA_40, 0); // red tint, not a red frame

    // Red dot
    lv_obj_t *dot = lv_obj_create(dc_card);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_bg_color(dot, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_shadow_color(dot, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_shadow_width(dot, 10, 0);
    lv_obj_set_style_shadow_opa(dot, LV_OPA_50, 0);
    lv_obj_align(dot, LV_ALIGN_TOP_MID, -50, 30);

    lv_obj_t *lbl_err = lv_label_create(dc_card);
    lv_label_set_text_fmt(lbl_err, "  %s", L(L_DISCONNECTED));
    lv_obj_set_style_text_color(lbl_err, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_text_font(lbl_err, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_err, LV_ALIGN_TOP_MID, 8, 26);

    lv_obj_t *lbl_msg = lv_label_create(dc_card);
    lv_label_set_text(lbl_msg, L(L_WIFI_NOT_CONNECTED));
    lv_obj_set_style_text_color(lbl_msg, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_align(lbl_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_msg, LV_ALIGN_CENTER, 0, -8);

    // Setup WiFi button — amber pill
    lv_obj_t *btn_wifi = lv_btn_create(dc_card);
    lv_obj_set_size(btn_wifi, 170, 40);
    lv_obj_align(btn_wifi, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(btn_wifi, CLR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(btn_wifi, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_wifi, 0, 0);
    lv_obj_set_style_shadow_color(btn_wifi, CLR_PRIMARY, 0);
    lv_obj_set_style_shadow_width(btn_wifi, 12, 0);
    lv_obj_set_style_shadow_opa(btn_wifi, LV_OPA_40, 0);
    lv_obj_set_style_radius(btn_wifi, 12, 0);
    lv_obj_t *lbl_wifi = lv_label_create(btn_wifi);
    lv_label_set_text_fmt(lbl_wifi, LV_SYMBOL_WIFI " %s", L(L_SETUP_WIFI));
    lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(CLR_HEX_ON_ACCENT), 0);
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_wifi);
    lv_obj_add_event_cb(btn_wifi, btn_wifi_config_cb, LV_EVENT_ALL, NULL);
  }

  // ╔═══════════════════════════════════════════╗
  // ║       TAB 2 — Panel Settings              ║
  // ╚═══════════════════════════════════════════╝

  // ── Glass Card 1: Display ────────────────────
  lv_obj_t *card_disp = ui_create_glass_card(tab_dev, 444, 98);
  lv_obj_align(card_disp, LV_ALIGN_TOP_MID, 0, 4);
  lv_obj_set_style_pad_all(card_disp, 12, 0);

  // Brightness
  lv_obj_t *br_label = lv_label_create(card_disp);
  lv_label_set_text(br_label, L(L_BRIGHTNESS));
  lv_obj_set_style_text_color(br_label, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(br_label, &lv_font_montserrat_12, 0);
  lv_obj_align(br_label, LV_ALIGN_TOP_LEFT, 2, 0);

  lv_obj_t *slider = lv_slider_create(card_disp);
  lv_obj_set_width(slider, 130);
  ui_style_slider(slider);
  lv_slider_set_range(slider, 10, 255);
  lv_slider_set_value(slider, displayBrightness, LV_ANIM_OFF);
  lv_obj_align(slider, LV_ALIGN_BOTTOM_LEFT, 2, -6);
  lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_ALL, NULL);

  // Time Format
  lv_obj_t *tf_label = lv_label_create(card_disp);
  lv_label_set_text(tf_label, L(L_TIME_FORMAT));
  lv_obj_set_style_text_color(tf_label, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(tf_label, &lv_font_montserrat_12, 0);
  lv_obj_align(tf_label, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *dd_tf = lv_dropdown_create(card_disp);
  static char tf_opts[48];
  snprintf(tf_opts, sizeof(tf_opts), "%s\n%s", L(L_TIME_12H), L(L_TIME_24H));
  lv_dropdown_set_options(dd_tf, tf_opts);
  lv_dropdown_set_selected(dd_tf, use24HourFormat ? 1 : 0);
  lv_obj_set_width(dd_tf, 110);
  lv_obj_align(dd_tf, LV_ALIGN_BOTTOM_MID, 0, -4);
  ui_style_dropdown(dd_tf);
  lv_obj_add_event_cb(
      dd_tf,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          use24HourFormat = (lv_dropdown_get_selected(dd) == 1);
          Preferences p; p.begin(NVS_NAMESPACE, false);
          p.putBool("time_24h", use24HourFormat);
          p.end();
        }
      },
      LV_EVENT_ALL, NULL);

  // Screensaver Style
  lv_obj_t *ss_label = lv_label_create(card_disp);
  lv_label_set_text(ss_label, L(L_SCREENSAVER));
  lv_obj_set_style_text_color(ss_label, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(ss_label, &lv_font_montserrat_12, 0);
  lv_obj_align(ss_label, LV_ALIGN_TOP_RIGHT, -4, 0);

  lv_obj_t *dd_ss = lv_dropdown_create(card_disp);
  static char ss_opts[64];
  snprintf(ss_opts, sizeof(ss_opts), "%s\n%s\n%s", L(L_FLIP_CLOCK), L(L_MINIMAL), L(L_SCREEN_OFF));
  lv_dropdown_set_options(dd_ss, ss_opts);
  lv_dropdown_set_selected(dd_ss, screensaverStyle);
  lv_obj_set_width(dd_ss, 120);
  lv_obj_align(dd_ss, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
  ui_style_dropdown(dd_ss);
  lv_obj_add_event_cb(
      dd_ss,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          screensaverStyle = lv_dropdown_get_selected(dd);
          Preferences p; p.begin(NVS_NAMESPACE, false);
          p.putInt("ss_style", screensaverStyle);
          p.end();
        }
      },
      LV_EVENT_ALL, NULL);

  // ── Glass Card 2: System ─────────────────────
  lv_obj_t *card_sys = ui_create_glass_card(tab_dev, 444, 98);
  lv_obj_align(card_sys, LV_ALIGN_TOP_MID, 0, 108);
  lv_obj_set_style_pad_all(card_sys, 12, 0);

  // Timeout
  lv_obj_t *to_label = lv_label_create(card_sys);
  lv_label_set_text(to_label, L(L_SCREEN_TIMEOUT));
  lv_obj_set_style_text_color(to_label, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(to_label, &lv_font_montserrat_12, 0);
  lv_obj_align(to_label, LV_ALIGN_TOP_LEFT, 2, 0);

  lv_obj_t *dd_to = lv_dropdown_create(card_sys);
  static char to_opts[64];
  snprintf(to_opts, sizeof(to_opts), "%s\n%s\n%s\n%s", L(L_1_MIN), L(L_2_MIN), L(L_5_MIN), L(L_NEVER));
  lv_dropdown_set_options(dd_to, to_opts);
  if (screensaverTimeoutMs == 60000)
    lv_dropdown_set_selected(dd_to, 0);
  else if (screensaverTimeoutMs == 120000)
    lv_dropdown_set_selected(dd_to, 1);
  else if (screensaverTimeoutMs == 300000)
    lv_dropdown_set_selected(dd_to, 2);
  else
    lv_dropdown_set_selected(dd_to, 3);
  lv_obj_set_width(dd_to, 120);
  lv_obj_align(dd_to, LV_ALIGN_BOTTOM_LEFT, 2, -4);
  ui_style_dropdown(dd_to);
  lv_obj_add_event_cb(
      dd_to,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          int sel = lv_dropdown_get_selected(dd);
          if (sel == 0)
            screensaverTimeoutMs = 60000;
          else if (sel == 1)
            screensaverTimeoutMs = 120000;
          else if (sel == 2)
            screensaverTimeoutMs = 300000;
          else
            screensaverTimeoutMs = 0;
          Preferences p; p.begin(NVS_NAMESPACE, false);
          p.putULong("ss_timeout", screensaverTimeoutMs);
          p.end();
        }
      },
      LV_EVENT_ALL, NULL);

  // Language dropdown — center of system card
  lv_obj_t *lang_label = lv_label_create(card_sys);
  lv_label_set_text(lang_label, L(L_LANGUAGE));
  lv_obj_set_style_text_color(lang_label, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(lang_label, &lv_font_montserrat_12, 0);
  lv_obj_align(lang_label, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *dd_lang = lv_dropdown_create(card_sys);
  // Build dropdown options: "English\nไทย"
  static char lang_opts[64];
  {
    char *p = lang_opts;
    for (int i = 0; i < LANG_OPTIONS_COUNT; i++) {
      if (i > 0) *p++ = '\n';
      strcpy(p, lang_names[i]);
      p += strlen(lang_names[i]);
    }
    *p = '\0';
  }
  lv_dropdown_set_options(dd_lang, lang_opts);
  // Select current language
  for (int i = 0; i < LANG_OPTIONS_COUNT; i++) {
    if (strcmp(currentLang, lang_codes[i]) == 0) {
      lv_dropdown_set_selected(dd_lang, i);
      break;
    }
  }
  lv_obj_set_width(dd_lang, 110);
  lv_obj_align(dd_lang, LV_ALIGN_BOTTOM_MID, 0, -4);
  ui_style_dropdown(dd_lang);
  lv_obj_add_event_cb(
      dd_lang,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          int sel = lv_dropdown_get_selected(dd);
          if (sel >= 0 && sel < LANG_OPTIONS_COUNT) {
            strncpy(currentLang, lang_codes[sel], sizeof(currentLang) - 1);
            currentLang[sizeof(currentLang) - 1] = '\0';
            Preferences p; p.begin(NVS_NAMESPACE, false);
            p.putString("lang", currentLang);
            p.end();
            lang_load(currentLang);
            // Rebuild UI to apply new language
            ui_refresh_lang();
            build_settings_screen();
          }
        }
      },
      LV_EVENT_ALL, NULL);

  // Reset Web Auth — pill button, right side
  lv_obj_t *btn_reset = lv_btn_create(card_sys);
  lv_obj_set_size(btn_reset, 120, 44);
  lv_obj_align(btn_reset, LV_ALIGN_RIGHT_MID, -4, 14);
  lv_obj_set_style_bg_color(btn_reset, lv_color_hex(CLR_HEX_PILL_BG), 0);
  lv_obj_set_style_bg_opa(btn_reset, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn_reset, lv_color_hex(CLR_HEX_DANGER), 0);
  lv_obj_set_style_border_width(btn_reset, 1, 0);
  lv_obj_set_style_radius(btn_reset, 12, 0);
  lv_obj_set_style_shadow_width(btn_reset, 0, 0);

  // Haptic switch — above reset button
  lv_obj_t *haptic_label = lv_label_create(card_sys);
  lv_label_set_text(haptic_label, L(L_HAPTIC));
  lv_obj_set_style_text_color(haptic_label, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(haptic_label, &lv_font_montserrat_12, 0);
  lv_obj_align(haptic_label, LV_ALIGN_RIGHT_MID, -100, -20);

  lv_obj_t *sw_haptic = lv_switch_create(card_sys);
  lv_obj_set_size(sw_haptic, 40, 22);
  lv_obj_align(sw_haptic, LV_ALIGN_RIGHT_MID, -4, -20);
  ui_style_switch(sw_haptic);
  if (hapticEnabled) lv_obj_add_state(sw_haptic, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw_haptic, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
      hapticEnabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
      Preferences p; p.begin(NVS_NAMESPACE, false);
      p.putBool("haptic", hapticEnabled);
      p.end();
      if (hapticEnabled) hal_haptic_buzz(); // test buzz
    }
  }, LV_EVENT_ALL, NULL);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset);
  lv_label_set_text_fmt(lbl_reset, LV_SYMBOL_WARNING " %s", L(L_RESET_PASS));
  lv_obj_set_style_text_color(lbl_reset, lv_color_hex(CLR_HEX_DANGER), 0);
  lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_12, 0);
  lv_obj_center(lbl_reset);
  lv_obj_add_event_cb(
      btn_reset,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
          applySettings(wifi_ssid.c_str(), wifi_pass.c_str(),
                        mqtt_server_ip.c_str(), mqtt_username.c_str(),
                        mqtt_password.c_str(), weatherCity, panelTitle,
                        themeDark, useLargeTiles, displayBrightness,
                        use24HourFormat, "admin", "admin");
        }
      },
      LV_EVENT_ALL, NULL);

  // ── Glass Card 3: Appearance (Layout + Wallpaper) ───────
  lv_obj_t *card_appr = ui_create_glass_card(tab_dev, 444, 116);
  lv_obj_align(card_appr, LV_ALIGN_TOP_MID, 0, 212);
  lv_obj_set_style_pad_all(card_appr, 12, 0);

  // Layout style dropdown — left
  lv_obj_t *lay_label = lv_label_create(card_appr);
  lv_label_set_text(lay_label, "Home Layout");
  lv_obj_set_style_text_color(lay_label, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(lay_label, &lv_font_montserrat_12, 0);
  lv_obj_align(lay_label, LV_ALIGN_TOP_LEFT, 2, 0);

  lv_obj_t *dd_lay = lv_dropdown_create(card_appr);
  lv_dropdown_set_options(dd_lay, "Modern\nClassic");
  lv_dropdown_set_selected(dd_lay, homeLayoutStyle);
  lv_obj_set_width(dd_lay, 130);
  lv_obj_align(dd_lay, LV_ALIGN_BOTTOM_LEFT, 2, -4);
  ui_style_dropdown(dd_lay);
  lv_obj_add_event_cb(
      dd_lay,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          homeLayoutStyle = lv_dropdown_get_selected(dd);
          Preferences p; p.begin(NVS_NAMESPACE, false);
          p.putInt("home_layout", homeLayoutStyle);
          p.end();
          // Rebuild main grid so new layout applies immediately
          rebuild_grid();
        }
      },
      LV_EVENT_ALL, NULL);

  // Wallpaper presets — right
  lv_obj_t *wp_label = lv_label_create(card_appr);
  lv_label_set_text(wp_label, "Wallpaper");
  lv_obj_set_style_text_color(wp_label, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(wp_label, &lv_font_montserrat_12, 0);
  lv_obj_align(wp_label, LV_ALIGN_TOP_LEFT, 150, 0);

  // Decode preset thumbnails on first open (cached in PSRAM)
  for (int i = 0; i < 3; i++) wp_build_thumb(i);

  // 4 thumbnails: preset 1, 2, 3, then a Trash button to remove wallpaper
  for (int i = 0; i < 4; i++) {
    lv_obj_t *frame = lv_obj_create(card_appr);
    lv_obj_set_size(frame, 64, 50);
    lv_obj_align(frame, LV_ALIGN_BOTTOM_LEFT, 150 + i * 70, -4);
    lv_obj_set_style_bg_color(frame, lv_color_hex(CLR_HEX_PILL_BG), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(frame,
        i == 3 ? lv_color_hex(CLR_HEX_DANGER) : lv_color_hex(CLR_HEX_PILL_BORDER), 0);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_radius(frame, 8, 0);
    lv_obj_set_style_pad_all(frame, 1, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(frame, LV_OBJ_FLAG_CLICKABLE);

    if (i < 3 && wp_thumb_decoded[i]) {
      // Show the actual decoded wallpaper preview
      lv_obj_t *img = lv_img_create(frame);
      lv_img_set_src(img, &wp_thumb_dsc[i]);
      lv_obj_center(img);
      lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    } else {
      // Fallback: numeric label or trash icon
      lv_obj_t *bl = lv_label_create(frame);
      lv_label_set_text(bl, i == 3 ? LV_SYMBOL_TRASH : (i == 0 ? "1" : i == 1 ? "2" : "3"));
      lv_obj_set_style_text_color(bl,
          i == 3 ? lv_color_hex(CLR_HEX_DANGER) : lv_color_hex(CLR_HEX_TEXT_HI), 0);
      lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
      lv_obj_center(bl);
      lv_obj_add_flag(bl, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    intptr_t id = (i == 3) ? 0 : (i + 1);
    lv_obj_add_event_cb(
        frame,
        [](lv_event_t *e) {
          if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            int id = (int)(intptr_t)lv_event_get_user_data(e);
            extern void ui_apply_wallpaper_preset(int);
            ui_apply_wallpaper_preset(id);
          }
        },
        LV_EVENT_ALL, (void *)id);
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
