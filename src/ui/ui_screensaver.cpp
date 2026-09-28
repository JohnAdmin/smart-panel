// ─── Premium Screensaver ──────────────────────────────────────────────────────
// Style 0: Premium Flip Clock (HH:MM) — warm amber accents, glass panels,
//          pulsing colon, weather + status bar
// Style 1: Premium Minimal   — clean typography, accent line, weather
// Style 2: Screen Off        — pure black, wake-on-touch
// Style 3: Weather           — clock + current conditions over a 5-day
//          forecast strip (Open-Meteo, same fetch as the other styles)

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

// snprintf truncates by bytes, so an overflow can stop halfway through a UTF-8
// sequence and the leftover bytes render as a missing-glyph box rather than as
// nothing. The buffers below are sized not to overflow, but a translation is
// runtime data from LittleFS and can grow past whatever we sized for — this
// makes that show up as a shorter string instead of a box.
static void trim_partial_utf8(char *s) {
  size_t len = strlen(s);
  if (!len) return;
  size_t i = len - 1;
  while (i > 0 && ((uint8_t)s[i] & 0xC0) == 0x80) i--; // back over continuations
  uint8_t lead = (uint8_t)s[i];
  size_t need = lead < 0x80             ? 1
                : (lead & 0xE0) == 0xC0 ? 2
                : (lead & 0xF0) == 0xE0 ? 3
                : (lead & 0xF8) == 0xF0 ? 4
                                        : 1;
  if (i + need > len) s[i] = '\0';
}
static lv_obj_t *ss_stock_lbl[STOCK_MAX_SYMBOLS] = {NULL, NULL, NULL};

// ── Weather style (3) ───────────────────────────────────
// Top row: connectivity, forecast age, wake hint. Upper band: clock/date on
// the left, today's conditions in a right-hand column. Then an hourly strip
// (the next few hours' rain is what gets checked before leaving the house)
// and one card per forecast day. Coordinates are in the oversized
// pixel-shift wrapper, so SS_SIDE_PAD lands 14 px in from the glass and the
// visible band runs from y = 8 to y = 328.
#define SS_WX_COL_W      196   // right-hand "now" column
#define SS_WX_TOP_Y      30    // top of the upper band
#define SS_WX_NOW_ICON   40
#define SS_WX_HR_Y       128   // hourly strip
#define SS_WX_HR_H       64
#define SS_WX_HR_CAP_W   44    // caption column at the strip's left
#define SS_WX_BAR_W      20    // rain-chance bar
#define SS_WX_BAR_H      10
#define SS_WX_CARD_Y     200   // forecast cards
#define SS_WX_CARD_H     100
#define SS_WX_CARD_GAP   6
#define SS_WX_FC_ICON    28
// The day marked as today. Amber with G well under R: the panel's weak blue
// channel turns a yellower tint (like CLR_HEX_ACCENT_TINT) olive here, where
// it fills a whole card rather than a small tile.
#define SS_WX_TODAY_TINT 0x2A1A0A

static lv_obj_t *ss_wx_time_lbl = NULL;
static lv_obj_t *ss_wx_now_icon = NULL;
static lv_obj_t *ss_wx_temp_lbl = NULL;
static lv_obj_t *ss_wx_hilo = NULL;      // row: ↑ hi ↓ lo
static lv_obj_t *ss_wx_hi_lbl = NULL;
static lv_obj_t *ss_wx_lo_lbl = NULL;
static lv_obj_t *ss_wx_cond_lbl = NULL;
static lv_obj_t *ss_wx_place_lbl = NULL;
static lv_obj_t *ss_wx_detail_lbl = NULL;
static lv_obj_t *ss_wx_hr_panel = NULL;
static lv_obj_t *ss_wx_hr_time[WEATHER_HOURLY_SLOTS] = {};
static lv_obj_t *ss_wx_hr_temp[WEATHER_HOURLY_SLOTS] = {};
static lv_obj_t *ss_wx_hr_bar[WEATHER_HOURLY_SLOTS] = {}; // fill, inside a track
static lv_obj_t *ss_wx_hr_pct[WEATHER_HOURLY_SLOTS] = {};
static lv_obj_t *ss_wx_card[WEATHER_FORECAST_DAYS] = {};
static lv_obj_t *ss_wx_day_lbl[WEATHER_FORECAST_DAYS] = {};
static lv_obj_t *ss_wx_fc_icon[WEATHER_FORECAST_DAYS] = {};
static lv_obj_t *ss_wx_temp_rng[WEATHER_FORECAST_DAYS] = {};
static lv_obj_t *ss_wx_rain_lbl[WEATHER_FORECAST_DAYS] = {};
// Generation last rendered. The weather block is only rewritten when a fetch
// lands, not on the 1 s clock tick — the icons are object trees, and
// rebuilding them every second would churn the LVGL heap for nothing.
static uint32_t ss_wx_gen_shown = 0;
static bool ss_wx_dirty = true;

static void reset_wx_pointers() {
  ss_wx_time_lbl = ss_wx_now_icon = ss_wx_temp_lbl = NULL;
  ss_wx_hilo = ss_wx_hi_lbl = ss_wx_lo_lbl = NULL;
  ss_wx_cond_lbl = ss_wx_place_lbl = ss_wx_detail_lbl = ss_wx_hr_panel = NULL;
  for (int i = 0; i < WEATHER_HOURLY_SLOTS; i++)
    ss_wx_hr_time[i] = ss_wx_hr_temp[i] = ss_wx_hr_bar[i] = ss_wx_hr_pct[i] = NULL;
  for (int i = 0; i < WEATHER_FORECAST_DAYS; i++)
    ss_wx_card[i] = ss_wx_day_lbl[i] = ss_wx_fc_icon[i] = ss_wx_temp_rng[i] =
        ss_wx_rain_lbl[i] = NULL;
  ss_wx_dirty = true;
}

// ── Thai tone marks ─────────────────────────────────────
// LVGL 8 does no OpenType shaping, so a tone mark after an upper vowel (นี้,
// ชื้น, เพื่อ) is drawn at its resting height — on top of the vowel, where it
// disappears. These font copies route glyph lookup through th_glyph_dsc(),
// which lifts such a tone clear of the vowel, and are TH_HEADROOM px taller
// above the baseline so the lifted mark isn't clipped by the label's box.
//
// It relies on LVGL fetching a line's glyphs in order, which both the
// measuring and drawing passes do. Only this screen uses the copies: the
// extra height would shift every label laid out against the stock fonts.
#define TH_HEADROOM 5

static bool th_upper_vowel(uint32_t c) {
  return c == 0x0E31 || (c >= 0x0E34 && c <= 0x0E37) || c == 0x0E47 ||
         c == 0x0E4D;
}
static bool th_tone(uint32_t c) { return c >= 0x0E48 && c <= 0x0E4C; }

static lv_coord_t s_th_vowel_top = 0; // top of the upper vowel just fetched

static bool th_glyph_dsc(const lv_font_t *f, lv_font_glyph_dsc_t *g,
                         uint32_t c, uint32_t next) {
  if (!lv_font_get_glyph_dsc_fmt_txt(f, g, c, next)) return false;
  if (th_upper_vowel(c))
    s_th_vowel_top = g->ofs_y + g->box_h;
  else if (th_tone(c)) {
    if (s_th_vowel_top && g->ofs_y <= s_th_vowel_top) g->ofs_y = s_th_vowel_top + 1;
  } else
    s_th_vowel_top = 0;
  return true;
}

static lv_font_t s_th_font12, s_th_font14;
static const lv_font_t *th_font(int size) {
  static bool ready = false;
  if (!ready) {
    s_th_font12 = lv_font_montserrat_12;
    s_th_font14 = lv_font_montserrat_14;
    for (lv_font_t *f : {&s_th_font12, &s_th_font14}) {
      f->get_glyph_dsc = th_glyph_dsc;
      f->line_height += TH_HEADROOM;
    }
    ready = true;
  }
  return size == 14 ? &s_th_font14 : &s_th_font12;
}

// ── Weather icons ───────────────────────────────────────
// Drawn from flat shapes rather than a glyph font: the icon font is a
// hand-picked subset with no weather glyphs, and regenerating it for five
// icons is a heavier change than a few circles. Flat fills only — no
// gradients (see the RGB565 notes in ui_helpers.h).
enum WxKind { WX_SUN, WX_PARTLY, WX_CLOUD, WX_FOG, WX_RAIN, WX_SNOW, WX_STORM };

static WxKind wmo_kind(int code) {
  if (code <= 1)  return WX_SUN;
  if (code == 2)  return WX_PARTLY;
  if (code == 3)  return WX_CLOUD;
  if (code <= 48) return WX_FOG;
  if (code <= 67) return WX_RAIN;   // drizzle, rain, freezing rain
  if (code <= 77) return WX_SNOW;
  if (code <= 82) return WX_RAIN;   // rain showers
  if (code <= 86) return WX_SNOW;   // snow showers
  return WX_STORM;                  // 95-99 thunderstorm
}

static lv_obj_t *wx_shape(lv_obj_t *p, int x, int y, int w, int h, int radius,
                          uint32_t hex) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(hex), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

// Cloud: a pill base with two humps, occupying the box width at vertical
// offset `dy`. `k` scales it for the partly-cloudy variant.
static void wx_cloud(lv_obj_t *box, int s, int dy, float k, int dx) {
  const uint32_t c = CLR_HEX_TEXT_MID;
  int bw = (int)(s * 0.80f * k), bh = (int)(s * 0.30f * k);
  int bx = dx + (s - bw) / 2, by = dy + (int)(s * 0.46f);
  int d1 = (int)(s * 0.36f * k), d2 = (int)(s * 0.48f * k);
  wx_shape(box, bx + bw / 8, by - d1 / 2, d1, d1, LV_RADIUS_CIRCLE, c);
  wx_shape(box, bx + bw / 2 - d2 / 3, by - d2 * 2 / 3, d2, d2,
           LV_RADIUS_CIRCLE, c);
  wx_shape(box, bx, by, bw, bh, bh / 2, c);
}

// lv_line keeps a pointer to its points, so they need storage that outlives
// the call. One set per icon slot: 0 = the large "now" icon, 1..N = cards.
static lv_point_t s_bolt_pts[1 + WEATHER_FORECAST_DAYS][4];

static void wx_draw_icon(lv_obj_t *box, int s, int code, int slot) {
  lv_obj_clean(box);
  const WxKind k = wmo_kind(code);
  const bool precip = (k == WX_RAIN || k == WX_SNOW || k == WX_STORM);
  const int lift = precip ? -(int)(s * 0.14f) : 0; // room for what falls

  switch (k) {
  case WX_SUN: {
    int d = (int)(s * 0.56f);
    lv_obj_t *sun = wx_shape(box, (s - d) / 2, (s - d) / 2, d, d,
                             LV_RADIUS_CIRCLE, CLR_HEX_ACCENT);
    // A soft halo reads as light without a gradient fill.
    lv_obj_set_style_shadow_color(sun, lv_color_hex(CLR_HEX_ACCENT), 0);
    lv_obj_set_style_shadow_width(sun, s / 4, 0);
    lv_obj_set_style_shadow_opa(sun, LV_OPA_30, 0);
    break;
  }
  case WX_PARTLY: {
    int d = (int)(s * 0.42f);
    wx_shape(box, (int)(s * 0.08f), (int)(s * 0.10f), d, d, LV_RADIUS_CIRCLE,
             CLR_HEX_ACCENT);
    wx_cloud(box, s, (int)(s * 0.10f), 0.85f, (int)(s * 0.08f));
    break;
  }
  case WX_FOG: {
    int h = s / 11 < 2 ? 2 : s / 11;
    for (int i = 0; i < 3; i++)
      wx_shape(box, (int)(s * (i == 1 ? 0.10f : 0.20f)),
               (int)(s * (0.28f + 0.20f * i)), (int)(s * (i == 1 ? 0.80f : 0.60f)),
               h, h / 2, CLR_HEX_TEXT_LOW);
    break;
  }
  default:
    wx_cloud(box, s, lift, 1.0f, 0);
    break;
  }

  const int fall_y = (int)(s * 0.70f);
  if (k == WX_RAIN) {
    int w = s / 14 < 2 ? 2 : s / 14, h = (int)(s * 0.20f);
    for (int i = 0; i < 3; i++)
      wx_shape(box, (int)(s * (0.28f + 0.22f * i)) - w / 2, fall_y, w, h, w / 2,
               CLR_HEX_COOL);
  } else if (k == WX_SNOW) {
    int d = s / 8 < 3 ? 3 : s / 8;
    for (int i = 0; i < 3; i++)
      wx_shape(box, (int)(s * (0.28f + 0.22f * i)) - d / 2,
               fall_y + (i == 1 ? d : 0), d, d, LV_RADIUS_CIRCLE, CLR_HEX_TEXT_HI);
  } else if (k == WX_STORM) {
    lv_point_t *p = s_bolt_pts[slot];
    p[0] = {(lv_coord_t)(s * 0.56f), (lv_coord_t)(s * 0.58f)};
    p[1] = {(lv_coord_t)(s * 0.42f), (lv_coord_t)(s * 0.78f)};
    p[2] = {(lv_coord_t)(s * 0.58f), (lv_coord_t)(s * 0.78f)};
    p[3] = {(lv_coord_t)(s * 0.44f), (lv_coord_t)(s * 0.98f)};
    lv_obj_t *bolt = lv_line_create(box);
    lv_line_set_points(bolt, p, 4);
    lv_obj_set_style_line_color(bolt, lv_color_hex(CLR_HEX_ACCENT), 0);
    lv_obj_set_style_line_width(bolt, s / 14 < 2 ? 2 : s / 14, 0);
    lv_obj_set_style_line_rounded(bolt, true, 0);
  }
}

// Plain container: no theme style, no scroll, and taps fall through to wake.
static lv_obj_t *wx_box(lv_obj_t *p, int w, int h) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, w, h);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

static lv_obj_t *wx_icon_box(lv_obj_t *p, int s) { return wx_box(p, s, s); }

static lv_obj_t *wx_label(lv_obj_t *p, const lv_font_t *font, uint32_t hex) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(hex), 0);
  lv_label_set_text(l, "");
  return l;
}

// Flat card surface, shared by the hourly strip and the day cards.
static lv_obj_t *wx_card(lv_obj_t *p, int w, int h, bool today) {
  lv_obj_t *c = wx_box(p, w, h);
  lv_obj_set_style_bg_color(
      c, lv_color_hex(today ? SS_WX_TODAY_TINT : CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(
      c, lv_color_hex(today ? CLR_HEX_ACCENT : CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_opa(c, today ? LV_OPA_60 : LV_OPA_70, 0);
  lv_obj_set_style_radius(c, 10, 0);
  return c;
}

// ↑ / ↓ beside today's high and low. Drawn rather than typed: U+2191/2193
// aren't in the generated fonts, and LV_SYMBOL_UP/DOWN are chevrons that
// read as ^ and v. Each is one polyline: head, shaft, and back up the shaft.
static const lv_point_t s_arrow_up[5] = {{0, 3}, {3, 0}, {3, 10}, {3, 0}, {6, 3}};
static const lv_point_t s_arrow_dn[5] = {{0, 7}, {3, 10}, {3, 0}, {3, 10}, {6, 7}};

static lv_obj_t *wx_arrow(lv_obj_t *p, const lv_point_t *pts) {
  lv_obj_t *l = lv_line_create(p);
  lv_line_set_points(l, pts, 5);
  lv_obj_set_style_line_color(l, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_set_style_line_width(l, 1, 0);
  lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
  return l;
}

static void build_weather_style(lv_obj_t *wrap) {
  const lv_font_t *th12 = th_font(12), *th14 = th_font(14);
  const int inner_w = SCREEN_WIDTH + 20 - SS_SIDE_PAD * 2;

  // Top row: connectivity on the left, the forecast's age in the middle; the
  // caller's wake hint takes the right. The y values put every baseline on
  // the same line despite the Thai fonts' extra headroom.
  ss_label_mqtt_wifi = wx_label(wrap, &lv_font_montserrat_14, CLR_HEX_TEXT_LOW);
  lv_label_set_recolor(ss_label_mqtt_wifi, true);
  lv_obj_set_pos(ss_label_mqtt_wifi, SS_SIDE_PAD, 12);

  ss_label_sysinfo = wx_label(wrap, th12, CLR_HEX_TEXT_LOW);
  lv_obj_align(ss_label_sysinfo, LV_ALIGN_TOP_MID, 0, 9);

  // Upper-left: clock and date. The clock is the same size as the Minimal
  // style's so switching styles doesn't change how far away it can be read.
  ss_wx_time_lbl = wx_label(wrap, &lv_font_montserrat_48, CLR_HEX_TEXT_HI);
  lv_obj_set_style_text_letter_space(ss_wx_time_lbl, 3, 0);
  lv_label_set_text(ss_wx_time_lbl, currentTime);
  lv_obj_align(ss_wx_time_lbl, LV_ALIGN_TOP_LEFT, SS_SIDE_PAD, SS_WX_TOP_Y);

  ss_meridiem_lbl = wx_label(wrap, &lv_font_montserrat_14, CLR_HEX_TEXT_LOW);
  lv_label_set_text(ss_meridiem_lbl, currentMeridiem);
  lv_obj_align_to(ss_meridiem_lbl, ss_wx_time_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 4,
                  -8);

  ss_label_date = wx_label(wrap, th14, CLR_HEX_TEXT_MID);
  lv_label_set_long_mode(ss_label_date, LV_LABEL_LONG_DOT);
  lv_obj_set_width(ss_label_date, inner_w - SS_WX_COL_W - 20);
  lv_label_set_text(ss_label_date, currentDate);
  lv_obj_align(ss_label_date, LV_ALIGN_TOP_LEFT, SS_SIDE_PAD, SS_WX_TOP_Y + 52);

  // Hairline between the two halves of the upper band
  lv_obj_t *vrule = wx_box(wrap, 1, 84);
  lv_obj_set_style_bg_color(vrule, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_bg_opa(vrule, LV_OPA_70, 0);
  lv_obj_align(vrule, LV_ALIGN_TOP_RIGHT, -(SS_SIDE_PAD + SS_WX_COL_W + 10),
               SS_WX_TOP_Y + 4);

  // Upper-right: today's conditions
  lv_obj_t *col = wx_box(wrap, SS_WX_COL_W, 94);
  lv_obj_align(col, LV_ALIGN_TOP_RIGHT, -SS_SIDE_PAD, SS_WX_TOP_Y);

  ss_wx_now_icon = wx_icon_box(col, SS_WX_NOW_ICON);
  lv_obj_set_pos(ss_wx_now_icon, 0, 2);

  const int tx = SS_WX_NOW_ICON + 10;
  ss_wx_temp_lbl = wx_label(col, &lv_font_montserrat_24, CLR_HEX_TEXT_HI);
  lv_obj_set_pos(ss_wx_temp_lbl, tx, 0);

  // ↑hi ↓lo, one line on the reading's baseline; x is set once the reading's
  // width is known.
  ss_wx_hilo = wx_box(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(ss_wx_hilo, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ss_wx_hilo, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(ss_wx_hilo, 3, 0);
  lv_obj_set_y(ss_wx_hilo, 9);
  wx_arrow(ss_wx_hilo, s_arrow_up);
  ss_wx_hi_lbl = wx_label(ss_wx_hilo, &lv_font_montserrat_12, CLR_HEX_TEXT_MID);
  wx_arrow(ss_wx_hilo, s_arrow_dn);
  ss_wx_lo_lbl = wx_label(ss_wx_hilo, &lv_font_montserrat_12, CLR_HEX_TEXT_LOW);

  ss_wx_cond_lbl = wx_label(col, th14, CLR_HEX_TEXT_HI);
  lv_label_set_long_mode(ss_wx_cond_lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(ss_wx_cond_lbl, SS_WX_COL_W - tx);
  lv_obj_set_pos(ss_wx_cond_lbl, tx, 24);

  ss_wx_place_lbl = wx_label(col, th12, CLR_HEX_TEXT_MID);
  lv_label_set_long_mode(ss_wx_place_lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(ss_wx_place_lbl, SS_WX_COL_W);
  lv_obj_set_pos(ss_wx_place_lbl, 0, 48);

  ss_wx_detail_lbl = wx_label(col, th12, CLR_HEX_TEXT_LOW);
  lv_label_set_long_mode(ss_wx_detail_lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(ss_wx_detail_lbl, SS_WX_COL_W);
  lv_obj_set_pos(ss_wx_detail_lbl, 0, 68);

  // Hourly strip: a caption column, then one slot per hour — time, reading,
  // a rain-chance bar and its figure. The first slot is the next hour.
  ss_wx_hr_panel = wx_card(wrap, inner_w, SS_WX_HR_H, false);
  lv_obj_set_pos(ss_wx_hr_panel, SS_SIDE_PAD, SS_WX_HR_Y);
  lv_obj_add_flag(ss_wx_hr_panel, LV_OBJ_FLAG_HIDDEN); // shown once there's data

  lv_obj_t *cap = wx_label(ss_wx_hr_panel, th12, CLR_HEX_TEXT_LOW);
  lv_label_set_text(cap, L(L_WX_HOURLY));
  lv_obj_set_pos(cap, 10, 0);
  lv_obj_t *cap_rain = wx_label(ss_wx_hr_panel, &lv_font_montserrat_12, CLR_HEX_COOL);
  lv_label_set_text(cap_rain, LV_SYMBOL_TINT " %");
  lv_obj_set_pos(cap_rain, 10, 46);

  const int slot_w = (inner_w - SS_WX_HR_CAP_W - 4) / WEATHER_HOURLY_SLOTS;
  for (int i = 0; i < WEATHER_HOURLY_SLOTS; i++) {
    lv_obj_t *slot = wx_box(ss_wx_hr_panel, slot_w, SS_WX_HR_H - 2);
    lv_obj_set_pos(slot, SS_WX_HR_CAP_W + i * slot_w, 0);

    ss_wx_hr_time[i] = wx_label(slot, &lv_font_montserrat_12,
                                i == 0 ? CLR_HEX_ACCENT_HI : CLR_HEX_TEXT_LOW);
    lv_obj_align(ss_wx_hr_time[i], LV_ALIGN_TOP_MID, 0, 4);

    ss_wx_hr_temp[i] = wx_label(slot, &lv_font_montserrat_14, CLR_HEX_TEXT_HI);
    lv_obj_align(ss_wx_hr_temp[i], LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *track = wx_box(slot, SS_WX_BAR_W, SS_WX_BAR_H);
    lv_obj_set_style_bg_color(track, lv_color_hex(CLR_HEX_HAIRLINE), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_50, 0);
    lv_obj_set_style_radius(track, 2, 0);
    lv_obj_align(track, LV_ALIGN_TOP_MID, 0, 37);
    ss_wx_hr_bar[i] = wx_box(track, SS_WX_BAR_W, 0);
    lv_obj_set_style_bg_opa(ss_wx_hr_bar[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ss_wx_hr_bar[i], 2, 0);
    lv_obj_align(ss_wx_hr_bar[i], LV_ALIGN_BOTTOM_MID, 0, 0);

    ss_wx_hr_pct[i] = wx_label(slot, &lv_font_montserrat_12, CLR_HEX_TEXT_LOW);
    lv_obj_align(ss_wx_hr_pct[i], LV_ALIGN_TOP_MID, 0, 46);
  }

  // Forecast cards. Today's carries the accent outline, the only place amber
  // marks a surface on this screen besides the icons.
  const int card_w =
      (inner_w - SS_WX_CARD_GAP * (WEATHER_FORECAST_DAYS - 1)) / WEATHER_FORECAST_DAYS;
  for (int i = 0; i < WEATHER_FORECAST_DAYS; i++) {
    lv_obj_t *c = wx_card(wrap, card_w, SS_WX_CARD_H, i == 0);
    lv_obj_set_pos(c, SS_SIDE_PAD + i * (card_w + SS_WX_CARD_GAP), SS_WX_CARD_Y);
    lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN); // shown once a day has data
    ss_wx_card[i] = c;

    ss_wx_day_lbl[i] =
        wx_label(c, th12, i == 0 ? CLR_HEX_ACCENT_HI : CLR_HEX_TEXT_MID);
    lv_obj_align(ss_wx_day_lbl[i], LV_ALIGN_TOP_MID, 0, 3);

    ss_wx_fc_icon[i] = wx_icon_box(c, SS_WX_FC_ICON);
    lv_obj_align(ss_wx_fc_icon[i], LV_ALIGN_TOP_MID, 0, 28);

    ss_wx_temp_rng[i] = wx_label(c, &lv_font_montserrat_14, CLR_HEX_TEXT_HI);
    lv_label_set_recolor(ss_wx_temp_rng[i], true);
    lv_obj_align(ss_wx_temp_rng[i], LV_ALIGN_TOP_MID, 0, 58);

    ss_wx_rain_lbl[i] = wx_label(c, &lv_font_montserrat_12, CLR_HEX_TEXT_LOW);
    lv_obj_align(ss_wx_rain_lbl[i], LV_ALIGN_TOP_MID, 0, 76);
  }
}

// Likely rain is worth a colour; a 10 % chance is not.
static uint32_t wx_rain_hex(int pct) {
  return pct >= 60 ? CLR_HEX_COOL : CLR_HEX_TEXT_LOW;
}

// Rewrites everything that comes from the weather fetch. Called on build and
// whenever weatherGeneration moves.
static void refresh_weather_style() {
  char buf[96];
  if (!ss_wx_temp_lbl) return;

  if (weatherValid) {
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", weatherTemp);
    lv_label_set_text(ss_wx_temp_lbl, buf);
    lv_label_set_text(ss_wx_cond_lbl, wmoToDesc(weatherCode));
    wx_draw_icon(ss_wx_now_icon, SS_WX_NOW_ICON, weatherCode, 0);
  } else {
    lv_label_set_text(ss_wx_temp_lbl, "--\xC2\xB0");
    lv_label_set_text(ss_wx_cond_lbl, "");
    lv_obj_clean(ss_wx_now_icon);
  }

  if (weatherValid && weatherForecastDays > 0) {
    snprintf(buf, sizeof(buf), "%d\xC2\xB0 ", weatherForecast[0].hi);
    lv_label_set_text(ss_wx_hi_lbl, buf);
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", weatherForecast[0].lo);
    lv_label_set_text(ss_wx_lo_lbl, buf);
    lv_obj_clear_flag(ss_wx_hilo, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ss_wx_hilo, LV_OBJ_FLAG_HIDDEN);
  }
  // The hi/lo pair tucks in just right of however wide the reading is.
  lv_obj_update_layout(ss_wx_temp_lbl);
  lv_obj_set_x(ss_wx_hilo,
               lv_obj_get_x(ss_wx_temp_lbl) + lv_obj_get_width(ss_wx_temp_lbl) + 8);

  const char *place = weatherCityName[0] ? weatherCityName : weatherCity;
  snprintf(buf, sizeof(buf), LV_SYMBOL_GPS "  %s", place);
  trim_partial_utf8(buf);
  lv_label_set_text(ss_wx_place_lbl, buf);

  if (weatherValid) {
    snprintf(buf, sizeof(buf), "%s %.0f\xC2\xB0 \xC2\xB7 %s %d%% \xC2\xB7 %s %.0f %s",
             L(L_WX_FEELS), weatherFeels, L(L_WX_HUM), weatherHumidity,
             L(L_WX_WIND), weatherWind, L(L_WX_KMH));
    trim_partial_utf8(buf);
    lv_label_set_text(ss_wx_detail_lbl, buf);
  } else {
    lv_label_set_text(ss_wx_detail_lbl, "");
  }

  // Hourly strip
  const bool have_hours = weatherValid && weatherHourlyCount > 0;
  if (have_hours) lv_obj_clear_flag(ss_wx_hr_panel, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(ss_wx_hr_panel, LV_OBJ_FLAG_HIDDEN);
  for (int i = 0; have_hours && i < WEATHER_HOURLY_SLOTS; i++) {
    lv_obj_t *slot = lv_obj_get_parent(ss_wx_hr_time[i]);
    if (i >= weatherHourlyCount) {
      lv_obj_add_flag(slot, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(slot, LV_OBJ_FLAG_HIDDEN);
    const WeatherHour &h = weatherHourly[i];
    if (use24HourFormat)
      snprintf(buf, sizeof(buf), "%02d:00", h.hour);
    else
      snprintf(buf, sizeof(buf), "%d%s", h.hour % 12 ? h.hour % 12 : 12,
               h.hour < 12 ? "am" : "pm");
    lv_label_set_text(ss_wx_hr_time[i], buf);
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", h.temp);
    lv_label_set_text(ss_wx_hr_temp[i], buf);

    if (h.rainPct >= 0) {
      snprintf(buf, sizeof(buf), "%d%%", h.rainPct);
      lv_obj_set_style_text_color(ss_wx_hr_pct[i], lv_color_hex(wx_rain_hex(h.rainPct)), 0);
      int bh = h.rainPct * SS_WX_BAR_H / 100;
      lv_obj_set_height(ss_wx_hr_bar[i], bh < 2 ? 2 : bh);
      lv_obj_set_style_bg_color(ss_wx_hr_bar[i], lv_color_hex(wx_rain_hex(h.rainPct)), 0);
      lv_obj_clear_flag(ss_wx_hr_bar[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      snprintf(buf, sizeof(buf), "\xE2\x80\x93");
      lv_obj_add_flag(ss_wx_hr_bar[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(ss_wx_hr_pct[i], buf);
  }

  static const LangKey day_keys[7] = {L_DAYL_SU, L_DAYL_MO, L_DAYL_TU, L_DAYL_WE,
                                      L_DAYL_TH, L_DAYL_FR, L_DAYL_SA};
  for (int i = 0; i < WEATHER_FORECAST_DAYS; i++) {
    if (!ss_wx_card[i]) continue;
    if (!weatherValid || i >= weatherForecastDays) {
      lv_obj_add_flag(ss_wx_card[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    const WeatherDay &d = weatherForecast[i];
    lv_obj_clear_flag(ss_wx_card[i], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ss_wx_day_lbl[i],
                      i == 0 ? L(L_TODAY) : L(day_keys[d.wday % 7]));
    wx_draw_icon(ss_wx_fc_icon[i], SS_WX_FC_ICON, d.code, 1 + i);
    snprintf(buf, sizeof(buf), "%d\xC2\xB0 #9C9CA8 %d\xC2\xB0#", d.hi, d.lo);
    lv_label_set_text(ss_wx_temp_rng[i], buf);
    if (d.rainPct >= 0) {
      snprintf(buf, sizeof(buf), LV_SYMBOL_TINT " %d%%", d.rainPct);
      lv_label_set_text(ss_wx_rain_lbl[i], buf);
      lv_obj_set_style_text_color(ss_wx_rain_lbl[i],
                                  lv_color_hex(wx_rain_hex(d.rainPct)), 0);
    } else {
      lv_label_set_text(ss_wx_rain_lbl[i], "");
    }
  }

  if (weatherUpdatedAt[0])
    snprintf(buf, sizeof(buf), "%s %s \xC2\xB7 Open-Meteo", L(L_WX_UPDATED),
             weatherUpdatedAt);
  else
    snprintf(buf, sizeof(buf), "Open-Meteo");
  trim_partial_utf8(buf);
  lv_label_set_text(ss_label_sysinfo, buf);

  ss_wx_gen_shown = weatherGeneration;
  ss_wx_dirty = false;
}

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
    reset_wx_pointers();
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
  reset_wx_pointers();
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

  // ====== STYLE 3: WEATHER ======
  if (screensaverStyle == 3) {
    // "แตะเพื่อปลดล็อก" stacks a tone on a vowel; the Thai-aware font keeps it
    // visible, and y moves up by its extra headroom to hold the baseline.
    lv_obj_set_style_text_font(ss_hint, th_font(12), 0);
    lv_obj_align(ss_hint, LV_ALIGN_TOP_RIGHT, -SS_SIDE_PAD, 14 - TH_HEADROOM);
    build_weather_style(ss_content_wrap);
    return;
  }

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

  // Status row — weather on the left, connectivity on the right.
  //
  // A flex row rather than two absolute widths. The weather string is the one
  // that varies: a Thai condition and a Thai place name are far longer than
  // "Partly Cloudy · Bangkok", and the old fixed 250 px cut them off. Letting
  // it take whatever the connectivity label does not use means the split
  // follows the text instead of a number someone guessed once — and the
  // connectivity side changes width too, as the device count does.
  lv_obj_t *status_row = create_ss_row(ss_content_wrap, SS_STATUS_Y);
  lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(status_row, 14, 0);

  ss_label_sysinfo = lv_label_create(status_row);
  lv_obj_set_style_text_font(ss_label_sysinfo, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ss_label_sysinfo, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_label_set_long_mode(ss_label_sysinfo, LV_LABEL_LONG_DOT);
  // Fixed height pins it to one line; the width comes from the flex grow.
  lv_obj_set_height(ss_label_sysinfo, SS_ROW_H);
  lv_obj_set_flex_grow(ss_label_sysinfo, 1);
  lv_label_set_text(ss_label_sysinfo, "");

  ss_label_mqtt_wifi = lv_label_create(status_row);
  lv_obj_set_style_text_font(ss_label_mqtt_wifi, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ss_label_mqtt_wifi, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_label_set_recolor(ss_label_mqtt_wifi, true);
  lv_label_set_text(ss_label_mqtt_wifi, "");

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
  // UTF-8 makes this far longer than it reads. The condition comes from a
  // runtime translation — Thai's "มีเมฆบางส่วน" alone is 36 bytes — and a Thai
  // city name fills weatherCityName to its last byte, so "27°  <condition>  ·
  // <city>" reaches 75 bytes where the English it was sized against reached 30.
  // At 64 the city was cut off the end, mid-sequence, and the remnant rendered
  // as a missing-glyph box. Sized for both runtime strings at full width plus
  // the separators, with room for a longer translation than any shipping today.
  char wBuf[160] = "";
  if (weatherValid) {
    if (weatherCityName[0])
      snprintf(wBuf, sizeof(wBuf), "%.0f\xC2\xB0""  %s  \xC2\xB7  %s",
               weatherTemp, wmoToDesc(weatherCode), weatherCityName);
    else
      snprintf(wBuf, sizeof(wBuf), "%.0f\xC2\xB0""  %s",
               weatherTemp, wmoToDesc(weatherCode));
    trim_partial_utf8(wBuf);
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

  // --- Style 3: Weather ---
  if (screensaverStyle == 3) {
    if (ss_wx_time_lbl &&
        strcmp(lv_label_get_text(ss_wx_time_lbl), currentTime) != 0) {
      lv_label_set_text(ss_wx_time_lbl, currentTime);
      lv_obj_align_to(ss_meridiem_lbl, ss_wx_time_lbl,
                      LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);
    }
    if (ss_label_date &&
        strcmp(lv_label_get_text(ss_label_date), currentDate) != 0)
      lv_label_set_text(ss_label_date, currentDate);
    if (ss_wx_dirty || ss_wx_gen_shown != weatherGeneration)
      refresh_weather_style();
    if (ss_label_mqtt_wifi)
      lv_label_set_text(ss_label_mqtt_wifi, netBuf);
    return;
  }

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
