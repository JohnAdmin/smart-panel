#pragma once
// ========================================================
//  Shared UI Helpers — eliminates duplicated styling code
// ========================================================
#include "ui_screens.h"
#include <lvgl.h>

// ────────────────────────────────────────────────────────
//  DESIGN TOKENS
//  One accent (amber), one neutral ramp (cool slate), one
//  radius scale. Every screen pulls from these — changing a
//  token here restyles the whole panel.
// ────────────────────────────────────────────────────────

// ── Layout / dimensions ─────────────────────────────────
#define UI_HEADER_HEIGHT       44
#define UI_TABBAR_HEIGHT       40
#define UI_SETTINGS_HDR_HEIGHT 52
#define UI_PILL_BTN_W          44
#define UI_PILL_BTN_H          34
#define UI_PILL_RADIUS         10
#define UI_CARD_RADIUS         16
#define UI_TILE_RADIUS         18
#define UI_TILE_W              140
#define UI_TILE_H              104
#define UI_TILE_LARGE_W        220
#define UI_FAV_TILE_W          136
#define UI_FAV_TILE_H          100
#define UI_WEATHER_CARD_W      168
#define UI_MAX_FAV_NORMAL      6
#define UI_MAX_FAV_LARGE       4
#define UI_TOAST_DISMISS_MS    2000
#define UI_MODAL_W             360
#define UI_MODAL_H             180
#define UI_TEXTAREA_H          42

// ── Neutral ramp ────────────────────────────────────────
// Tuned for the ST7796, not for a desktop monitor. The previous slate ramp
// carried G above R by 4–11 counts and leaned on blue to look cool — but this
// panel's blue subpixel is the dimmest, so at surface-level brightness the
// blue was crushed and what survived was R plus a stronger G: a visible green
// cast on every card and button.
//
// Fix: keep R and G equal so nothing tilts toward green, let blue carry the
// coolness on its own, and lift the darkest step clear of the panel's
// black-crush region where the cast was worst.
//
// If a trace of green still shows on your unit, subtract 2–3 from the middle
// (G) byte of these four values — that is the only knob that matters.
#define CLR_HEX_SURFACE_0      0x16161C  // app scrim: header, tab bar
#define CLR_HEX_SURFACE_1      0x202028  // resting card / tile
#define CLR_HEX_SURFACE_2      0x2C2C36  // raised element: icon badge, chip, input
#define CLR_HEX_HAIRLINE       0x3E3E4A  // 1 px separators and tile outlines

// ── Text ramp (same R = G rule) ─────────────────────────
#define CLR_HEX_TEXT_HI        0xF8F8FC  // primary
#define CLR_HEX_TEXT_MID       0xB2B2BC  // secondary
#define CLR_HEX_TEXT_LOW       0x7C7C88  // tertiary / inactive

// ── Accent + semantics ──────────────────────────────────
#define CLR_HEX_ACCENT         0xF59E0B  // amber — the single accent
#define CLR_HEX_ACCENT_HI      0xFBBF24  // amber, lifted (text on dark tint)
#define CLR_HEX_ACCENT_TINT    0x2C2008  // amber @ ~12 % — ON-tile fill (top)
#define CLR_HEX_ACCENT_TINT_LO 0x1A1305  // ON-tile fill (bottom of gradient)
#define CLR_HEX_OK             0x34D399  // connected / active
#define CLR_HEX_DANGER         0xEF4444  // destructive actions (delete, all-off)
#define CLR_HEX_DANGER_HI      0xFCA5A5  // danger text on a dark red tint

// ── Device tile ON-state look ───────────────────────────
// 0 = Luminous (default): amber-tinted dark fill + amber glow. Keeps the dark
//     theme intact and stays comfortable at night.
// 1 = Illuminated card: white fill with dark text (Apple-Home look).
#define UI_TILE_ON_STYLE       0

// ── Legacy aliases — mapped onto the ramp above so older screens
//    (settings, device manager, scenes, modals) inherit the refresh.
#define CLR_HEX_HEADER_BG      CLR_HEX_SURFACE_0
#define CLR_HEX_PILL_BG        CLR_HEX_SURFACE_2
#define CLR_HEX_PILL_BORDER    CLR_HEX_HAIRLINE
#define CLR_HEX_CARD_BG        CLR_HEX_SURFACE_1
#define CLR_HEX_CARD_BORDER    CLR_HEX_HAIRLINE
#define CLR_HEX_TOAST_BG       CLR_HEX_SURFACE_2
#define CLR_HEX_MUTED_ICON     CLR_HEX_TEXT_MID
#define CLR_HEX_DARK_BORDER    CLR_HEX_HAIRLINE
#define CLR_HEX_DARK_BORDER2   CLR_HEX_HAIRLINE
#define CLR_HEX_FROSTED_BG     CLR_HEX_SURFACE_0
#define CLR_HEX_BADGE_BG       CLR_HEX_SURFACE_2
#define CLR_HEX_ON_ACCENT      0x111827  // label color on an amber accent fill

// Accent buttons carry a colored glow instead of a border.
#define UI_ACCENT_GLOW_W       12

// ── Pill Button Factory ─────────────────────────────────
// Creates a styled pill button inside `parent` with an icon/text label.
// Returns the button object (label is created internally, reachable as
// lv_obj_get_child(btn, 0) when the caller needs lv_label_set_text_fmt).
//
// `filter` defaults to LV_EVENT_ALL, which requires the callback to check
// lv_event_get_code() itself. Pass LV_EVENT_CLICKED for callbacks that don't.
static inline lv_obj_t *ui_create_pill_btn(lv_obj_t *parent, int w, int h,
                                            const char *text, lv_color_t text_color,
                                            lv_event_cb_t cb, void *user_data = NULL,
                                            lv_event_code_t filter = LV_EVENT_ALL) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_HEX_PILL_BG), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(CLR_HEX_PILL_BORDER), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_opa(btn, LV_OPA_80, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_radius(btn, UI_PILL_RADIUS, 0);
  // Press feedback: lift the fill and light the outline in accent.
  lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_HEX_HAIRLINE), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(btn, lv_color_hex(CLR_HEX_ACCENT), LV_STATE_PRESSED);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, text_color, 0);
  lv_obj_center(lbl);
  if (cb) lv_obj_add_event_cb(btn, cb, filter, user_data);
  return btn;
}

// ── Accent Button Factory ───────────────────────────────
// The amber primary-action button (Save, Add) — solid CLR_PRIMARY fill, no
// border, matching glow, black 12 px label. Same conventions as
// ui_create_pill_btn for the label and `filter`.
static inline lv_obj_t *ui_create_accent_btn(lv_obj_t *parent, int w, int h,
                                              const char *text, lv_event_cb_t cb,
                                              void *user_data = NULL,
                                              lv_event_code_t filter = LV_EVENT_ALL) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, CLR_PRIMARY, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_color(btn, CLR_PRIMARY, 0);
  lv_obj_set_style_shadow_width(btn, UI_ACCENT_GLOW_W, 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_40, 0);
  lv_obj_set_style_radius(btn, UI_PILL_RADIUS, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_HEX_ACCENT_HI), LV_STATE_PRESSED);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_ON_ACCENT), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(lbl);
  if (cb) lv_obj_add_event_cb(btn, cb, filter, user_data);
  return btn;
}

// ── Surface ─────────────────────────────────────────────
// The one raised-panel look used by tiles, cards and list rows: a soft
// vertical gradient, a hairline outline and a neutral drop shadow. Opacity is
// high enough that a bright wallpaper can't wash the content out — at 80 % the
// settings forms turned the colour of whatever image was behind them.
static inline void ui_style_surface(lv_obj_t *obj, int radius) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_color(obj, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_set_style_shadow_color(obj, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(obj, 14, 0);
  lv_obj_set_style_shadow_ofs_y(obj, 4, 0);
  lv_obj_set_style_shadow_opa(obj, LV_OPA_40, 0);
}

// ── Glass Card Factory ──────────────────────────────────
// Creates a semi-transparent dark card with subtle border — matches the
// design system used across settings, device manager, and dimmer modal.
static inline lv_obj_t *ui_create_glass_card(lv_obj_t *parent, int w, int h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, w, h);
  ui_style_surface(card, UI_CARD_RADIUS);
  lv_obj_set_style_pad_all(card, 16, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

// ── Textarea States ─────────────────────────────────────
// Focus ring, placeholder and cursor. Without these the field gives no sign
// it is the one receiving keystrokes.
static inline void ui_style_textarea_states(lv_obj_t *ta) {
  lv_obj_set_style_border_color(ta, lv_color_hex(CLR_HEX_ACCENT), LV_STATE_FOCUSED);
  lv_obj_set_style_border_opa(ta, LV_OPA_COVER, LV_STATE_FOCUSED);
  lv_obj_set_style_text_color(ta, lv_color_hex(CLR_HEX_TEXT_LOW),
                              LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_set_style_bg_color(ta, lv_color_hex(CLR_HEX_ACCENT), LV_PART_CURSOR);
  lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, LV_PART_CURSOR);
}

// ── Styled Textarea Factory ─────────────────────────────
// Creates a dark-themed single-line textarea for forms.
static inline lv_obj_t *ui_create_textarea(lv_obj_t *parent, int w,
                                            const char *placeholder,
                                            const char *initial_text,
                                            lv_event_cb_t kb_cb) {
  lv_obj_t *ta = lv_textarea_create(parent);
  lv_obj_set_size(ta, w, UI_TEXTAREA_H);
  lv_textarea_set_placeholder_text(ta, placeholder);
  if (initial_text && initial_text[0]) lv_textarea_set_text(ta, initial_text);
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_style_bg_color(ta, lv_color_hex(CLR_HEX_SURFACE_2), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_radius(ta, UI_PILL_RADIUS, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  ui_style_textarea_states(ta);
  if (kb_cb) lv_obj_add_event_cb(ta, kb_cb, LV_EVENT_ALL, NULL);
  return ta;
}

// ── Form Textarea Factory ───────────────────────────────
// The full-width field used on the stacked edit forms (device, scene). Darker
// card fill than ui_create_textarea's pill styling, and the height varies by
// form density — 40 px on the device editor, 36 px on the scene editor.
static inline lv_obj_t *ui_create_form_textarea(lv_obj_t *parent, int height,
                                                 const char *placeholder,
                                                 lv_event_cb_t cb,
                                                 void *user_data = NULL) {
  lv_obj_t *ta = lv_textarea_create(parent);
  lv_obj_set_width(ta, lv_pct(100));
  lv_obj_set_height(ta, height);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_obj_set_style_bg_color(ta, lv_color_hex(CLR_HEX_SURFACE_2), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_radius(ta, UI_PILL_RADIUS, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  ui_style_textarea_states(ta);
  if (cb) lv_obj_add_event_cb(ta, cb, LV_EVENT_ALL, user_data);
  return ta;
}

// ── Frosted Header Factory ──────────────────────────────
// Creates the semi-transparent header bar used on settings/sub-screens.
static inline lv_obj_t *ui_create_frosted_header(lv_obj_t *parent, int height) {
  lv_obj_t *hdr = lv_obj_create(parent);
  lv_obj_set_size(hdr, SCREEN_WIDTH, height);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(hdr, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_opa(hdr, LV_OPA_90, 0);
  lv_obj_set_style_border_color(hdr, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(hdr, LV_OPA_70, 0);
  lv_obj_set_style_border_width(hdr, 1, 0);
  lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_shadow_width(hdr, 0, 0);
  lv_obj_set_style_radius(hdr, 0, 0);
  lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
  return hdr;
}

// ── Hairline Divider ────────────────────────────────────
// A 1 px rule used to separate blocks inside a card.
static inline lv_obj_t *ui_create_divider(lv_obj_t *parent, int w) {
  lv_obj_t *ln = lv_obj_create(parent);
  lv_obj_set_size(ln, w, 1);
  lv_obj_set_style_bg_color(ln, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_bg_opa(ln, LV_OPA_80, 0);
  lv_obj_set_style_border_width(ln, 0, 0);
  lv_obj_set_style_radius(ln, 0, 0);
  lv_obj_set_style_pad_all(ln, 0, 0);
  lv_obj_clear_flag(ln, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return ln;
}

// ── Status Dot ──────────────────────────────────────────
// Small filled circle used as a state indicator next to a label.
static inline lv_obj_t *ui_create_dot(lv_obj_t *parent, int d, lv_color_t c) {
  lv_obj_t *dot = lv_obj_create(parent);
  lv_obj_set_size(dot, d, d);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, c, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_shadow_width(dot, 0, 0);
  lv_obj_set_style_pad_all(dot, 0, 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return dot;
}

// ── Scrollbar ───────────────────────────────────────────
// Slim neutral scrollbar — an affordance, not decoration.
static inline void ui_style_scrollbar(lv_obj_t *obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(CLR_HEX_TEXT_LOW), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(obj, LV_OPA_40, LV_PART_SCROLLBAR);
  lv_obj_set_style_width(obj, 3, LV_PART_SCROLLBAR);
  lv_obj_set_style_pad_right(obj, 2, LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(obj, 2, LV_PART_SCROLLBAR);
}

// ── Form Control Stylers ────────────────────────────────
// LVGL's stock theme paints sliders, switches, dropdown lists and the keyboard
// in its default blue-on-light palette. These bring them onto the token ramp.
// Call right after creating the widget.

static inline void ui_style_slider(lv_obj_t *s) {
  lv_obj_set_style_bg_color(s, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(s, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_MAIN);

  lv_obj_set_style_bg_color(s, lv_color_hex(CLR_HEX_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

  // White knob lifted by a neutral shadow — no coloured halo
  lv_obj_set_style_bg_color(s, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_border_width(s, 0, LV_PART_KNOB);
  lv_obj_set_style_shadow_color(s, lv_color_black(), LV_PART_KNOB);
  lv_obj_set_style_shadow_width(s, 8, LV_PART_KNOB);
  lv_obj_set_style_shadow_ofs_y(s, 2, LV_PART_KNOB);
  lv_obj_set_style_shadow_opa(s, LV_OPA_50, LV_PART_KNOB);
}

static inline void ui_style_switch(lv_obj_t *sw) {
  lv_obj_set_style_bg_color(sw, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(sw, lv_color_hex(CLR_HEX_HAIRLINE), LV_PART_MAIN);
  lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);

  lv_obj_set_style_bg_color(sw, lv_color_hex(CLR_HEX_ACCENT),
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);

  lv_obj_set_style_bg_color(sw, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_shadow_color(sw, lv_color_black(), LV_PART_KNOB);
  lv_obj_set_style_shadow_width(sw, 6, LV_PART_KNOB);
  lv_obj_set_style_shadow_opa(sw, LV_OPA_40, LV_PART_KNOB);
}

// The dropdown list is created lazily, so it can only be styled once the
// dropdown reports it has opened (LV_EVENT_READY).
static inline void _ui_dd_list_style_cb(lv_event_t *e) {
  lv_obj_t *list = lv_dropdown_get_list(lv_event_get_target(e));
  if (!list) return;
  lv_obj_set_style_bg_color(list, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(list, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(list, 1, 0);
  lv_obj_set_style_radius(list, UI_PILL_RADIUS, 0);
  lv_obj_set_style_text_color(list, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_pad_all(list, 4, 0);
  lv_obj_set_style_shadow_color(list, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(list, 24, 0);
  lv_obj_set_style_shadow_ofs_y(list, 6, 0);
  lv_obj_set_style_shadow_opa(list, LV_OPA_60, 0);
  // Highlighted row
  lv_obj_set_style_bg_color(list, lv_color_hex(CLR_HEX_ACCENT), LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_set_style_text_color(list, lv_color_hex(CLR_HEX_ON_ACCENT), LV_PART_SELECTED);
  lv_obj_set_style_radius(list, 8, LV_PART_SELECTED);
  ui_style_scrollbar(list);
}

static inline void ui_style_dropdown(lv_obj_t *dd) {
  lv_obj_set_style_bg_color(dd, lv_color_hex(CLR_HEX_SURFACE_2), 0);
  lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(dd, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(dd, 1, 0);
  lv_obj_set_style_radius(dd, UI_PILL_RADIUS, 0);
  lv_obj_set_style_text_color(dd, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_set_style_shadow_width(dd, 0, 0);
  lv_obj_set_style_border_color(dd, lv_color_hex(CLR_HEX_ACCENT), LV_STATE_PRESSED);
  lv_obj_add_event_cb(dd, _ui_dd_list_style_cb, LV_EVENT_READY, NULL);
}

static inline void ui_style_keyboard(lv_obj_t *kb) {
  lv_obj_set_style_bg_color(kb, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(kb, 0, 0);
  lv_obj_set_style_pad_all(kb, 4, 0);
  lv_obj_set_style_pad_gap(kb, 4, 0);

  lv_obj_set_style_bg_color(kb, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, lv_color_hex(CLR_HEX_TEXT_HI), LV_PART_ITEMS);
  lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, 8, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, lv_color_hex(CLR_HEX_ACCENT),
                            LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(kb, lv_color_hex(CLR_HEX_ON_ACCENT),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
}

// ── Message Box ─────────────────────────────────────────
// lv_msgbox_create() renders with the stock LVGL theme, which is a light grey
// card — jarring on this dark UI. Call this on every msgbox right after
// creating it.
static inline void ui_style_msgbox(lv_obj_t *mbox) {
  lv_obj_set_style_bg_color(mbox, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_opa(mbox, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(mbox, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(mbox, 1, 0);
  lv_obj_set_style_radius(mbox, UI_CARD_RADIUS, 0);
  lv_obj_set_style_shadow_color(mbox, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(mbox, 30, 0);
  lv_obj_set_style_shadow_ofs_y(mbox, 8, 0);
  lv_obj_set_style_shadow_opa(mbox, LV_OPA_60, 0);
  lv_obj_set_style_text_color(mbox, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_set_style_pad_all(mbox, 18, 0);

  lv_obj_t *btns = lv_msgbox_get_btns(mbox);
  if (!btns) return;
  lv_obj_set_height(btns, 38);
  lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btns, 0, 0);
  lv_obj_set_style_pad_column(btns, 8, 0);
  lv_obj_set_style_bg_color(btns, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(btns, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(btns, lv_color_hex(CLR_HEX_TEXT_HI), LV_PART_ITEMS);
  lv_obj_set_style_radius(btns, UI_PILL_RADIUS, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(btns, lv_color_hex(CLR_HEX_ACCENT),
                            LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(btns, lv_color_hex(CLR_HEX_ON_ACCENT),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
}

// ── Chip ────────────────────────────────────────────────
// Compact raised pill for at-a-glance counters (e.g. "4/9 ON").
// The caller owns the returned chip; children are added by the caller.
static inline lv_obj_t *ui_create_chip(lv_obj_t *parent, int w, int h) {
  lv_obj_t *chip = lv_obj_create(parent);
  lv_obj_set_size(chip, w, h);
  lv_obj_set_style_bg_color(chip, lv_color_hex(CLR_HEX_SURFACE_2), 0);
  lv_obj_set_style_bg_opa(chip, LV_OPA_70, 0);
  lv_obj_set_style_border_color(chip, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(chip, 1, 0);
  lv_obj_set_style_border_opa(chip, LV_OPA_80, 0);
  lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(chip, 0, 0);
  lv_obj_set_style_pad_all(chip, 0, 0);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return chip;
}
