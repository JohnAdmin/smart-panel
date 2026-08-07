#include "../../include/scene.h"
#include "../../include/globals.h"
#include "../hal.h"
#include "../lang.h"
#include "../mqtt_manager.h"
#include "../wifi_manager.h"
#include "ui_dimmer_modal.h"
#include "ui_helpers.h"
#include "ui_screens.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <string.h>

// Globals for Dashboard
lv_obj_t *home_time_label = NULL;

// Strip invisible/control bytes AND leading punctuation/symbols from `s`
// in place. Removes UTF-8 BOM, zero-width chars, direction marks, C0
// controls, whitespace, and common ASCII/UTF-8 leading punctuation that
// stray from sloppy data input (period, middle dot, bullet, etc.) so the
// first visible char is always a letter or digit.
static void sanitize_visible_text(char *s) {
  if (!s || !*s) return;
  size_t skip = 0;
  bool stripped_any = false;
  while (s[skip]) {
    uint8_t b0 = (uint8_t)s[skip];
    uint8_t b1 = (uint8_t)s[skip + 1];
    uint8_t b2 = (uint8_t)s[skip + 2];
    // 3-byte UTF-8 invisible / punctuation
    if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) { skip += 3; stripped_any = true; continue; } // BOM
    if (b0 == 0xE2 && b1 == 0x80 && b2 >= 0x8B && b2 <= 0x8F) { skip += 3; stripped_any = true; continue; } // ZWSP/ZWNJ/ZWJ/LRM/RLM
    if (b0 == 0xE2 && b1 == 0x81 && b2 == 0xA0) { skip += 3; stripped_any = true; continue; } // word joiner
    if (b0 == 0xE2 && b1 == 0x80 && (b2 == 0xA8 || b2 == 0xA9)) { skip += 3; stripped_any = true; continue; } // line/para sep
    if (b0 == 0xE2 && b1 == 0x80 && b2 == 0xA2) { skip += 3; stripped_any = true; continue; } // bullet •
    // Thai combining marks U+0E30-U+0E3A (vowels/PINTHU) and U+0E47-U+0E4E
    // (tone marks). These should never appear at the start of a word and
    // render as stray dots/strokes on top of nothing. Encoded as E0 B8 Bx.
    if (b0 == 0xE0 && b1 == 0xB8 && b2 >= 0xB0 && b2 <= 0xBA) { skip += 3; stripped_any = true; continue; } // U+0E30-U+0E3A
    if (b0 == 0xE0 && b1 == 0xB9 && b2 >= 0x87 && b2 <= 0x8E) { skip += 3; stripped_any = true; continue; } // U+0E47-U+0E4E
    if (b0 == 0xC2 && b1 == 0xB7) { skip += 2; stripped_any = true; continue; } // middle dot ·
    // ASCII control + whitespace + leading punctuation
    if (b0 < 0x20 || b0 == 0x7F) { skip += 1; stripped_any = true; continue; }
    if (b0 == ' ' || b0 == '.' || b0 == ',' || b0 == ':' || b0 == ';' ||
        b0 == '-' || b0 == '_' || b0 == '*' || b0 == '~' || b0 == '`' ||
        b0 == '!' || b0 == '?') { skip += 1; stripped_any = true; continue; }
    break;
  }
  if (skip > 0) {
    if (stripped_any) {
      Serial.printf("[SANITIZE] Stripped %u leading byte(s) from \"%s\"\n",
                    (unsigned)skip, s);
    }
    size_t len = strlen(s + skip);
    memmove(s, s + skip, len + 1);
  }
}


// ========================================================
//  DASHBOARD VISIBILITY
// ========================================================
bool ui_device_is_visible(int idx) {
  if (idx < 0 || idx >= deviceCount || idx >= MAX_DEVICES) return false;
  return !devices[idx].isPanelStatus();
}

void ui_count_visible_devices(int *on_count, int *total) {
  int on = 0, n = 0;
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    if (!ui_device_is_visible(i)) continue;
    n++;
    if (devices[i].status) on++;
  }
  if (on_count) *on_count = on;
  if (total) *total = n;
}

// ========================================================
//  TILE STATE SWEEP
// ========================================================
// Two tile states change with the passage of time rather than with an incoming
// message, so nothing would ever repaint them:
//   • stale   — the state topic has said nothing for STALE_DEVICE_MS
//   • pending — a command went out and no confirmation came back
// This timer compares both against what each tile is currently showing and
// re-renders only the ones that actually changed.
static bool s_tile_stale[MAX_DEVICES] = {false};
static bool s_tile_pending[MAX_DEVICES] = {false};
static lv_timer_t *s_tile_sweep_timer = NULL;

static bool device_is_stale(int i) {
  return devices[i].state_topic[0] != '\0' && devices[i].lastSeenTime > 0 &&
         (millis() - devices[i].lastSeenTime) > STALE_DEVICE_MS;
}

static void tile_sweep_cb(lv_timer_t *t) {
  (void)t;
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    if (device_tiles[i] == NULL) continue;
    if (device_is_stale(i) != s_tile_stale[i] ||
        devices[i].isPending() != s_tile_pending[i])
      ui_update_device_status(i, devices[i].status);
  }
}

// The room tabview is gone — rooms are cards on Home now, and which view the
// user is on is remembered by s_view instead of by tab name.

// ========================================================
//  FAN PULSE ANIMATION HELPER (breathing glow when ON)
// ========================================================
static void fan_pulse_anim_cb(void *obj, int32_t val) {
  lv_obj_set_style_shadow_width((lv_obj_t *)obj, val, 0);
  // val 10..30 → opacity 70..190. Keep the result inside lv_opa_t's 0–255
  // range: the old 150 + val*4 mapping overflowed past val=26 and made the
  // glow snap back to almost transparent mid-breath.
  lv_obj_set_style_shadow_opa((lv_obj_t *)obj, (lv_opa_t)(70 + (val - 10) * 6), 0);
}

static void fan_spin_start(lv_obj_t *icon_cont) {
  if (lv_anim_get(icon_cont, fan_pulse_anim_cb)) return;
  lv_obj_set_style_shadow_color(icon_cont, lv_color_hex(CLR_HEX_ACCENT), 0);
  lv_obj_set_style_shadow_spread(icon_cont, 4, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, icon_cont);
  lv_anim_set_values(&a, 10, 30);             // shadow 10px → 30px
  lv_anim_set_time(&a, 800);                 // expand 0.8s
  lv_anim_set_playback_time(&a, 800);        // shrink 0.8s
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, fan_pulse_anim_cb);
  lv_anim_start(&a);
}

static void fan_spin_stop(lv_obj_t *icon_cont) {
  lv_anim_del(icon_cont, fan_pulse_anim_cb);
  lv_obj_set_style_shadow_width(icon_cont, 0, 0);
  lv_obj_set_style_shadow_opa(icon_cont, LV_OPA_TRANSP, 0);
}

// ========================================================
//  EVENT CALLBACKS
// ========================================================

void btn_toggle_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);

  if (idx < 0 || idx >= deviceCount)
    return;

  if (code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED ||
      code == LV_EVENT_CLICKED || code == LV_EVENT_SHORT_CLICKED ||
      code == LV_EVENT_LONG_PRESSED) {
    hal_note_tile_event();
  }

  if (code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED ||
      code == LV_EVENT_CLICKED || code == LV_EVENT_SHORT_CLICKED ||
      code == LV_EVENT_LONG_PRESSED) {
    const char *event_name = "OTHER";
    if (code == LV_EVENT_PRESSED) {
      event_name = "PRESSED";
    } else if (code == LV_EVENT_RELEASED) {
      event_name = "RELEASED";
    } else if (code == LV_EVENT_CLICKED) {
      event_name = "CLICKED";
    } else if (code == LV_EVENT_SHORT_CLICKED) {
      event_name = "SHORT_CLICKED";
    } else if (code == LV_EVENT_LONG_PRESSED) {
      event_name = "LONG_PRESSED";
    }
    Serial.printf("[TILE] idx=%d event=%s\n", idx, event_name);
  }

  if (code == LV_EVENT_CLICKED) {
    // A fan's power and its speed are the same dial: switching one on at speed
    // 0 would leave a tile that claims to be on while the fan sits still. Tap
    // moves it between "off" and the lowest speed instead, and the speed row
    // handles everything above that.
    if (devices[idx].dev_type == DEV_FAN)
      set_device_level(idx, devices[idx].status ? 0 : 1);
    else
      toggle_device(idx);
  } else if (code == LV_EVENT_LONG_PRESSED) {
    // Only dimmers get the brightness modal. Fans and ACs also use the level
    // channel, but their tiles carry their own control and the modal's 0-100
    // slider would be meaningless for a speed or a setpoint.
    if (devices[idx].dev_type == DEV_DIMMER &&
        devices[idx].dimmer_topic[0] != '\0') {
      build_dimmer_modal(idx);
    }
  }
}

void scene_btn_event_cb(lv_event_t *e) {
  int scene_id = (int)(ptrdiff_t)lv_event_get_user_data(e);
  if (scene_id == 0) {
    // Leave Mode
    mqtt_publish_string("sc01/scene/leave", "1");
  } else if (scene_id == 1) {
    // Home Mode
    mqtt_publish_string("sc01/scene/home", "1");
  }
}

// "All OFF" — a one-tap, un-undoable action, so it asks first (device deletion
// already does; switching a whole room off did not).
//
// An empty room name means the whole house: the Home grid's All-off card
// passes "" so it can reuse this path rather than carrying a second copy of
// the confirm-then-sweep logic.
static char s_pending_off_room[48] = "";
static bool s_pending_off_armed = false;

static bool device_in_off_scope(int i, const char *room) {
  if (!ui_device_is_visible(i)) return false; // never switch the panel itself off
  return room[0] == '\0' || strcmp(devices[i].room, room) == 0;
}

static void all_off_msgbox_cb(lv_event_t *e) {
  lv_obj_t *mbox = lv_event_get_current_target(e);
  if (lv_msgbox_get_active_btn(mbox) == 0 && s_pending_off_armed) { // Yes
    for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
      if (device_in_off_scope(i, s_pending_off_room) && devices[i].status)
        toggle_device(i);
    }
  }
  s_pending_off_room[0] = '\0';
  s_pending_off_armed = false;
  lv_msgbox_close(mbox);
}

static void room_all_off_cb(lv_event_t *e) {
  const char *room = (const char *)lv_event_get_user_data(e);
  if (!room) return;

  int on_now = 0;
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++)
    if (device_in_off_scope(i, room) && devices[i].status) on_now++;

  if (on_now == 0) { // nothing to do — don't ask a pointless question
    ui_show_toast(L(L_NOTHING_ON));
    return;
  }

  strncpy(s_pending_off_room, room, sizeof(s_pending_off_room) - 1);
  s_pending_off_room[sizeof(s_pending_off_room) - 1] = '\0';
  s_pending_off_armed = true;

  // The button map must outlive this call — lv_msgbox keeps the pointer.
  static const char *btns[3];
  btns[0] = L(L_YES);
  btns[1] = L(L_NO);
  btns[2] = "";

  char msg[96];
  snprintf(msg, sizeof(msg), L(L_CONFIRM_ALL_OFF_MSG),
           room[0] ? room : panelTitle);
  lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), L(L_CONFIRM_ALL_OFF), msg,
                                    btns, false);
  ui_style_msgbox(mbox);
  lv_obj_center(mbox);
  lv_obj_add_event_cb(mbox, all_off_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// ========================================================
//  DEVICE TILE
// ========================================================
// Two columns at ~197 px, laid out the way the design specifies: icon and
// status on the top line, name under them, the type's control along the
// bottom. The old three-column 126 px tile had no room for a control at all —
// a four-button fan row would have given 24 px targets.

// Per-tile control widget, by type: the fan's button matrix or the AC's
// setpoint label. NULL for toggles and dimmers (a dimmer's bar lives in
// device_level_bars). ui_update_device_status() repaints through this.
static lv_obj_t *device_ctrl[MAX_DEVICES] = {NULL};

// Shared across every fan tile — lv_btnmatrix stores the pointer rather than
// copying, and the labels are identical on all of them. Refilled on each
// rebuild so a language change lands.
static const char *s_fan_map[5] = {"Off", "1", "2", "3", ""};

static void fan_speed_cb(lv_event_t *e) {
  int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);
  set_device_level(idx, lv_btnmatrix_get_selected_btn(lv_event_get_target(e)));
}

static void ac_step_cb(lv_event_t *e) {
  // user_data packs the device index and the direction: idx*2 + (up ? 1 : 0)
  const int packed = (int)(ptrdiff_t)lv_event_get_user_data(e);
  const int idx = packed >> 1;
  if (idx < 0 || idx >= deviceCount) return;
  set_device_level(idx, devices[idx].brightness + ((packed & 1) ? 1 : -1));
}

// Small square stepper button for the AC setpoint.
static void ac_step_btn(lv_obj_t *parent, const char *glyph, lv_align_t align,
                        int packed) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_size(b, 30, 22);
  lv_obj_align(b, align, 0, 0);
  lv_obj_set_style_radius(b, 6, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(CLR_HEX_ACCENT), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, (lv_opa_t)56, LV_STATE_PRESSED);
  lv_obj_add_event_cb(b, ac_step_cb, LV_EVENT_CLICKED, (void *)(ptrdiff_t)packed);

  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, glyph);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_center(l);
}

// Builds a fully-styled device tile inside `parent` for device at `idx`.
static lv_obj_t *create_device_tile(lv_obj_t *parent, int idx, int tile_w,
                                    int tile_h, lv_obj_t **out_icon_cont,
                                    lv_obj_t **out_icon,
                                    lv_obj_t **out_name_lbl,
                                    lv_obj_t **out_stat_lbl,
                                    lv_obj_t **out_level_bar) {
  const Device &dev = devices[idx];
  const bool is_dimmer = dev.dev_type == DEV_DIMMER;
  const int inner_w = tile_w - 16; // tile pad is 8 a side

  // --- Tile container ---
  lv_obj_t *tile = lv_obj_create(parent);
  lv_obj_set_size(tile, tile_w, tile_h);
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(tile, LV_OBJ_FLAG_PRESS_LOCK);

  // Resting surface: flat fill at 90 %, hairline outline, drop shadow. The
  // depth comes from the wallpaper reading through the fill, not from a
  // gradient — a synthetic ramp collapses to four hue steps here (see the
  // banding note in ui_helpers.h) whereas the wallpaper is continuous tone and
  // stays smooth. 90 % is the point where that still reads without letting a
  // bright image wash the labels out.
  lv_obj_set_style_bg_color(tile, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_dir(tile, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_90, 0);
  lv_obj_set_style_border_color(tile, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(tile, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tile, 1, 0);
  lv_obj_set_style_radius(tile, UI_TILE_RADIUS, 0);
  // Neutral drop shadow lifts the tile off the wallpaper without adding colour
  lv_obj_set_style_shadow_color(tile, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(tile, 14, 0);
  lv_obj_set_style_shadow_ofs_y(tile, 4, 0);
  lv_obj_set_style_shadow_opa(tile, LV_OPA_40, 0);
  lv_obj_set_style_pad_all(tile, 8, 0);
  // Touch feedback — outline lights up in accent while held
  lv_obj_set_style_border_color(tile, lv_color_hex(CLR_HEX_ACCENT), LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(tile, LV_OPA_80, LV_STATE_PRESSED);

  // --- 1. Icon badge, top-left ---
  lv_obj_t *ic_cont = lv_obj_create(tile);
  lv_obj_set_size(ic_cont, 30, 30);
  lv_obj_align(ic_cont, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_radius(ic_cont, LV_RADIUS_CIRCLE, 0);
  // Resting badge: raised neutral disc, not a coloured one — colour is the
  // signal for "this device is on".
  lv_obj_set_style_bg_color(ic_cont, lv_color_hex(CLR_HEX_SURFACE_2), 0);
  lv_obj_set_style_bg_opa(ic_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ic_cont, 0, 0);
  lv_obj_set_style_shadow_width(ic_cont, 0, 0);
  lv_obj_set_style_pad_all(ic_cont, 0, 0);
  lv_obj_clear_flag(ic_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ic_cont, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *ic = lv_label_create(ic_cont);
  lv_label_set_text(ic, getIconSymbol(dev.icon_type));
  lv_obj_set_style_text_font(ic, &material_icons_font, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_center(ic);
  lv_obj_add_flag(ic, LV_OBJ_FLAG_EVENT_BUBBLE);

  // --- 2. Status, top-right — set by ui_update_device_status() ---
  lv_obj_t *stat_lbl = lv_label_create(tile);
  lv_label_set_text(stat_lbl, L(L_OFF));
  lv_obj_set_style_text_font(stat_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(stat_lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_label_set_long_mode(stat_lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(stat_lbl, inner_w - 36);
  lv_obj_set_style_text_align(stat_lbl, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(stat_lbl, LV_ALIGN_TOP_RIGHT, 0, 4);
  lv_obj_add_flag(stat_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

  // --- 3. Device name, under the badge ---
  lv_obj_t *name_lbl = lv_label_create(tile);
  lv_label_set_text(name_lbl, dev.name);
  lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(name_lbl, inner_w);
  lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 0, 34);
  lv_obj_add_flag(name_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

  // --- 4. The type's control, along the bottom ---
  lv_obj_t *level_bar = NULL;
  device_ctrl[idx] = NULL;

  switch (dev.dev_type) {
  case DEV_DIMMER: {
    // The bar is what tells you the tile has a level at all — long-pressing to
    // open the brightness modal is otherwise an invisible gesture.
    level_bar = lv_bar_create(tile);
    lv_obj_set_size(level_bar, inner_w, 4);
    lv_obj_align(level_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(level_bar, 0, 100);
    lv_bar_set_value(level_bar, dev.brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(level_bar, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(level_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(level_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(level_bar, lv_color_hex(CLR_HEX_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_radius(level_bar, 2, LV_PART_INDICATOR);
    lv_obj_add_flag(level_bar, LV_OBJ_FLAG_EVENT_BUBBLE);
    break;
  }
  case DEV_FAN: {
    lv_obj_t *bm = ui_create_segmented(tile, s_fan_map, inner_w, 20,
                                       devices[idx].clampLevel(dev.brightness),
                                       fan_speed_cb, (void *)(ptrdiff_t)idx);
    lv_obj_align(bm, LV_ALIGN_BOTTOM_MID, 0, 0);
    device_ctrl[idx] = bm;
    break;
  }
  case DEV_AC: {
    lv_obj_t *bar = lv_obj_create(tile);
    lv_obj_set_size(bar, inner_w, 22);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    ac_step_btn(bar, LV_SYMBOL_MINUS, LV_ALIGN_LEFT_MID, idx * 2 + 0);
    ac_step_btn(bar, LV_SYMBOL_PLUS, LV_ALIGN_RIGHT_MID, idx * 2 + 1);

    // Setpoint in the cool blue — the one place this palette uses it, so the
    // number reads as a temperature rather than as device state.
    lv_obj_t *t = lv_label_create(bar);
    lv_label_set_text_fmt(t, "%d\xC2\xB0", devices[idx].clampLevel(dev.brightness));
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(CLR_HEX_COOL), 0);
    lv_obj_center(t);
    device_ctrl[idx] = t;
    break;
  }
  default:
    break; // DEV_TOGGLE — tapping the tile is the control
  }

  // --- Touch event ---
  lv_obj_add_event_cb(tile, btn_toggle_event_cb, LV_EVENT_ALL,
                      (void *)(ptrdiff_t)idx);

  // Populate output pointers
  *out_icon_cont = ic_cont;
  *out_icon = ic;
  *out_name_lbl = name_lbl;
  *out_stat_lbl = stat_lbl;
  if (out_level_bar) *out_level_bar = level_bar;
  (void)is_dimmer;

  return tile;
}

// ========================================================
//  MAIN-SCREEN VIEWS
// ========================================================
// ui_ScreenMain hosts several views inside main_body_container instead of
// giving each one its own lv screen. The nav rail and the header are built
// once on this screen and every view sits under them — which is what the
// design shows, and it means switching views costs one lv_obj_clean() rather
// than a screen load plus the cleanup bookkeeping every sub-screen needs.
//
// rebuild_grid() renders whichever view is current, so its existing callers
// (language change, device edit, settings save) keep working unchanged.
// Short local names for the ids ui_screens.h publishes.
#define VIEW_HOME      UI_VIEW_HOME
#define VIEW_FAVORITES UI_VIEW_FAVORITES
#define VIEW_SCENES    UI_VIEW_SCENES
#define VIEW_SCHEDULE  UI_VIEW_SCHEDULE
#define VIEW_SENSORS   UI_VIEW_SENSORS
static int s_view = VIEW_HOME;

int ui_current_main_view() { return s_view; }

void ui_show_main_view(int view) {
  s_view = view;
  rebuild_grid();
}

// Home cards carry only aggregate numbers, so nothing on them is redrawn by
// the per-device tile update. These pointers let ui_update_device_status()
// keep the counts live when a device changes state over MQTT while the user
// is looking at Home.
#define UI_MAX_HOME_CARDS (MAX_ROOMS + 1)
static struct HomeCard {
  int view_id; // room index, or VIEW_FAVORITES
  lv_obj_t *icon;
  lv_obj_t *dot;
  lv_obj_t *sub;
  lv_obj_t *sw; // list layout only
} s_home_cards[UI_MAX_HOME_CARDS];
static int s_home_card_count = 0;

// Devices counted for one card. Favourites are a pseudo-room: they behave the
// same on Home, they just gather their members by flag instead of by name.
static void home_card_counts(int view_id, int *on, int *total) {
  if (view_id == VIEW_FAVORITES) {
    int o = 0, n = 0;
    for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
      if (!ui_device_is_visible(i) || !devices[i].is_favorite) continue;
      n++;
      if (devices[i].status) o++;
    }
    if (on) *on = o;
    if (total) *total = n;
    return;
  }
  room_count_devices(view_id, on, total);
}

// True when `idx` belongs to the collection the given view shows.
static bool device_in_view(int idx, int view_id) {
  if (!ui_device_is_visible(idx)) return false;
  if (view_id == VIEW_FAVORITES) return devices[idx].is_favorite;
  if (view_id < 0 || view_id >= roomCount) return false;
  return strcmp(devices[idx].room, rooms[view_id].name) == 0;
}

static void home_card_set_state(HomeCard &c) {
  int on = 0, total = 0;
  home_card_counts(c.view_id, &on, &total);
  const lv_color_t live = lv_color_hex(on > 0 ? CLR_HEX_ACCENT : CLR_HEX_TEXT_LOW);

  if (c.icon) lv_obj_set_style_text_color(c.icon, live, 0);
  if (c.dot) lv_obj_set_style_bg_color(c.dot, live, 0);
  if (c.sub) {
    char buf[32];
    if (on > 0) snprintf(buf, sizeof(buf), L(L_ON_COUNT), on);
    else        snprintf(buf, sizeof(buf), "%s", L(L_NONE_ON));
    lv_label_set_text(c.sub, buf);
    lv_obj_set_style_text_color(
        c.sub, lv_color_hex(on > 0 ? CLR_HEX_ACCENT_HI : CLR_HEX_TEXT_LOW), 0);
  }
  if (c.sw) {
    if (on > 0) lv_obj_add_state(c.sw, LV_STATE_CHECKED);
    else        lv_obj_clear_state(c.sw, LV_STATE_CHECKED);
  }
}

static void refresh_home_cards() {
  for (int i = 0; i < s_home_card_count; i++)
    home_card_set_state(s_home_cards[i]);
}

// A new climate reading changes whether a room card has a third line at all,
// so unlike a device state change there is no widget to poke — the cards have
// to be rebuilt. Only worth doing while Home is actually on screen.
void ui_refresh_home_climate() {
  if (s_view == VIEW_HOME) rebuild_grid();
}

// ========================================================
//  HOME VIEW — rooms
// ========================================================

static void card_open_view_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  hal_note_tile_event();
  ui_show_main_view((int)(intptr_t)lv_event_get_user_data(e));
}

static void back_to_home_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  ui_show_main_view(VIEW_HOME);
}

// Whole-room switch on the list layout. No confirmation here on purpose: the
// design puts the switch on the row as a one-tap control, and the deliberate,
// un-undoable version of the same action — the All OFF pill inside the room —
// still asks. Nothing is rebuilt from inside the callback; the tiles and the
// row's own labels are repainted by ui_update_device_status() as each toggle
// lands.
static void room_switch_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  const int view_id = (int)(intptr_t)lv_event_get_user_data(e);
  const bool want_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    if (!device_in_view(i, view_id)) continue;
    if (devices[i].status != want_on) toggle_device(i);
  }
}

// Registers a card's live parts so refresh_home_cards() can find them again.
static void home_card_track(int view_id, lv_obj_t *icon, lv_obj_t *dot,
                            lv_obj_t *sub, lv_obj_t *sw) {
  if (s_home_card_count >= UI_MAX_HOME_CARDS) return;
  HomeCard &c = s_home_cards[s_home_card_count++];
  c.view_id = view_id;
  c.icon = icon;
  c.dot = dot;
  c.sub = sub;
  c.sw = sw;
  home_card_set_state(c);
}

// Grid layout: a 129×120 card per room. Icon and dot on the top line, name and
// live count anchored to the bottom.
static void create_room_card(lv_obj_t *parent, int view_id, const char *name,
                             int icon_type) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, UI_ROOM_CARD_W, UI_ROOM_CARD_H);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);

  // Same resting surface as a device tile: flat fill at 90 % so the wallpaper
  // supplies the depth a gradient cannot on this panel.
  lv_obj_set_style_bg_color(card, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
  lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(card, 14, 0);
  lv_obj_set_style_shadow_ofs_y(card, 4, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
  lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(CLR_HEX_ACCENT), LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(card, LV_OPA_80, LV_STATE_PRESSED);
  lv_obj_add_event_cb(card, card_open_view_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)view_id);

  lv_obj_t *icon = lv_label_create(card);
  lv_label_set_text(icon, getIconSymbol(icon_type));
  lv_obj_set_style_text_font(icon, &material_icons_font, 0);
  lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *dot = ui_create_dot(card, 7, lv_color_hex(CLR_HEX_TEXT_LOW));
  lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, 0, 5);

  const bool has_climate =
      (view_id >= 0 && view_id < roomCount && rooms[view_id].climateValid);

  lv_obj_t *nm = lv_label_create(card);
  lv_label_set_text(nm, name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
  lv_obj_set_width(nm, UI_ROOM_CARD_W - 20);
  lv_obj_align(nm, LV_ALIGN_BOTTOM_LEFT, 0, has_climate ? -36 : -18);
  lv_obj_add_flag(nm, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *sub = lv_label_create(card);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
  lv_obj_set_width(sub, UI_ROOM_CARD_W - 20);
  lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 0, has_climate ? -18 : 0);
  lv_obj_add_flag(sub, LV_OBJ_FLAG_EVENT_BUBBLE);

  if (has_climate) {
    lv_obj_t *clim = lv_label_create(card);
    lv_label_set_text_fmt(clim, "%.1f\xC2\xB0 \xC2\xB7 %d%%", rooms[view_id].temp,
                          rooms[view_id].hum);
    lv_obj_set_style_text_font(clim, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(clim, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_align(clim, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_flag(clim, LV_OBJ_FLAG_EVENT_BUBBLE);
  }

  home_card_track(view_id, icon, dot, sub, NULL);
}

// List layout: a 42 px row per room, carrying a whole-room switch.
static void create_room_row(lv_obj_t *parent, int view_id, const char *name,
                            int icon_type) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), UI_ROOM_ROW_H);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(row, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_dir(row, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_90, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(row, 10, 0);
  lv_obj_set_style_shadow_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(CLR_HEX_ACCENT), LV_STATE_PRESSED);

  lv_obj_t *icon = lv_label_create(row);
  lv_label_set_text(icon, getIconSymbol(icon_type));
  lv_obj_set_style_text_font(icon, &material_icons_font, 0);
  lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *nm = lv_label_create(row);
  lv_label_set_text(nm, name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
  lv_obj_set_width(nm, 210);
  lv_obj_align(nm, LV_ALIGN_LEFT_MID, 44, -8);
  lv_obj_add_flag(nm, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *sub = lv_label_create(row);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
  lv_obj_set_width(sub, 210);
  lv_obj_align(sub, LV_ALIGN_LEFT_MID, 44, 9);
  lv_obj_add_flag(sub, LV_OBJ_FLAG_EVENT_BUBBLE);

  // Whole-room switch. It sits before the chevron so the row still opens the
  // room when tapped anywhere else.
  lv_obj_t *sw = lv_switch_create(row);
  lv_obj_set_size(sw, 34, 19);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -32, 0);
  lv_obj_set_style_bg_color(sw, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(sw, lv_color_hex(CLR_HEX_HAIRLINE), LV_PART_MAIN);
  lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, lv_color_hex(CLR_HEX_ACCENT),
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, lv_color_white(), LV_PART_KNOB);
  lv_obj_add_event_cb(sw, room_switch_cb, LV_EVENT_VALUE_CHANGED,
                      (void *)(intptr_t)view_id);

  lv_obj_t *chev = lv_label_create(row);
  lv_label_set_text(chev, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(chev, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(chev, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_add_flag(chev, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_add_event_cb(row, card_open_view_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)view_id);
  home_card_track(view_id, icon, NULL, sub, sw);
}

// House-wide off. Sized and placed like a room card so the grid stays even.
// The design asks for a dashed outline here; LVGL 8 has no dashed border, so
// the card is set apart by a dimmer fill and a danger-tinted press state
// instead.
static void create_all_off_card(lv_obj_t *parent) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, UI_ROOM_CARD_W, UI_ROOM_CARD_H);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_style_bg_color(card, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_70, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_70, 0);
  lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(CLR_HEX_DANGER), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(card, LV_OPA_30, LV_STATE_PRESSED);
  lv_obj_set_style_border_color(card, lv_color_hex(CLR_HEX_DANGER), LV_STATE_PRESSED);
  // Empty room name = every room, which all_off_msgbox_cb reads as house-wide.
  lv_obj_add_event_cb(card, room_all_off_cb, LV_EVENT_CLICKED, (void *)"");

  lv_obj_t *icon = lv_label_create(card);
  lv_label_set_text(icon, LV_SYMBOL_POWER);
  lv_obj_set_style_text_font(icon, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(icon, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_center(icon);
  lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, L(L_ALL_OFF));
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(lbl, UI_ROOM_CARD_W - 20);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
}

static void build_home_view() {
  ui_set_header_title(panelTitle);

  const bool list_layout = (homeLayoutStyle == 1);

  lv_obj_t *c = lv_obj_create(main_body_container);
  lv_obj_set_size(c, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, UI_CONTENT_PAD, 0);
  lv_obj_set_flex_flow(c, list_layout ? LV_FLEX_FLOW_COLUMN
                                      : LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(c, list_layout ? 6 : 8, 0);
  lv_obj_set_style_pad_column(c, 8, 0);
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_AUTO);
  ui_style_scrollbar(c);

  int fav_count = 0;
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++)
    if (ui_device_is_visible(i) && devices[i].is_favorite) fav_count++;

  // Favourites lead: they are the devices the user singled out, so they should
  // not be one tap further away than the rooms they live in.
  if (fav_count > 0) {
    if (list_layout)
      create_room_row(c, VIEW_FAVORITES, L(L_FAVORITES), ICON_STRIP);
    else
      create_room_card(c, VIEW_FAVORITES, L(L_FAVORITES), ICON_STRIP);
  }

  int shown = 0;
  for (int r = 0; r < roomCount; r++) {
    int total = 0;
    room_count_devices(r, NULL, &total);
    if (total == 0) continue; // a room whose devices were all deleted
    char nm[48];
    strncpy(nm, rooms[r].name, sizeof(nm) - 1);
    nm[sizeof(nm) - 1] = '\0';
    sanitize_visible_text(nm);
    if (!nm[0]) continue;
    if (list_layout) create_room_row(c, r, nm, room_effective_icon(r));
    else             create_room_card(c, r, nm, room_effective_icon(r));
    shown++;
    safe_wdt_reset();
  }

  if (shown == 0 && fav_count == 0) {
    lv_obj_t *empty = lv_label_create(c);
    lv_label_set_text(empty, L(L_NO_ROOMS));
    lv_obj_set_style_text_color(empty, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(empty, LV_PCT(100));
    lv_obj_set_style_pad_top(empty, 60, 0);
    return;
  }

  if (!list_layout) create_all_off_card(c);
}

// ========================================================
//  ROOM / FAVOURITES VIEW — device tiles
// ========================================================
static void build_device_view(int view_id) {
  const bool favourites = (view_id == VIEW_FAVORITES);
  const char *title = favourites ? L(L_FAVORITES) : rooms[view_id].name;
  ui_set_header_title(title);

  // ── Sub-header: back, device count, room-wide off ──
  lv_obj_t *bar = lv_obj_create(main_body_container);
  lv_obj_set_size(bar, LV_PCT(100), 34);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *back = ui_create_pill_btn(bar, 30, 26, LV_SYMBOL_LEFT,
                                      lv_color_hex(CLR_HEX_TEXT_HI),
                                      back_to_home_cb, NULL, LV_EVENT_CLICKED);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, UI_CONTENT_PAD, 0);

  int on = 0, total = 0;
  home_card_counts(view_id, &on, &total);
  lv_obj_t *cnt = lv_label_create(bar);
  lv_label_set_text_fmt(cnt, "%d %s", total, L(L_DEVICES));
  lv_obj_set_style_text_font(cnt, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(cnt, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(cnt, LV_ALIGN_LEFT_MID, UI_CONTENT_PAD + 40, 0);

  // Outlined rather than filled — a destructive action should not be the
  // loudest thing in the room.
  if (!favourites) {
    lv_obj_t *off_btn = lv_btn_create(bar);
    lv_obj_set_size(off_btn, LV_SIZE_CONTENT, 26);
    lv_obj_align(off_btn, LV_ALIGN_RIGHT_MID, -UI_CONTENT_PAD, 0);
    lv_obj_set_style_bg_color(off_btn, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_bg_opa(off_btn, LV_OPA_20, 0);
    lv_obj_set_style_border_color(off_btn, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_border_width(off_btn, 1, 0);
    lv_obj_set_style_border_opa(off_btn, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(off_btn, 0, 0);
    lv_obj_set_style_radius(off_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(off_btn, 12, 0);
    lv_obj_set_style_pad_ver(off_btn, 2, 0);
    lv_obj_set_style_bg_opa(off_btn, LV_OPA_50, LV_STATE_PRESSED);
    lv_obj_add_event_cb(off_btn, room_all_off_cb, LV_EVENT_CLICKED,
                        (void *)rooms[view_id].name);
    lv_obj_t *off_lbl = lv_label_create(off_btn);
    lv_label_set_text_fmt(off_lbl, LV_SYMBOL_POWER "  %s", L(L_ALL_OFF));
    lv_obj_set_style_text_font(off_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(off_lbl, lv_color_hex(CLR_HEX_DANGER_HI), 0);
    lv_obj_center(off_lbl);
  }

  // ── Tile grid ──
  lv_obj_t *grid = lv_obj_create(main_body_container);
  lv_obj_set_size(grid, LV_PCT(100), UI_CONTENT_H - 34);
  lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_left(grid, UI_CONTENT_PAD, 0);
  lv_obj_set_style_pad_right(grid, UI_CONTENT_PAD, 0);
  lv_obj_set_style_pad_bottom(grid, 8, 0);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(grid, 10, 0);
  lv_obj_set_style_pad_column(grid, 10, 0);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);
  ui_style_scrollbar(grid);

  // "Large tiles" now means one full-width column rather than a wider tile —
  // two columns is already the design's layout, so the old 2-vs-3 distinction
  // has nowhere left to go.
  const int tile_w = useLargeTiles ? (UI_CONTENT_W - UI_CONTENT_PAD * 2)
                                   : UI_DEV_TILE_W;

  // Fan tiles share one label map; refill it here so a language change lands.
  s_fan_map[0] = L(L_OFF);

  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    if (!device_in_view(i, view_id)) continue;
    device_tiles[i] = create_device_tile(
        grid, i, tile_w, UI_DEV_TILE_H, &device_icon_containers[i], &device_icons[i],
        &device_labels[i], &device_status_labels[i], &device_level_bars[i]);
    ui_update_device_status(i, devices[i].status);
    safe_wdt_reset();
  }
}

// ========================================================
//  SCENES + SCHEDULE — one destination, two tabs
// ========================================================
// Scenes and schedules answer the same question — what runs, and when — so
// they share a rail slot and switch with a pill tab row instead of costing two.
// The scene *editor* still lives behind Settings; this is the run surface.

static void tab_switch_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  ui_show_main_view((int)(intptr_t)lv_event_get_user_data(e));
}

// One pill in the tab row. Selected is an accent tint with an accent outline —
// the same restraint as the nav rail, so a chosen tab never reads as a lit
// device the way a solid accent fill would.
static void tab_pill(lv_obj_t *row, const char *text, int x, bool active,
                     int view_id) {
  lv_obj_t *btn = lv_btn_create(row);
  lv_obj_set_size(btn, 92, 22);
  lv_obj_align(btn, LV_ALIGN_LEFT_MID, x, 0);
  lv_obj_set_style_radius(btn, 7, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_HEX_ACCENT), 0);
  lv_obj_set_style_bg_opa(btn, active ? (lv_opa_t)31 : LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(btn, (lv_opa_t)56, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(active ? CLR_HEX_ACCENT
                                                         : CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(btn, active ? (lv_opa_t)84 : LV_OPA_COVER, 0);
  lv_obj_add_event_cb(btn, tab_switch_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)view_id);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(
      lbl, lv_color_hex(active ? CLR_HEX_ACCENT : CLR_HEX_TEXT_LOW), 0);
  lv_obj_center(lbl);
}

// Builds the 30 px tab row and returns the container the tab body goes in.
static lv_obj_t *build_tabbed_shell(int active_view) {
  ui_set_header_title(active_view == UI_VIEW_SCENES ? L(L_SCENES)
                                                    : L(L_SCHEDULES));

  lv_obj_t *row = lv_obj_create(main_body_container);
  lv_obj_set_size(row, LV_PCT(100), 30);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_left(row, UI_CONTENT_PAD, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  tab_pill(row, L(L_SCENES), 0, active_view == UI_VIEW_SCENES, UI_VIEW_SCENES);
  tab_pill(row, L(L_SCHEDULES), 98, active_view == UI_VIEW_SCHEDULE,
           UI_VIEW_SCHEDULE);

  lv_obj_t *body = lv_obj_create(main_body_container);
  lv_obj_set_size(body, LV_PCT(100), UI_CONTENT_H - 30);
  lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body, 0, 0);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  return body;
}

static void build_scenes_view() {
  lv_obj_t *shell = build_tabbed_shell(UI_VIEW_SCENES);

  lv_obj_t *c = lv_obj_create(shell);
  lv_obj_set_size(c, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, 0, 0);
  lv_obj_set_style_pad_top(c, 4, 0);
  lv_obj_set_style_pad_bottom(c, 8, 0);
  lv_obj_set_style_pad_left(c, UI_CONTENT_PAD, 0);
  lv_obj_set_style_pad_right(c, UI_CONTENT_PAD, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(c, 10, 0);
  lv_obj_set_style_pad_column(c, 10, 0);
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_AUTO);
  ui_style_scrollbar(c);

  if (sceneCount == 0) {
    lv_obj_t *empty = lv_label_create(c);
    lv_label_set_text(empty, L(L_NO_SCENES));
    lv_obj_set_style_text_color(empty, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(empty, LV_PCT(100));
    lv_obj_set_style_pad_top(empty, 60, 0);
    return;
  }
  create_scene_tiles(c);
}

// ========================================================
//  SENSORS VIEW
// ========================================================
// Three stat cards over a per-room table. The design's third card is air
// quality; nothing in this firmware measures it and inventing a number would
// be worse than omitting one, so the slot shows the outdoor weather instead —
// which is real, already fetched, and the obvious companion to indoor
// readings now that the weather hero card has left Home.

// One stat card. Width is passed in because the row sizes itself to however
// many cards actually have data — a card with nothing to say is not drawn at
// all, so an empty box never appears.
static void sensor_stat_card(lv_obj_t *parent, int w, const char *label,
                             const char *value, lv_color_t value_color,
                             const char *sub) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, w, 64);
  lv_obj_set_style_bg_color(card, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(lbl, w - 20);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *v = lv_label_create(card);
  lv_label_set_text(v, value);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(v, value_color, 0);
  lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
  lv_obj_set_width(v, w - 20);
  lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  // The AQI band name sits under its number; the other cards pass NULL.
  if (sub) {
    lv_obj_t *s = lv_label_create(card);
    lv_label_set_text(s, sub);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s, value_color, 0);
    lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s, w - 20);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  }
}

// US AQI band. Colour runs OK → accent → danger; there is no separate scale
// for the worst two bands because at that point the number is the message.
static const char *aqi_band(int aqi, lv_color_t *color) {
  if (aqi <= 50)  { *color = lv_color_hex(CLR_HEX_OK);        return L(L_AQI_GOOD); }
  if (aqi <= 100) { *color = lv_color_hex(CLR_HEX_ACCENT_HI); return L(L_AQI_MODERATE); }
  if (aqi <= 150) { *color = lv_color_hex(CLR_HEX_ACCENT);    return L(L_AQI_SENSITIVE); }
  if (aqi <= 200) { *color = lv_color_hex(CLR_HEX_DANGER_HI); return L(L_AQI_UNHEALTHY); }
  *color = lv_color_hex(CLR_HEX_DANGER);
  return L(L_AQI_HAZARDOUS);
}

// One room's line in the table: name, how many of its devices are on, and its
// two readings. Rooms with no climate topic still appear — the dashes say
// "not measured here", which is more useful than hiding the room.
static void sensor_room_row(lv_obj_t *parent, int room_idx, bool last) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), 31);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(row, 0, 0);
  lv_obj_set_style_shadow_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  // A hairline under every line but the last reads as a table without boxing
  // each cell in.
  lv_obj_set_style_border_width(row, last ? 0 : 1, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(row, LV_OPA_50, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  const Room &rm = rooms[room_idx];
  int on = 0, total = 0;
  room_count_devices(room_idx, &on, &total);

  char nm[48];
  strncpy(nm, rm.name, sizeof(nm) - 1);
  nm[sizeof(nm) - 1] = '\0';
  sanitize_visible_text(nm);

  lv_obj_t *name = lv_label_create(row);
  lv_label_set_text(name, nm);
  lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(name, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
  lv_obj_set_width(name, 176);
  lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *on_lbl = lv_label_create(row);
  if (on > 0) {
    char b[24];
    snprintf(b, sizeof(b), L(L_ON_COUNT), on);
    lv_label_set_text(on_lbl, b);
    lv_obj_set_style_text_color(on_lbl, lv_color_hex(CLR_HEX_ACCENT_HI), 0);
  } else {
    lv_label_set_text(on_lbl, "\xE2\x80\x94"); // em dash
    lv_obj_set_style_text_color(on_lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  }
  lv_obj_set_style_text_font(on_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_width(on_lbl, 70);
  lv_obj_set_style_text_align(on_lbl, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(on_lbl, LV_ALIGN_LEFT_MID, 180, 0);

  lv_obj_t *t = lv_label_create(row);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
  lv_obj_set_width(t, 62);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 256, 0);

  lv_obj_t *h = lv_label_create(row);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_12, 0);
  lv_obj_set_width(h, 56);
  lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(h, LV_ALIGN_LEFT_MID, 322, 0);

  if (rm.climateValid) {
    lv_label_set_text_fmt(t, "%.1f\xC2\xB0", rm.temp);
    lv_obj_set_style_text_color(t, lv_color_hex(CLR_HEX_TEXT_HI), 0);
    lv_label_set_text_fmt(h, "%d%%", rm.hum);
    lv_obj_set_style_text_color(h, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  } else {
    lv_label_set_text(t, "\xE2\x80\x94");
    lv_obj_set_style_text_color(t, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_label_set_text(h, "\xE2\x80\x94");
    lv_obj_set_style_text_color(h, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  }
}

static void build_sensors_view() {
  ui_set_header_title(L(L_SENSORS));

  // Averages come only from rooms that actually report, so one configured
  // sensor doesn't get diluted by every room that has none.
  float t_sum = 0.0f;
  int h_sum = 0, n = 0;
  for (int r = 0; r < roomCount; r++) {
    if (!rooms[r].climateValid) continue;
    t_sum += rooms[r].temp;
    h_sum += rooms[r].hum;
    n++;
  }

  lv_obj_t *page = lv_obj_create(main_body_container);
  lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_set_style_pad_all(page, UI_CONTENT_PAD, 0);
  lv_obj_set_style_pad_row(page, 8, 0);
  lv_obj_set_style_pad_column(page, 8, 0);
  lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_scroll_dir(page, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_AUTO);
  ui_style_scrollbar(page);

  // Only cards with a real reading are built, and they share the row evenly —
  // an empty box says nothing and still costs a third of the width.
  const bool has_indoor = (n > 0);
  int n_cards = (has_indoor ? 2 : 0) + (weatherValid ? 1 : 0) +
                (airQualityValid ? 1 : 0);
  const int avail = UI_CONTENT_W - UI_CONTENT_PAD * 2;
  const int card_w = n_cards ? (avail - (n_cards - 1) * 8) / n_cards : 0;

  char buf[32];
  if (has_indoor) {
    snprintf(buf, sizeof(buf), "%.1f°", t_sum / n);
    sensor_stat_card(page, card_w, L(L_AVG_TEMP), buf,
                     lv_color_hex(CLR_HEX_TEXT_HI), NULL);
    snprintf(buf, sizeof(buf), "%d%%", h_sum / n);
    sensor_stat_card(page, card_w, L(L_AVG_HUM), buf,
                     lv_color_hex(CLR_HEX_TEXT_HI), NULL);
  }
  if (weatherValid) {
    snprintf(buf, sizeof(buf), "%.0f°", weatherTemp);
    sensor_stat_card(page, card_w, L(L_OUTDOOR), buf,
                     lv_color_hex(CLR_HEX_OK), NULL);
  }
  if (airQualityValid) {
    lv_color_t aqi_color;
    const char *band = aqi_band(airQualityAqi, &aqi_color);
    snprintf(buf, sizeof(buf), "%d", airQualityAqi);
    sensor_stat_card(page, card_w, L(L_AIR_QUALITY), buf, aqi_color, band);
  }


  // ── Per-room table ──
  lv_obj_t *table = lv_obj_create(page);
  lv_obj_set_size(table, LV_PCT(100),
                  UI_CONTENT_H - UI_CONTENT_PAD * 2 - (n_cards ? 72 : 0));
  lv_obj_set_style_bg_color(table, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_dir(table, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(table, LV_OPA_90, 0);
  lv_obj_set_style_border_color(table, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(table, 1, 0);
  lv_obj_set_style_border_opa(table, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(table, UI_CARD_RADIUS, 0);
  lv_obj_set_style_shadow_width(table, 0, 0);
  lv_obj_set_style_pad_all(table, 10, 0);
  lv_obj_set_style_pad_row(table, 0, 0);
  lv_obj_set_flex_flow(table, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(table, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(table, LV_SCROLLBAR_MODE_AUTO);
  ui_style_scrollbar(table);

  // Count the rooms that will actually get a line, so the last one can drop
  // its divider.
  int shown = 0, last_idx = -1;
  for (int r = 0; r < roomCount; r++) {
    int total = 0;
    room_count_devices(r, NULL, &total);
    if (total == 0 && !rooms[r].climateValid) continue;
    shown++;
    last_idx = r;
  }

  if (shown == 0) {
    lv_obj_t *empty = lv_label_create(table);
    lv_label_set_text(empty, L(L_NO_ROOMS));
    lv_obj_set_style_text_color(empty, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(empty, LV_PCT(100));
    lv_obj_set_style_pad_top(empty, 40, 0);
    return;
  }

  for (int r = 0; r < roomCount; r++) {
    int total = 0;
    room_count_devices(r, NULL, &total);
    if (total == 0 && !rooms[r].climateValid) continue;
    sensor_room_row(table, r, r == last_idx);
  }

  // Nothing reports yet — say what to do about it rather than leaving a table
  // of dashes with no explanation.
  if (n == 0) {
    lv_obj_t *hint = lv_label_create(table);
    lv_label_set_text(hint, L(L_NO_SENSORS));
    lv_obj_set_style_text_color(hint, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_set_style_pad_top(hint, 14, 0);
  }
}

// ========================================================
//  REBUILD GRID
// ========================================================
void rebuild_grid() {
  // Clear pointers to avoid stale references
  for (int i = 0; i < MAX_DEVICES; i++) {
    device_tiles[i] = NULL;
    device_icon_containers[i] = NULL;
    device_icons[i] = NULL;
    device_labels[i] = NULL;
    device_status_labels[i] = NULL;
    device_level_bars[i] = NULL;
    device_ctrl[i] = NULL;
    s_tile_stale[i] = false;
    s_tile_pending[i] = false;
  }
  s_home_card_count = 0;

  lv_obj_clean(main_body_container);

  // One sweep timer for the lifetime of the UI; it skips devices with no tile.
  if (!s_tile_sweep_timer)
    s_tile_sweep_timer = lv_timer_create(tile_sweep_cb, 1000, NULL);

  // Devices can gain or lose a room name at any time — from the portal, the
  // device manager, or a fresh devices.json — so the room list is reconciled
  // on every rebuild rather than only at boot.
  if (room_sync_from_devices()) saveRooms();

  // Scenes and schedules stand on their own — a panel with no devices can still
  // have both, so only the device-backed views fall back to Home here.
  if (deviceCount == 0 && s_view != VIEW_SCENES && s_view != VIEW_SCHEDULE &&
      s_view != VIEW_SENSORS) {
    s_view = VIEW_HOME;
    ui_set_header_title(panelTitle);
    lv_obj_t *empty_lbl = lv_label_create(main_body_container);
    lv_label_set_text(empty_lbl, L(L_NO_DEVICES));
    lv_obj_set_style_text_color(empty_lbl, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_font(empty_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(empty_lbl);
    return;
  }

  // The room being viewed can vanish under us when its last device is deleted.
  if (s_view >= roomCount) s_view = VIEW_HOME;
  if (s_view == VIEW_FAVORITES) {
    int fav = 0;
    home_card_counts(VIEW_FAVORITES, NULL, &fav);
    if (fav == 0) s_view = VIEW_HOME;
  }

  switch (s_view) {
  case VIEW_HOME:     build_home_view(); break;
  case VIEW_SCENES:   build_scenes_view(); break;
  case VIEW_SCHEDULE: build_schedule_view(build_tabbed_shell(VIEW_SCHEDULE)); break;
  case VIEW_SENSORS:  build_sensors_view(); break;
  default:            build_device_view(s_view); break;
  }
}

// ========================================================
//  DEVICE STATUS UPDATE
// ========================================================
void ui_update_device_status(int index, bool state) {
  // Home shows aggregate counts, not tiles, so its cards have to be refreshed
  // before the per-tile guard below returns — on that view every device_tiles[]
  // entry is NULL.
  refresh_home_cards();

  if (index < 0 || index >= deviceCount || device_tiles[index] == NULL)
    return;

  // ── State palette ───────────────────────────────────────────────────────
  // OFF is the resting surface from the design tokens; ON is a single accent
  // treatment applied consistently to fill, outline, glow and badge, so a lit
  // device is readable across the room without inverting the whole theme.
#if UI_TILE_ON_STYLE == 0
  // Luminous: amber-tinted dark fill, amber badge, dark glyph on the badge.
  const lv_color_t bg_on      = lv_color_hex(CLR_HEX_ACCENT_TINT);
  const lv_color_t ic_bg_on   = lv_color_hex(CLR_HEX_ACCENT);
  const lv_color_t txt_on     = lv_color_hex(CLR_HEX_TEXT_HI);
  const lv_color_t ico_on     = lv_color_hex(CLR_HEX_ON_ACCENT);
  const lv_color_t sub_on     = lv_color_hex(CLR_HEX_ACCENT_HI);
  const lv_opa_t   bg_on_opa  = LV_OPA_COVER;
#else
  // Illuminated card: white fill with dark text (Apple-Home look).
  const lv_color_t bg_on      = lv_color_hex(0xFFFFFF);
  const lv_color_t ic_bg_on   = lv_color_hex(CLR_HEX_ACCENT);
  const lv_color_t txt_on     = lv_color_hex(0x111827);
  const lv_color_t ico_on     = lv_color_hex(0xFFFFFF);
  const lv_color_t sub_on     = lv_color_hex(0x6B7280);
  const lv_opa_t   bg_on_opa  = LV_OPA_COVER;
#endif

  // OFF State — resting surface
  const lv_color_t bg_off      = lv_color_hex(CLR_HEX_SURFACE_1);
  const lv_color_t ic_bg_off   = lv_color_hex(CLR_HEX_SURFACE_2);
  const lv_color_t txt_off     = lv_color_hex(CLR_HEX_TEXT_HI);
  const lv_color_t sub_off     = lv_color_hex(CLR_HEX_TEXT_LOW);
  const lv_color_t ico_off     = lv_color_hex(CLR_HEX_TEXT_MID);

  // 1. Tile surface. Flat fill in both states — the RGB565 panel renders a
  // shallow gradient as a handful of single-channel steps (green and purple
  // seams, see ui_helpers.h). OFF stays at 90 % so the wallpaper gives the
  // resting tile its depth; ON goes solid, which is part of what makes a lit
  // device read as lit.
  lv_obj_set_style_bg_color(device_tiles[index], state ? bg_on : bg_off, 0);
  lv_obj_set_style_bg_grad_dir(device_tiles[index], LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(device_tiles[index], state ? bg_on_opa : LV_OPA_90, 0);
  lv_obj_set_style_border_width(device_tiles[index], 1, 0);
  lv_obj_set_style_border_color(device_tiles[index],
                                state ? ic_bg_on : lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(device_tiles[index],
                              state ? LV_OPA_70 : LV_OPA_COVER, 0);

  // Accent glow when ON, neutral drop shadow when OFF
  lv_obj_set_style_shadow_color(device_tiles[index],
                                state ? ic_bg_on : lv_color_black(), 0);
  lv_obj_set_style_shadow_width(device_tiles[index], state ? 18 : 14, 0);
  lv_obj_set_style_shadow_ofs_y(device_tiles[index], state ? 0 : 4, 0);
  lv_obj_set_style_shadow_opa(device_tiles[index],
                              state ? LV_OPA_30 : LV_OPA_40, 0);

  // 2. Icon container — always fully visible so icon is always readable
  lv_obj_set_style_bg_color(device_icon_containers[index],
                            state ? ic_bg_on : ic_bg_off, 0);
  lv_obj_set_style_bg_opa(device_icon_containers[index], LV_OPA_COVER, 0);

  // 3. Text & icon colors — show dimmer % for lamp-type devices when ON.
  // Record staleness and pending on every pass (not just the branch that
  // renders them) so the sweep timer has an accurate baseline to compare
  // against. Pending wins over stale: it is the newer, more actionable fact.
  const bool stale = device_is_stale(index);
  const bool pending = devices[index].isPending();
  s_tile_stale[index] = stale;
  s_tile_pending[index] = pending;

  const char *flag = pending ? "  " LV_SYMBOL_REFRESH
                             : (stale ? "  " LV_SYMBOL_WARNING : "");

  // Status reads the type's own vocabulary: a percentage for a dimmer, a speed
  // for a fan, "cooling" for an AC. A fan at speed 0 is off whatever the power
  // flag says, which is what levelImpliesOff() encodes.
  Device &d = devices[index];
  const int level = d.clampLevel(d.brightness);
  char stat_buf[40]; // lv_label_set_text() copies, so a local is fine
  if (!state) {
    snprintf(stat_buf, sizeof(stat_buf), "%s%s", L(L_OFF), flag);
  } else {
    switch (d.dev_type) {
    case DEV_DIMMER:
      snprintf(stat_buf, sizeof(stat_buf), "%s  %d%%%s", L(L_ON), level, flag);
      break;
    case DEV_FAN:
      if (level == 0)
        snprintf(stat_buf, sizeof(stat_buf), "%s%s", L(L_OFF), flag);
      else {
        char sp[24];
        snprintf(sp, sizeof(sp), L(L_SPEED), level);
        snprintf(stat_buf, sizeof(stat_buf), "%s%s", sp, flag);
      }
      break;
    case DEV_AC:
      snprintf(stat_buf, sizeof(stat_buf), "%s%s", L(L_COOLING), flag);
      break;
    default:
      snprintf(stat_buf, sizeof(stat_buf), "%s%s", L(L_ON), flag);
      break;
    }
  }
  lv_label_set_text(device_status_labels[index], stat_buf);

  // Keep the type's control in step — a level can arrive over MQTT as easily
  // as from a tap here.
  if (device_ctrl[index]) {
    if (d.dev_type == DEV_FAN) {
      lv_btnmatrix_clear_btn_ctrl_all(device_ctrl[index],
                                      LV_BTNMATRIX_CTRL_CHECKED);
      lv_btnmatrix_set_btn_ctrl(device_ctrl[index], level,
                                LV_BTNMATRIX_CTRL_CHECKED);
    } else if (d.dev_type == DEV_AC) {
      lv_label_set_text_fmt(device_ctrl[index], "%d\xC2\xB0", level);
    }
  }

  // While a command is unconfirmed the badge sits at partial opacity, so the
  // tile reads as "asked for, not acknowledged" instead of claiming success.
  lv_obj_set_style_bg_opa(device_icon_containers[index],
                          pending ? LV_OPA_50 : LV_OPA_COVER, 0);

  // Brightness level bar tracks the device and dims when the device is off
  if (device_level_bars[index]) {
    lv_bar_set_value(device_level_bars[index], devices[index].brightness,
                     LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(device_level_bars[index],
                            state ? LV_OPA_COVER : LV_OPA_30, LV_PART_INDICATOR);
  }
  lv_obj_set_style_text_color(device_icons[index], state ? ico_on : ico_off, 0);
  lv_obj_set_style_text_color(device_labels[index], state ? txt_on : txt_off,
                              0);
  lv_obj_set_style_text_color(device_status_labels[index],
                              state ? sub_on : sub_off, 0);

  // --- Fan spin animation ---
  if (devices[index].icon_type == ICON_FAN) {
    if (state) fan_spin_start(device_icon_containers[index]);
    else       fan_spin_stop(device_icon_containers[index]);
  }

  lv_obj_invalidate(device_tiles[index]);

}
