// ─── Premium Screensaver ──────────────────────────────────────────────────────
// Style 0: Premium Flip Clock (HH:MM) — warm amber accents, glass panels,
//          pulsing colon, weather + status bar
// Style 1: Premium Minimal   — clean typography, accent line, weather
// Style 2: Screen Off        — pure black, wake-on-touch

#include <Arduino.h>
#include <lvgl.h>
#include "../wifi_manager.h"
#include "../lang.h"
#include "../stock.h"
#include "ui_helpers.h"
#include "ui_screens.h"
#include <WiFi.h>

// ── Screensaver layout constants ────────────────────────
// The clock group is centred in the space above the footer rule; the footer
// stacks an optional ticker row over the weather/status row.
#define SS_FLAP_W        88
#define SS_FLAP_H        138
#define SS_FLAP_X1       157   // outer digit offset from centre
#define SS_FLAP_X2       61    // inner digit offset from centre
#define SS_CLOCK_Y       -12
#define SS_SIDE_PAD      24
#define SS_ROW_H         18
#define SS_STATUS_Y      -18   // status row centre, from bottom
#define SS_TICKER_Y      -52   // ticker row centre, from bottom

#define SS_STOCK_UP_COLOR   "34D399"
#define SS_STOCK_DOWN_COLOR "F87171"
#define SS_STOCK_FLAT_COLOR "6B7688"
#define SS_STOCK_NA_COLOR   "6B7688"

// --- Screensaver UI element pointers (extern-visible) ---
lv_obj_t *ss_label_date;
lv_obj_t *flip_hour_tens_lbl;
lv_obj_t *flip_hour_ones_lbl;
lv_obj_t *flip_min_tens_lbl;
lv_obj_t *flip_min_ones_lbl;
lv_obj_t *flip_sec_tens_lbl;
lv_obj_t *flip_sec_ones_lbl;
lv_obj_t *ss_label_sysinfo;
lv_obj_t *ss_label_mqtt_wifi;

// --- Internal state ---
static char last_h1 = 0, last_h2 = 0, last_m1 = 0, last_m2 = 0,
            last_s1 = 0, last_s2 = 0;
static uint32_t last_ss_activation_ms = 0;
static lv_obj_t *ss_minimal_time_lbl = NULL;
static lv_obj_t *ss_colon_lbl = NULL;
static lv_obj_t *ss_meridiem_lbl = NULL; // AM/PM flag, 12-hour mode only
static int last_built_style = -1;

// Called from web_server POST handler when stock/style config changes.
// Forces a full rebuild on the next show_screensaver() call.
void invalidate_screensaver_build() { last_built_style = -1; }
static int ss_pixel_shift_x = 0;
static int ss_pixel_shift_y = 0;
static unsigned long ss_last_shift_ms = 0;
static lv_obj_t *ss_content_wrap = NULL; // wrapper for pixel-shift anti burn-in
static lv_obj_t *ss_stock_lbl[STOCK_MAX_SYMBOLS] = {NULL, NULL, NULL};

// Colon pulse animation callback
static void colon_opa_anim_cb(void *obj, int32_t v) {
  lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

// ── Flap flip ───────────────────────────────────────────
// A real split-flap rotates the card face; this target cannot. 3D transforms
// don't exist in LVGL 8, and `lv_obj_set_style_transform_angle` crashes here
// rather than degrading (see CLAUDE.md), so the substitute is the one the
// design specifies: grow the plate's height from zero while fading it in, over
// 450 ms on an ease-out path. Read together they land close enough to a card
// dropping into place.
static void flap_h_anim_cb(void *obj, int32_t v) {
  lv_obj_set_height((lv_obj_t *)obj, v);
}
static void flap_opa_anim_cb(void *obj, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

void screensaver_touch_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (millis() - last_ss_activation_ms < 500)
      return;
    screensaverActive = false;
    lastTouchTime = millis();
    lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, true);
    ui_ScreenSaver = NULL;
    last_built_style = -1;
    ss_label_date = NULL;
    flip_hour_tens_lbl = flip_hour_ones_lbl = NULL;
    flip_min_tens_lbl = flip_min_ones_lbl = NULL;
    flip_sec_tens_lbl = flip_sec_ones_lbl = NULL;
    ss_label_sysinfo = NULL;
    ss_label_mqtt_wifi = NULL;
    ss_minimal_time_lbl = NULL;
    ss_colon_lbl = NULL;
    ss_meridiem_lbl = NULL;
    for (int _i = 0; _i < STOCK_MAX_SYMBOLS; _i++) ss_stock_lbl[_i] = NULL;
  }
}

// --- Split-flap digit card ---
// A real split-flap card is a dark slab with a machined seam across the middle
// — the light comes from the digit, not from the frame. The previous version
// ringed every card in amber, which turned the clock into a grid of boxes.
lv_obj_t *create_flip_flap(lv_obj_t *parent, int x_offset) {
  lv_obj_t *flap = lv_obj_create(parent);
  lv_obj_set_size(flap, SS_FLAP_W, SS_FLAP_H);
  lv_obj_align(flap, LV_ALIGN_CENTER, x_offset, SS_CLOCK_Y);

  // Machined slab, flat fill. The old top-to-base gradient banded into coloured
  // seams on this RGB565 panel — see the note in ui_helpers.h. The seam strip
  // below still gives the flap its fold.
  lv_obj_set_style_bg_color(flap, lv_color_hex(0x1A2029), 0);
  lv_obj_set_style_bg_grad_dir(flap, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(flap, LV_OPA_COVER, 0);

  // Hairline edge only — no coloured frame
  lv_obj_set_style_border_color(flap, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(flap, 1, 0);
  lv_obj_set_style_border_opa(flap, LV_OPA_70, 0);
  lv_obj_set_style_radius(flap, 12, 0);

  // Depth from a neutral drop shadow, not a halo
  lv_obj_set_style_shadow_color(flap, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(flap, 22, 0);
  lv_obj_set_style_shadow_ofs_y(flap, 6, 0);
  lv_obj_set_style_shadow_opa(flap, LV_OPA_50, 0);
  lv_obj_set_style_pad_all(flap, 0, 0);
  lv_obj_clear_flag(flap, LV_OBJ_FLAG_SCROLLABLE);

  // Top bevel — a single lit pixel row reads as glass
  lv_obj_t *highlight = lv_obj_create(flap);
  lv_obj_set_size(highlight, SS_FLAP_W - 24, 1);
  lv_obj_align(highlight, LV_ALIGN_TOP_MID, 0, 5);
  lv_obj_set_style_bg_color(highlight, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(highlight, LV_OPA_20, 0);
  lv_obj_set_style_border_width(highlight, 0, 0);
  lv_obj_set_style_radius(highlight, 0, 0);

  // Large digit
  lv_obj_t *lbl = lv_label_create(flap);
  lv_obj_set_style_text_font(lbl, &lv_font_arial_120, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_label_set_text(lbl, "0");
  lv_obj_center(lbl);

  // Mechanical seam: a dark gap with a lit lip underneath
  lv_obj_t *seam = lv_obj_create(flap);
  lv_obj_set_size(seam, SS_FLAP_W, 2);
  lv_obj_align(seam, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(seam, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(seam, LV_OPA_70, 0);
  lv_obj_set_style_border_width(seam, 0, 0);
  lv_obj_set_style_radius(seam, 0, 0);
  lv_obj_set_style_shadow_width(seam, 0, 0);

  lv_obj_t *seam_lip = lv_obj_create(flap);
  lv_obj_set_size(seam_lip, SS_FLAP_W, 1);
  lv_obj_align(seam_lip, LV_ALIGN_CENTER, 0, 2);
  lv_obj_set_style_bg_color(seam_lip, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(seam_lip, LV_OPA_10, 0);
  lv_obj_set_style_border_width(seam_lip, 0, 0);
  lv_obj_set_style_radius(seam_lip, 0, 0);
  lv_obj_set_style_shadow_width(seam_lip, 0, 0);

  return lbl;
}

// --- Footer row factory ---
// Transparent full-width strip used for the ticker and status lines. Boxing
// them in bordered panels was what made the bottom of the screen look busy.
static lv_obj_t *create_ss_row(lv_obj_t *parent, int y_from_bottom) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, SCREEN_WIDTH - SS_SIDE_PAD * 2, SS_ROW_H);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, y_from_bottom);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_shadow_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  return row;
}

void build_screensaver() {
  if (ui_ScreenSaver && last_built_style == screensaverStyle)
    return;

  if (ui_ScreenSaver)
    lv_obj_del(ui_ScreenSaver);
  ui_ScreenSaver = lv_obj_create(NULL);
  last_built_style = screensaverStyle;

  // Pitch black background
  lv_obj_set_style_bg_color(ui_ScreenSaver, lv_color_make(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(ui_ScreenSaver, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ui_ScreenSaver, 0, 0);
  lv_obj_set_style_outline_width(ui_ScreenSaver, 0, 0);
  lv_obj_set_style_outline_opa(ui_ScreenSaver, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(ui_ScreenSaver, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui_ScreenSaver, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui_ScreenSaver, screensaver_touch_cb, LV_EVENT_ALL, NULL);

  // Reset all pointers
  ss_label_date = NULL;
  flip_hour_tens_lbl = flip_hour_ones_lbl = NULL;
  flip_min_tens_lbl = flip_min_ones_lbl = NULL;
  flip_sec_tens_lbl = flip_sec_ones_lbl = NULL;
  ss_label_sysinfo = ss_label_mqtt_wifi = NULL;
  ss_minimal_time_lbl = NULL;
  ss_colon_lbl = NULL;
  ss_meridiem_lbl = NULL;
  ss_content_wrap = NULL;
  for (int _i = 0; _i < STOCK_MAX_SYMBOLS; _i++) ss_stock_lbl[_i] = NULL;
  last_h1 = last_h2 = last_m1 = last_m2 = last_s1 = last_s2 = 0;

  // ====== STYLE 2: SCREEN OFF ======
  if (screensaverStyle == 2)
    return;

  // Oversized wrapper container for anti burn-in pixel shift
  // Larger than screen so shifting doesn't reveal edges
  ss_content_wrap = lv_obj_create(ui_ScreenSaver);
  lv_obj_set_size(ss_content_wrap, SCREEN_WIDTH + 20, SCREEN_HEIGHT + 16);
  lv_obj_set_pos(ss_content_wrap, -10, -8); // center the oversize
  lv_obj_set_style_bg_color(ss_content_wrap, lv_color_make(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(ss_content_wrap, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ss_content_wrap, 0, 0);
  lv_obj_set_style_pad_all(ss_content_wrap, 0, 0);
  lv_obj_set_style_radius(ss_content_wrap, 0, 0);
  lv_obj_set_style_clip_corner(ss_content_wrap, true, 0);
  lv_obj_clear_flag(ss_content_wrap, LV_OBJ_FLAG_SCROLLABLE);

  // Wake hint — the screensaver covers the whole panel, rail included, so
  // nothing else on screen says the display is still live and touchable.
  lv_obj_t *ss_hint = lv_label_create(ss_content_wrap);
  lv_label_set_text(ss_hint, L(L_TAP_TO_WAKE));
  lv_obj_set_style_text_font(ss_hint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ss_hint, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_set_style_text_opa(ss_hint, LV_OPA_60, 0);
  lv_obj_align(ss_hint, LV_ALIGN_TOP_RIGHT, -SS_SIDE_PAD, 14);

  // ====== STYLE 1: PREMIUM MINIMAL ======
  if (screensaverStyle == 1) {
    // Panel title — quiet eyebrow above the clock
    lv_obj_t *m_title = lv_label_create(ss_content_wrap);
    lv_label_set_text(m_title,
                      (panelTitle[0] != '\0') ? panelTitle : L(L_SMART_HOME));
    lv_obj_set_style_text_font(m_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_title, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_set_style_text_letter_space(m_title, 4, 0);
    lv_obj_align(m_title, LV_ALIGN_CENTER, 0, -78);

    // Large time — centered, wide letter spacing
    ss_minimal_time_lbl = lv_label_create(ss_content_wrap);
    lv_label_set_text(ss_minimal_time_lbl, currentTime);
    lv_obj_set_style_text_font(ss_minimal_time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ss_minimal_time_lbl, lv_color_hex(CLR_HEX_TEXT_HI), 0);
    lv_obj_set_style_text_letter_space(ss_minimal_time_lbl, 8, 0);
    lv_obj_align(ss_minimal_time_lbl, LV_ALIGN_CENTER, 0, -36);

    // AM/PM flag — baseline-aligned to the right of the clock
    ss_meridiem_lbl = lv_label_create(ss_content_wrap);
    lv_label_set_text(ss_meridiem_lbl, currentMeridiem);
    lv_obj_set_style_text_font(ss_meridiem_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ss_meridiem_lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_align_to(ss_meridiem_lbl, ss_minimal_time_lbl,
                    LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);

    // Amber accent underline — the only colour on the screen
    lv_obj_t *accent = lv_obj_create(ss_content_wrap);
    lv_obj_set_size(accent, 48, 2);
    lv_obj_align(accent, LV_ALIGN_CENTER, 0, -2);
    lv_obj_set_style_bg_color(accent, lv_color_hex(CLR_HEX_ACCENT), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_90, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 1, 0);
    lv_obj_set_style_shadow_width(accent, 0, 0);

    // Date
    ss_label_date = lv_label_create(ss_content_wrap);
    lv_label_set_text(ss_label_date, currentDate);
    lv_obj_set_style_text_font(ss_label_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ss_label_date, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_letter_space(ss_label_date, 2, 0);
    lv_obj_align(ss_label_date, LV_ALIGN_CENTER, 0, 18);

    // Weather
    ss_label_sysinfo = lv_label_create(ss_content_wrap);
    lv_obj_set_style_text_font(ss_label_sysinfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ss_label_sysinfo, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_label_set_long_mode(ss_label_sysinfo, LV_LABEL_LONG_DOT);
    lv_obj_set_size(ss_label_sysinfo, SCREEN_WIDTH - 40, SS_ROW_H);
    lv_obj_set_style_text_align(ss_label_sysinfo, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ss_label_sysinfo, "");
    lv_obj_align(ss_label_sysinfo, LV_ALIGN_CENTER, 0, 44);

    // Footer rule + connectivity
    lv_obj_t *m_rule = ui_create_divider(ss_content_wrap,
                                         SCREEN_WIDTH - SS_SIDE_PAD * 2);
    lv_obj_align(m_rule, LV_ALIGN_BOTTOM_MID, 0, SS_STATUS_Y - 26);
    lv_obj_set_style_bg_opa(m_rule, LV_OPA_50, 0);

    ss_label_mqtt_wifi = lv_label_create(ss_content_wrap);
    lv_obj_set_style_text_font(ss_label_mqtt_wifi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ss_label_mqtt_wifi, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_label_set_recolor(ss_label_mqtt_wifi, true);
    lv_obj_set_style_text_align(ss_label_mqtt_wifi, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ss_label_mqtt_wifi, "");
    lv_obj_align(ss_label_mqtt_wifi, LV_ALIGN_BOTTOM_MID, 0, SS_STATUS_Y);
    return;
  }

  // ====== STYLE 0: PREMIUM FLIP CLOCK ======

  // Panel title — a quiet eyebrow. A screensaver's job is to show the time;
  // the title should not compete with it.
  lv_obj_t *ss_title = lv_label_create(ss_content_wrap);
  const char *title = (panelTitle[0] != '\0') ? panelTitle : L(L_SMART_HOME);
  lv_label_set_text(ss_title, title);
  lv_obj_set_style_text_font(ss_title, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ss_title, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_set_style_text_letter_space(ss_title, 4, 0);
  lv_obj_align(ss_title, LV_ALIGN_TOP_MID, 0, 14);

  // Date — one step up in the hierarchy from the title
  ss_label_date = lv_label_create(ss_content_wrap);
  lv_label_set_text(ss_label_date, currentDate);
  lv_obj_set_style_text_font(ss_label_date, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ss_label_date, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_letter_space(ss_label_date, 2, 0);
  lv_obj_align(ss_label_date, LV_ALIGN_TOP_MID, 0, 34);

  // === 4 flip panels: HH : MM ===
  // Pairs are grouped tightly and split by a wider gap, so the colon has room
  // to sit between them instead of being crushed against the cards.
  flip_hour_tens_lbl = create_flip_flap(ss_content_wrap, -SS_FLAP_X1);
  flip_hour_ones_lbl = create_flip_flap(ss_content_wrap, -SS_FLAP_X2);
  flip_min_tens_lbl  = create_flip_flap(ss_content_wrap,  SS_FLAP_X2);
  flip_min_ones_lbl  = create_flip_flap(ss_content_wrap,  SS_FLAP_X1);

  // Pulsing colon — the one place amber appears on this screen
  ss_colon_lbl = lv_label_create(ss_content_wrap);
  lv_label_set_text(ss_colon_lbl, ":");
  lv_obj_set_style_text_font(ss_colon_lbl, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(ss_colon_lbl, lv_color_hex(CLR_HEX_ACCENT), 0);
  lv_obj_align(ss_colon_lbl, LV_ALIGN_CENTER, 0, SS_CLOCK_Y - 6);

  // AM/PM flag — sits in the same centre channel, below the colon. Empty in
  // 24-hour mode, so the channel just holds the colon.
  ss_meridiem_lbl = lv_label_create(ss_content_wrap);
  lv_label_set_text(ss_meridiem_lbl, currentMeridiem);
  lv_obj_set_style_text_font(ss_meridiem_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ss_meridiem_lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(ss_meridiem_lbl, LV_ALIGN_CENTER, 0, SS_CLOCK_Y + 34);

  // Smooth colon pulse animation (fade in/out, 1.6s cycle)
  lv_anim_t pulse;
  lv_anim_init(&pulse);
  lv_anim_set_var(&pulse, ss_colon_lbl);
  lv_anim_set_values(&pulse, LV_OPA_COVER, LV_OPA_20);
  lv_anim_set_time(&pulse, 800);
  lv_anim_set_playback_time(&pulse, 800);
  lv_anim_set_repeat_count(&pulse, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&pulse, lv_anim_path_ease_in_out);
  lv_anim_set_exec_cb(&pulse, colon_opa_anim_cb);
  lv_anim_start(&pulse);

  // === Footer ===
  // One hairline separates the clock from its data; the rows themselves are
  // unboxed text, which is what keeps the screen calm.
  lv_obj_t *foot_rule = ui_create_divider(ss_content_wrap,
                                          SCREEN_WIDTH - SS_SIDE_PAD * 2);
  // 26 = half the row height + an 8 px breathing gap above the first row
  lv_obj_align(foot_rule, LV_ALIGN_BOTTOM_MID, 0,
               stockEnabled ? (SS_TICKER_Y - 26) : (SS_STATUS_Y - 26));
  lv_obj_set_style_bg_opa(foot_rule, LV_OPA_50, 0);

  // Status row — weather on the left, connectivity on the right
  lv_obj_t *status_row = create_ss_row(ss_content_wrap, SS_STATUS_Y);

  ss_label_sysinfo = lv_label_create(status_row);
  lv_obj_set_style_text_font(ss_label_sysinfo, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ss_label_sysinfo, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_label_set_long_mode(ss_label_sysinfo, LV_LABEL_LONG_DOT);
  // Fixed height pins this to one line — the old 205 px width let a long
  // "temp / condition / city" string wrap and push the row out of shape.
  lv_obj_set_size(ss_label_sysinfo, 250, SS_ROW_H);
  lv_label_set_text(ss_label_sysinfo, "");
  lv_obj_align(ss_label_sysinfo, LV_ALIGN_LEFT_MID, 0, 0);

  ss_label_mqtt_wifi = lv_label_create(status_row);
  lv_obj_set_style_text_font(ss_label_mqtt_wifi, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ss_label_mqtt_wifi, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_label_set_recolor(ss_label_mqtt_wifi, true);
  lv_label_set_text(ss_label_mqtt_wifi, "");
  lv_obj_align(ss_label_mqtt_wifi, LV_ALIGN_RIGHT_MID, 0, 0);

  // === Stock ticker row (above the status row, only when stockEnabled) ===
  if (stockEnabled) {
    // Flex row with even spacing — the old fixed left/centre/right offsets
    // overlapped each other once a symbol had a price attached.
    lv_obj_t *stock_row = create_ss_row(ss_content_wrap, SS_TICKER_Y);
    lv_obj_set_flex_flow(stock_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stock_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int _i = 0; _i < STOCK_MAX_SYMBOLS; _i++) {
      ss_stock_lbl[_i] = lv_label_create(stock_row);
      lv_obj_set_style_text_font(ss_stock_lbl[_i], &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(ss_stock_lbl[_i], lv_color_hex(CLR_HEX_TEXT_MID), 0);
      lv_label_set_recolor(ss_stock_lbl[_i], true);
      lv_label_set_long_mode(ss_stock_lbl[_i], LV_LABEL_LONG_CLIP);
      lv_label_set_text(ss_stock_lbl[_i], "--");
    }
  } // end if (stockEnabled)
} // end build_screensaver

void show_screensaver() {
  screensaverActive = true;
  last_ss_activation_ms = millis();
  build_screensaver();
  update_screensaver();
  lv_scr_load_anim(ui_ScreenSaver, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

void update_screensaver() {
  if (!screensaverActive || !ui_ScreenSaver)
    return;

  if (screensaverStyle == 2)
    return; // Screen Off — nothing to update

  // --- Anti burn-in pixel shift (±5px every 60s) ---
  if (ss_content_wrap && millis() - ss_last_shift_ms > 60000) {
    ss_last_shift_ms = millis();
    ss_pixel_shift_x = (int)(esp_random() % 17) - 8; // -8 to +8
    ss_pixel_shift_y = (int)(esp_random() % 13) - 6; // -6 to +6
    // Shift the oversized wrapper — screen stays fixed, no white edges
    lv_obj_set_pos(ss_content_wrap, -10 + ss_pixel_shift_x, -8 + ss_pixel_shift_y);
  }

  // --- Format weather string ---
  // "28°  Partly Cloudy  ·  Bangkok" — a middle dot separates the place from
  // the reading more cleanly than a hyphen at this size.
  char wBuf[64] = "";
  if (weatherValid) {
    if (weatherCityName[0])
      snprintf(wBuf, sizeof(wBuf), "%.0f\xC2\xB0""  %s  \xC2\xB7  %s",
               weatherTemp, weatherDesc, weatherCityName);
    else
      snprintf(wBuf, sizeof(wBuf), "%.0f\xC2\xB0""  %s",
               weatherTemp, weatherDesc);
  }

  // --- Connectivity: coloured glyphs + how much of the house is on ---
  // "n on" rather than a device total: the total never changes, so it told you
  // nothing you could act on from across the room.
  int on_count = 0, total_count = 0;
  ui_count_visible_devices(&on_count, &total_count);
  char onBuf[24];
  snprintf(onBuf, sizeof(onBuf), L(L_ON_COUNT), on_count);

  char netBuf[112];
  snprintf(netBuf, sizeof(netBuf),
           "#%s " LV_SYMBOL_WIFI "#   #%s " LV_SYMBOL_UPLOAD "#   #%s %s#",
           isWifiConnected ? "34D399" : "EF4444",
           isMqttConnected ? "34D399" : "EF4444",
           on_count > 0 ? "F59E0B" : "6B7688", onBuf);

  // --- AM/PM flag (both styles; empty string in 24-hour mode) ---
  if (ss_meridiem_lbl &&
      strcmp(lv_label_get_text(ss_meridiem_lbl), currentMeridiem) != 0)
    lv_label_set_text(ss_meridiem_lbl, currentMeridiem);

  // --- Style 1: Premium Minimal ---
  if (screensaverStyle == 1) {
    if (ss_minimal_time_lbl)
      lv_label_set_text(ss_minimal_time_lbl, currentTime);
    if (ss_label_date)
      lv_label_set_text(ss_label_date, currentDate);
    if (ss_label_sysinfo)
      lv_label_set_text(ss_label_sysinfo, wBuf);
    if (ss_label_mqtt_wifi)
      lv_label_set_text(ss_label_mqtt_wifi, netBuf);
    return;
  }

  // --- Style 0: Premium Flip Clock ---
  if (strlen(currentTime) >= 5) {
    char h1 = currentTime[0], h2 = currentTime[1];
    char m1 = currentTime[3], m2 = currentTime[4];

    // The new digit is set first and then revealed by the animation. The
    // previous version swapped the text and *then* squashed the plate down and
    // back, so the new digit was already legible before the movement started —
    // which read as a glitch rather than a flip.
    auto update_flap = [](lv_obj_t *lbl, char new_val, char &last_val) {
      if (new_val == last_val) return;
      last_val = new_val;
      lv_obj_t *flap = lv_obj_get_parent(lbl);
      lv_label_set_text_fmt(lbl, "%c", new_val);

      // A minute can change while the previous flip is still running (the
      // clock ticks every second and the animation lasts 450 ms). Drop the
      // in-flight pass first, or the plate is left at whatever height the
      // interrupted animation had reached.
      lv_anim_del(flap, flap_h_anim_cb);
      lv_anim_del(flap, flap_opa_anim_cb);

      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, flap);
      lv_anim_set_time(&a, 450);
      lv_anim_set_path_cb(&a, lv_anim_path_ease_out);

      lv_anim_set_values(&a, 0, SS_FLAP_H);
      lv_anim_set_exec_cb(&a, flap_h_anim_cb);
      lv_anim_start(&a);

      lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
      lv_anim_set_exec_cb(&a, flap_opa_anim_cb);
      lv_anim_start(&a);
    };

    if (flip_hour_tens_lbl) update_flap(flip_hour_tens_lbl, h1, last_h1);
    if (flip_hour_ones_lbl) update_flap(flip_hour_ones_lbl, h2, last_h2);
    if (flip_min_tens_lbl)  update_flap(flip_min_tens_lbl, m1, last_m1);
    if (flip_min_ones_lbl)  update_flap(flip_min_ones_lbl, m2, last_m2);

    if (ss_label_date)
      lv_label_set_text(ss_label_date, currentDate);
  }

  // Weather in status bar (left)
  if (ss_label_sysinfo)
    lv_label_set_text(ss_label_sysinfo, wBuf);

  // WiFi/MQTT on the right of the status row
  if (ss_label_mqtt_wifi)
    lv_label_set_text(ss_label_mqtt_wifi, netBuf);

  // Stock ticker row — update 3 labels
  if (stockEnabled) {
    for (int _i = 0; _i < STOCK_MAX_SYMBOLS; _i++) {
      if (!ss_stock_lbl[_i]) continue;
      if (!stockData[_i].is_valid) {
        // Show symbol with grey N/A to indicate unavailable data
        char no_data_buf[32];
        if (stockSymbols[_i][0]) {
          // Strip exchange suffix for display
          char disp_sym[12] = {};
          strncpy(disp_sym, stockSymbols[_i], sizeof(disp_sym) - 1);
          char *dot = strchr(disp_sym, '.');
          if (dot) *dot = '\0';
          snprintf(no_data_buf, sizeof(no_data_buf),
                   "%s #" SS_STOCK_NA_COLOR " N/A#", disp_sym);
        } else {
          snprintf(no_data_buf, sizeof(no_data_buf), "--");
        }
        lv_label_set_text(ss_stock_lbl[_i], no_data_buf);
        continue;
      }
      // Build display symbol: strip exchange suffix (.BK, .US, etc.)
      char disp[12] = {};
      strncpy(disp, stockData[_i].symbol, sizeof(disp) - 1);
      char *dot = strchr(disp, '.');
      if (dot) *dot = '\0';

      const float pct = stockData[_i].percent_change;
      const bool is_up = pct > 0.001f;
      const bool is_down = pct < -0.001f;
      const char *color = is_up ? SS_STOCK_UP_COLOR : (is_down ? SS_STOCK_DOWN_COLOR : SS_STOCK_FLAT_COLOR);
      const char *trend = is_up ? LV_SYMBOL_UP : (is_down ? LV_SYMBOL_DOWN : "=");
      const char *sign = is_up ? "+" : "";
      char buf[72];
      snprintf(buf, sizeof(buf), "%s %.2f #%s %s %s%.2f%%%s#",
               disp,
               stockData[_i].price,
               color, trend, sign, pct,
               stockData[_i].market_open ? "" : " z");
      lv_label_set_text(ss_stock_lbl[_i], buf);
    }
  }
}
