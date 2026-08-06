#include "../../include/scene.h"
#include "../../include/globals.h"
#include "../hal.h"
#include "../lang.h"
#include "../wifi_manager.h"
#include "ui_dimmer_modal.h"
#include "ui_helpers.h"
#include "ui_screens.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <string.h>

// Globals for Dashboard
lv_obj_t *home_time_label = NULL;
lv_obj_t *home_date_label = NULL;
lv_obj_t *home_weather_label = NULL;
lv_obj_t *home_weather_temp_label = NULL;
lv_obj_t *home_weather_city_label = NULL;
static lv_obj_t *home_on_pill = NULL;
static lv_obj_t *home_on_lbl = NULL;

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

// ========================================================
//  ACTIVE TAB MEMORY
// ========================================================
// rebuild_grid() throws the tabview away and builds a new one, which used to
// dump the user back on Home after every device edit, language change or
// settings save. The tab is remembered by *name* rather than index, so it
// survives rooms being added, removed or re-sorted.
#define UI_MAX_TABS 18
static char s_tab_names[UI_MAX_TABS][48];
static int  s_tab_count = 0;
static char s_active_tab_name[48] = "";

static void tabview_changed_cb(lv_event_t *e) {
  uint16_t idx = lv_tabview_get_tab_act(lv_event_get_target(e));
  if (idx < (uint16_t)s_tab_count) {
    strncpy(s_active_tab_name, s_tab_names[idx], sizeof(s_active_tab_name) - 1);
    s_active_tab_name[sizeof(s_active_tab_name) - 1] = '\0';
  }
}

// Records a tab's name as it is created, so the callback above can map an
// index back to a name.
static void tab_register(const char *name) {
  if (s_tab_count >= UI_MAX_TABS) return;
  strncpy(s_tab_names[s_tab_count], name, 47);
  s_tab_names[s_tab_count][47] = '\0';
  s_tab_count++;
}

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
    toggle_device(idx);
  } else if (code == LV_EVENT_LONG_PRESSED) {
    if (devices[idx].dimmer_topic[0] != '\0') {
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

// Room "All OFF" — a one-tap, un-undoable action, so it asks first (device
// deletion already does; switching a whole room off did not).
static char s_pending_off_room[48] = "";

static void all_off_msgbox_cb(lv_event_t *e) {
  lv_obj_t *mbox = lv_event_get_current_target(e);
  if (lv_msgbox_get_active_btn(mbox) == 0 && s_pending_off_room[0]) { // Yes
    for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
      if (!ui_device_is_visible(i)) continue; // never switch the panel itself off
      if (strcmp(devices[i].room, s_pending_off_room) == 0 && devices[i].status)
        toggle_device(i);
    }
  }
  s_pending_off_room[0] = '\0';
  lv_msgbox_close(mbox);
}

static void room_all_off_cb(lv_event_t *e) {
  const char *room = (const char *)lv_event_get_user_data(e);
  if (!room) return;

  int on_now = 0;
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    if (ui_device_is_visible(i) && strcmp(devices[i].room, room) == 0 &&
        devices[i].status)
      on_now++;
  }
  if (on_now == 0) { // nothing to do — don't ask a pointless question
    ui_show_toast(L(L_NOTHING_ON));
    return;
  }

  strncpy(s_pending_off_room, room, sizeof(s_pending_off_room) - 1);
  s_pending_off_room[sizeof(s_pending_off_room) - 1] = '\0';

  // The button map must outlive this call — lv_msgbox keeps the pointer.
  static const char *btns[3];
  btns[0] = L(L_YES);
  btns[1] = L(L_NO);
  btns[2] = "";

  char msg[96];
  snprintf(msg, sizeof(msg), L(L_CONFIRM_ALL_OFF_MSG), room);
  lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), L(L_CONFIRM_ALL_OFF), msg,
                                    btns, false);
  ui_style_msgbox(mbox);
  lv_obj_center(mbox);
  lv_obj_add_event_cb(mbox, all_off_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// ========================================================
//  Q1: SINGLE TILE FACTORY — eliminates ~55 lines of duplication
// ========================================================
// Builds a fully-styled device tile inside `parent` for device at `idx`.
// Returns the tile object and populates the five output pointer params so the
// caller can store them in the global device_* arrays.
// --------------------------------------------------------
static lv_obj_t *create_device_tile(lv_obj_t *parent, int idx, int tile_w,
                                    int tile_h, lv_obj_t **out_icon_cont,
                                    lv_obj_t **out_icon,
                                    lv_obj_t **out_name_lbl,
                                    lv_obj_t **out_stat_lbl,
                                    lv_obj_t **out_level_bar,
                                    bool home_style = false) {
  // Dimmable devices get a level bar along the bottom edge. Long-press to open
  // the brightness modal is otherwise an invisible gesture — the bar is what
  // tells you the tile has a level to adjust, and what that level is.
  const bool has_dimmer = devices[idx].dimmer_topic[0] != '\0';
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

  // --- 1. Icon Container (Badge circle for device icon) ---
  lv_obj_t *ic_cont = lv_obj_create(tile);
  lv_obj_set_size(ic_cont, 44, 44);
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

  // --- 2. Icon label ---
  lv_obj_t *ic = lv_label_create(ic_cont);
  lv_label_set_text(ic, getIconSymbol(devices[idx].icon_type));
  lv_obj_set_style_text_font(ic, &material_icons_font, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_center(ic);
  lv_obj_add_flag(ic, LV_OBJ_FLAG_EVENT_BUBBLE);

  // --- 3. Device name ---
  lv_obj_t *name_lbl = lv_label_create(tile);
  lv_label_set_text(name_lbl, devices[idx].name);
  lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(name_lbl, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_add_flag(name_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

  // --- 4. Status text ("On" / "Off") — one step down in the type scale ---
  lv_obj_t *stat_lbl = lv_label_create(tile);
  lv_label_set_text(stat_lbl, L(L_OFF));
  lv_obj_set_style_text_font(stat_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(stat_lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_add_flag(stat_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

  // --- 5. Brightness level bar (dimmable devices only) ---
  lv_obj_t *level_bar = NULL;
  if (has_dimmer) {
    level_bar = lv_bar_create(tile);
    lv_obj_set_size(level_bar, tile_w - 30, 3);
    lv_obj_align(level_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(level_bar, 0, 100);
    lv_bar_set_value(level_bar, devices[idx].brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(level_bar, lv_color_hex(CLR_HEX_SURFACE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(level_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(level_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(level_bar, lv_color_hex(CLR_HEX_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_radius(level_bar, 2, LV_PART_INDICATOR);
    lv_obj_add_flag(level_bar, LV_OBJ_FLAG_EVENT_BUBBLE);
  }
  // The bar eats 6 px at the bottom, so the text stack lifts and the badge
  // shrinks a little to keep the same gaps on a tile of unchanged height.
  const int lift = has_dimmer ? 6 : 0;

  // --- 6. Layout: badge on top, name, then status — same rhythm everywhere ---
  if (home_style) {
    const int badge = has_dimmer ? 36 : 40;
    lv_obj_set_size(ic_cont, badge, badge);
    lv_obj_align(ic_cont, LV_ALIGN_TOP_MID, 0, has_dimmer ? 2 : 4);

    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, tile_w - 14);
    lv_obj_set_style_text_align(name_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(name_lbl, LV_ALIGN_BOTTOM_MID, 0, -18 - lift);

    lv_label_set_long_mode(stat_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(stat_lbl, tile_w - 14);
    lv_obj_set_style_text_align(stat_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(stat_lbl, LV_ALIGN_BOTTOM_MID, 0, -2 - lift);
  } else {
    if (has_dimmer) lv_obj_set_size(ic_cont, 40, 40);
    lv_obj_align(ic_cont, LV_ALIGN_TOP_MID, 0, has_dimmer ? 2 : 3);

    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, tile_w - 18);
    lv_obj_set_style_text_align(name_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(name_lbl, LV_ALIGN_BOTTOM_MID, 0, -19 - lift);

    lv_label_set_long_mode(stat_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(stat_lbl, tile_w - 18);
    lv_obj_set_style_text_align(stat_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(stat_lbl, LV_ALIGN_BOTTOM_MID, 0, -3 - lift);
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

  return tile;
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
    fav_tiles[i] = NULL;
    fav_icon_containers[i] = NULL;
    fav_icons[i] = NULL;
    fav_labels[i] = NULL;
    fav_status_labels[i] = NULL;
    device_level_bars[i] = NULL;
    fav_level_bars[i] = NULL;
    s_tile_stale[i] = false;
    s_tile_pending[i] = false;
  }
  lv_obj_clean(main_body_container);

  // One sweep timer for the lifetime of the UI; it skips devices with no tile.
  // 1 Hz is fast enough for the pending indicator and costs a couple of
  // comparisons per device.
  if (!s_tile_sweep_timer)
    s_tile_sweep_timer = lv_timer_create(tile_sweep_cb, 1000, NULL);

  if (deviceCount == 0) {
    lv_obj_t *empty_lbl = lv_label_create(main_body_container);
    lv_label_set_text(empty_lbl, L(L_NO_DEVICES));
    lv_obj_set_style_text_color(empty_lbl, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_font(empty_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(empty_lbl);
    return;
  }

  // 1. Find unique rooms (max 16 rooms, 48 chars each to match Device.room)
  // Hidden devices are skipped here too, so a room that only holds the panel's
  // own status entry never gets a tab.
  static char unique_rooms[16][48];
  int num_rooms = 0;
  for (int i = 0; i < deviceCount; i++) {
    if (!ui_device_is_visible(i)) continue;
    bool found = false;
    for (int j = 0; j < num_rooms; j++) {
      if (strcmp(unique_rooms[j], devices[i].room) == 0) {
        found = true;
        break;
      }
    }
    if (!found && num_rooms < 16) {
      strncpy(unique_rooms[num_rooms], devices[i].room, 48);
      unique_rooms[num_rooms][47] = '\0'; // ensure null-termination
      sanitize_visible_text(unique_rooms[num_rooms]);
      // Skip empty room names that became empty after sanitization
      if (unique_rooms[num_rooms][0] == '\0') {
        continue;
      }
      // Re-check for duplicates after sanitization
      bool dup = false;
      for (int j = 0; j < num_rooms; j++) {
        if (strcmp(unique_rooms[j], unique_rooms[num_rooms]) == 0) { dup = true; break; }
      }
      if (dup) continue;
      num_rooms++;
    }
    safe_wdt_reset(); // Feed while searching
  }

  // Sort room names alphabetically (case-insensitive) so room tabs after "Home"
  // appear in stable, predictable order regardless of devices.json ordering.
  for (int a = 0; a < num_rooms - 1; a++) {
    for (int b = 0; b < num_rooms - 1 - a; b++) {
      if (strcasecmp(unique_rooms[b], unique_rooms[b + 1]) > 0) {
        char tmp[48];
        strncpy(tmp, unique_rooms[b], 48);
        strncpy(unique_rooms[b], unique_rooms[b + 1], 48);
        strncpy(unique_rooms[b + 1], tmp, 48);
      }
    }
  }

  // 2. Create Tabview
  lv_obj_t *room_tabview =
      lv_tabview_create(main_body_container, LV_DIR_TOP, UI_TABBAR_HEIGHT);
  lv_obj_set_size(room_tabview, LV_PCT(100), LV_PCT(100));

  // Make the entire Tabview structure transparent to inherit the seamless
  // deep-space background
  lv_obj_set_style_bg_opa(room_tabview, LV_OPA_TRANSP, 0);
  lv_obj_t *tab_content = lv_tabview_get_content(room_tabview);
  lv_obj_set_style_bg_opa(tab_content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tab_content, 0, 0);
  lv_obj_set_style_pad_all(tab_content, 0,
                           0); // Explicitly clear any default theme padding
  // Prevent swipe gestures on tab content from stealing tile taps.
  lv_obj_clear_flag(tab_content, LV_OBJ_FLAG_SCROLLABLE);

  // ── Tab bar: quiet scrim, text-only tabs, amber underline on the active one.
  // The stock theme paints a filled block behind the selected tab, which fights
  // the tiles for attention — replaced here with a 3 px indicator rule.
  lv_obj_t *tab_btns = lv_tabview_get_tab_btns(room_tabview);
  // Scrim at 90 %, matching the cards. At the old 70 % a bright wallpaper lifted
  // the strip to (65,80,90), which put the inactive tab labels at 2.0:1 — and no
  // colour in the text ramp could rescue them without becoming TEXT_MID.
  lv_obj_set_style_bg_color(tab_btns, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_90, 0);
  lv_obj_set_style_border_color(tab_btns, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(tab_btns, LV_OPA_50, 0);
  lv_obj_set_style_border_width(tab_btns, 1, 0);
  lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_14, 0);
  lv_obj_set_style_pad_all(tab_btns, 0, 0);

  // Inactive tab item
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_text_color(tab_btns, lv_color_hex(CLR_HEX_TEXT_LOW), LV_PART_ITEMS);
  lv_obj_set_style_border_width(tab_btns, 0, LV_PART_ITEMS);
  lv_obj_set_style_radius(tab_btns, 0, LV_PART_ITEMS);
  lv_obj_set_style_pad_all(tab_btns, 0, LV_PART_ITEMS);

  // Active tab item — brightest label + accent underline
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(tab_btns, lv_color_hex(CLR_HEX_TEXT_HI),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(tab_btns, lv_color_hex(CLR_HEX_ACCENT),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(tab_btns, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_BOTTOM,
                               LV_PART_ITEMS | LV_STATE_CHECKED);

  // Press feedback on a tab
  lv_obj_set_style_bg_color(tab_btns, lv_color_hex(CLR_HEX_ACCENT), LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_10, LV_PART_ITEMS | LV_STATE_PRESSED);

  // 3. Home Tab — Premium Dashboard
  s_tab_count = 0; // rebuilt below in the same order the tabs are added
  lv_obj_t *home_tab = lv_tabview_add_tab(room_tabview, "Home");
  tab_register("Home");
  lv_obj_set_scrollbar_mode(home_tab, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(home_tab, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(home_tab, 0, 0);
  lv_obj_set_style_pad_all(home_tab, 0, 0);

  // Date / weather / device-count moved to global header.
  // Home tab now hosts a NS-Panel-style weather hero card on the left and
  // the Favorites grid on the right.
  // Customisation: clearing the Weather City in Settings hides the weather
  // card and lets the favourites grid take the full home tab width.
  home_date_label    = NULL;
  home_on_pill       = NULL;
  home_on_lbl        = NULL;

  // Home layout: 0=Modern (weather hero + favourites), 1=Classic (full-width grid)
  const bool show_weather_card = (homeLayoutStyle == 0) && (weatherCity[0] != '\0');
  const int  fav_left_offset   = show_weather_card ? (UI_WEATHER_CARD_W + 18) : 0;

  if (show_weather_card) {
  // ── Weather hero card (left) ──
  // A single column of information with a clear top-to-bottom hierarchy:
  //   city → glyph + temperature → condition → rule → house summary.
  // The rule and summary anchor the bottom so the card never reads as a
  // half-empty box, which is what the old layout looked like.
  lv_obj_t *weather_card = lv_obj_create(home_tab);
  lv_obj_set_size(weather_card, UI_WEATHER_CARD_W, LV_PCT(100) - 8);
  lv_obj_align(weather_card, LV_ALIGN_LEFT_MID, 10, 0);
  // Same 90 % flat fill as the tiles it sits beside — at cover it was the one
  // opaque surface on the home tab and read as a slab next to them. The card
  // carries the smallest type on the screen (TEXT_LOW at 12 px), so the cost
  // was measured: against the shipped wallpaper the fill only ever darkens,
  // leaving that text at 3.95:1 either way. A bright wallpaper takes it to
  // ~3.1:1 — the same exposure the tiles' On/Off labels already run with.
  lv_obj_set_style_bg_color(weather_card, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_dir(weather_card, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(weather_card, LV_OPA_90, 0);
  lv_obj_set_style_radius(weather_card, 20, 0);
  lv_obj_set_style_border_width(weather_card, 1, 0);
  lv_obj_set_style_border_color(weather_card, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(weather_card, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_color(weather_card, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(weather_card, 16, 0);
  lv_obj_set_style_shadow_ofs_y(weather_card, 4, 0);
  lv_obj_set_style_shadow_opa(weather_card, LV_OPA_40, 0);
  lv_obj_set_style_pad_all(weather_card, 14, 0);
  lv_obj_clear_flag(weather_card, LV_OBJ_FLAG_SCROLLABLE);

  // City label (top) — small caps-ish eyebrow, widest tracking on the screen
  lv_obj_t *w_city = lv_label_create(weather_card);
  {
    char city_buf[64];
    strncpy(city_buf,
            weatherValid && weatherCityName[0] ? weatherCityName : weatherCity,
            sizeof(city_buf) - 1);
    city_buf[sizeof(city_buf) - 1] = '\0';
    sanitize_visible_text(city_buf);
    lv_label_set_text(w_city, city_buf);
  }
  lv_obj_set_style_text_font(w_city, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(w_city, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_set_style_text_letter_space(w_city, 1, 0);
  lv_label_set_long_mode(w_city, LV_LABEL_LONG_DOT);
  lv_obj_set_width(w_city, UI_WEATHER_CARD_W - 28);
  lv_obj_align(w_city, LV_ALIGN_TOP_LEFT, 0, 0);
  home_weather_city_label = w_city;

  // Condition glyph — the sun mark from the Material set, in accent
  lv_obj_t *w_ico = lv_label_create(weather_card);
  lv_label_set_text(w_ico, "\xEE\x94\x98"); // E518 light_mode
  lv_obj_set_style_text_font(w_ico, &material_icons_font, 0);
  lv_obj_set_style_text_color(w_ico, lv_color_hex(CLR_HEX_ACCENT), 0);
  lv_obj_align(w_ico, LV_ALIGN_TOP_LEFT, 0, 24);

  // Big temperature — the single largest element in the body
  lv_obj_t *w_temp = lv_label_create(weather_card);
  if (weatherValid) {
    static char tbuf[16];
    snprintf(tbuf, sizeof(tbuf), "%.0f\xC2\xB0", weatherTemp);
    lv_label_set_text(w_temp, tbuf);
  } else {
    lv_label_set_text(w_temp, "--\xC2\xB0");
  }
  lv_obj_set_style_text_font(w_temp, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(w_temp, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_align(w_temp, LV_ALIGN_TOP_LEFT, 0, 56);
  home_weather_temp_label = w_temp;

  // Condition / description
  lv_obj_t *w_desc = lv_label_create(weather_card);
  lv_label_set_text(w_desc, weatherValid ? weatherDesc : "Loading...");
  lv_obj_set_style_text_font(w_desc, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(w_desc, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_label_set_long_mode(w_desc, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(w_desc, UI_WEATHER_CARD_W - 28);
  lv_obj_align(w_desc, LV_ALIGN_TOP_LEFT, 0, 118);
  home_weather_label = w_desc;

  // Footer: rule + live house summary ("n of n on"), kept in sync by
  // ui_update_device_status() through home_on_pill / home_on_lbl.
  lv_obj_t *w_rule = ui_create_divider(weather_card, UI_WEATHER_CARD_W - 28);
  lv_obj_align(w_rule, LV_ALIGN_BOTTOM_LEFT, 0, -26);

  home_on_pill = ui_create_dot(weather_card, 8, lv_color_hex(CLR_HEX_TEXT_LOW));
  lv_obj_align(home_on_pill, LV_ALIGN_BOTTOM_LEFT, 1, -5);

  home_on_lbl = lv_label_create(weather_card);
  lv_label_set_text_fmt(home_on_lbl, "0 of %d on", deviceCount);
  lv_obj_set_style_text_font(home_on_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(home_on_lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(home_on_lbl, LV_ALIGN_BOTTOM_LEFT, 16, 0);
  } else {
    // Weather hidden — clear pointers so update_home_dashboard skips them
    home_weather_label      = NULL;
    home_weather_temp_label = NULL;
    home_weather_city_label = NULL;
  }

  // ── Favorites container (right of weather card, or full width if hidden) ──
  // Vertical scrolling enabled so >2 favourites are reachable.
  lv_obj_t *fav_container = lv_obj_create(home_tab);
  lv_obj_set_size(fav_container, SCREEN_WIDTH - fav_left_offset, LV_PCT(100));
  lv_obj_align(fav_container, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_bg_opa(fav_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(fav_container, 0, 0);
  lv_obj_set_style_pad_all(fav_container, 0, 0);
  lv_obj_set_flex_flow(fav_container, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(fav_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_top(fav_container, 8, 0);
  lv_obj_set_style_pad_bottom(fav_container, 8, 0);
  lv_obj_set_style_pad_row(fav_container, 10, 0);
  lv_obj_set_style_pad_column(fav_container, 10, 0);
  lv_obj_set_style_pad_left(fav_container, 4, 0);
  lv_obj_set_style_pad_right(fav_container, 4, 0);
  lv_obj_add_flag(fav_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(fav_container, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(fav_container, LV_SCROLLBAR_MODE_AUTO);
  ui_style_scrollbar(fav_container);

  // 3b. Scenes Tab (only if scenes exist)
  if (sceneCount > 0) {
    lv_obj_t *scenes_tab = lv_tabview_add_tab(room_tabview, "Scenes");
    tab_register("Scenes");
    lv_obj_set_scrollbar_mode(scenes_tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(scenes_tab, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scenes_tab, 0, 0);
    lv_obj_set_style_pad_all(scenes_tab, 0, 0);
    lv_obj_set_flex_flow(scenes_tab, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(scenes_tab, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(scenes_tab, 10, 0);
    lv_obj_set_style_pad_bottom(scenes_tab, 8, 0);
    lv_obj_set_style_pad_row(scenes_tab, 10, 0);
    lv_obj_set_style_pad_column(scenes_tab, 10, 0);
    lv_obj_set_style_pad_left(scenes_tab, 14, 0);
    lv_obj_set_style_pad_right(scenes_tab, 14, 0);
    lv_obj_add_flag(scenes_tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scenes_tab, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scenes_tab, LV_SCROLLBAR_MODE_AUTO);
    ui_style_scrollbar(scenes_tab);
    create_scene_tiles(scenes_tab);
  }

  // 4. Room Tabs
  // Tile dimensions
  int tile_w = UI_TILE_W;
  int tile_h = UI_TILE_H;
  int max_fav_count = UI_MAX_FAV_NORMAL;

  if (useLargeTiles) {
    tile_w = UI_TILE_LARGE_W;
    tile_h = UI_TILE_H;
    max_fav_count = UI_MAX_FAV_LARGE;
  }

  lv_obj_t *tabs[16]; // max 16 unique rooms
  for (int i = 0; i < num_rooms; i++) {
    tabs[i] = lv_tabview_add_tab(room_tabview, unique_rooms[i]);
    tab_register(unique_rooms[i]);
    lv_obj_set_style_bg_opa(tabs[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tabs[i], 0, 0);
    lv_obj_set_style_pad_all(
        tabs[i], 0, 0); // Strip default tab padding to prevent grid wrap
    lv_obj_set_flex_flow(tabs[i], LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(tabs[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(tabs[i], 6, 0); // Match Home tab alignment
    lv_obj_set_style_pad_bottom(tabs[i], 8, 0);
    lv_obj_set_style_pad_row(tabs[i], 10, 0);
    lv_obj_set_style_pad_column(tabs[i], 10, 0);
    lv_obj_set_style_pad_left(tabs[i], 14, 0);
    lv_obj_set_style_pad_right(tabs[i], 14, 0);
    // Vertical scroll only: rooms with more than one row of devices used to be
    // unreachable. Horizontal swipe stays disabled so it can't steal tile taps.
    lv_obj_add_flag(tabs[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tabs[i], LV_DIR_VER);
    lv_obj_set_scrollbar_mode(tabs[i], LV_SCROLLBAR_MODE_AUTO);
    ui_style_scrollbar(tabs[i]);

    // ── Room action row: full-width so it always owns the first line ──
    lv_obj_t *act_row = lv_obj_create(tabs[i]);
    lv_obj_set_size(act_row, LV_PCT(100), 30);
    lv_obj_set_style_bg_opa(act_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(act_row, 0, 0);
    lv_obj_set_style_pad_all(act_row, 0, 0);
    lv_obj_clear_flag(act_row, LV_OBJ_FLAG_SCROLLABLE);

    // Device count for this room, left-aligned
    int room_n = 0;
    for (int d = 0; d < deviceCount && d < MAX_DEVICES; d++) {
      if (ui_device_is_visible(d) &&
          strcmp(devices[d].room, unique_rooms[i]) == 0) room_n++;
    }
    lv_obj_t *cnt_lbl = lv_label_create(act_row);
    lv_label_set_text_fmt(cnt_lbl, "%d %s", room_n, L(L_DEVICES));
    lv_obj_set_style_text_font(cnt_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cnt_lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_align(cnt_lbl, LV_ALIGN_LEFT_MID, 2, 0);

    // "All OFF" — outlined danger pill, right-aligned. Outlined rather than
    // filled so a destructive action never dominates the room view.
    lv_obj_t *off_btn = lv_btn_create(act_row);
    lv_obj_set_size(off_btn, LV_SIZE_CONTENT, 28);
    lv_obj_align(off_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(off_btn, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_bg_opa(off_btn, LV_OPA_20, 0);
    lv_obj_set_style_border_color(off_btn, lv_color_hex(CLR_HEX_DANGER), 0);
    lv_obj_set_style_border_width(off_btn, 1, 0);
    lv_obj_set_style_border_opa(off_btn, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(off_btn, 0, 0);
    lv_obj_set_style_radius(off_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(off_btn, 14, 0);
    lv_obj_set_style_pad_ver(off_btn, 4, 0);
    lv_obj_set_style_bg_opa(off_btn, LV_OPA_50, LV_STATE_PRESSED);
    lv_obj_add_event_cb(off_btn, room_all_off_cb, LV_EVENT_CLICKED,
                        (void *)unique_rooms[i]);
    lv_obj_t *off_lbl = lv_label_create(off_btn);
    lv_label_set_text_fmt(off_lbl, LV_SYMBOL_POWER "  %s", L(L_ALL_OFF));
    lv_obj_set_style_text_font(off_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(off_lbl, lv_color_hex(CLR_HEX_DANGER_HI), 0);
    lv_obj_center(off_lbl);
  }

  // 5. Create Device Tiles using the shared factory function
  // Favorites use compact dimensions so 2 columns fit beside the weather card
  // (container ~280 wide, tile_fav_w 130 + 8 gap → 2 per row, vertical scroll
  //  handles overflow when there are many favourites).
  // Classic layout (no weather card) gives 3 columns of larger tiles.
  const int fav_tile_w = show_weather_card ? UI_FAV_TILE_W : 150;
  const int fav_tile_h = show_weather_card ? UI_FAV_TILE_H : UI_TILE_H;
  const int fav_max    = MAX_DEVICES; // allow all favourites; container scrolls

  int fav_count = 0;
  for (int i = 0; i < deviceCount && i < MAX_DEVICES; i++) {
    // Panel status entry: no tile anywhere. Its pointers stay NULL, which
    // ui_update_device_status() already treats as "nothing to redraw".
    if (!ui_device_is_visible(i)) continue;

    if (devices[i].is_favorite && fav_count < fav_max) {
      // --- Favorite tile: placed on the Home tab (tracked for live updates) ---
      fav_tiles[i] = create_device_tile(
          fav_container, i, fav_tile_w, fav_tile_h, &fav_icon_containers[i],
          &fav_icons[i], &fav_labels[i], &fav_status_labels[i],
          &fav_level_bars[i], /*home_style=*/true);
      fav_count++;
      // Colours are applied by ui_update_device_status() below, which mirrors
      // the room tile onto this one.
    }

    // --- Room tile: placed in the matching room tab (tracked for live updates) ---
    lv_obj_t *parent_tab = tabs[0];
    for (int j = 0; j < num_rooms; j++) {
      if (strcmp(unique_rooms[j], devices[i].room) == 0) {
        parent_tab = tabs[j];
        break;
      }
    }

    device_tiles[i] = create_device_tile(
        parent_tab, i, tile_w, tile_h, &device_icon_containers[i],
        &device_icons[i], &device_labels[i], &device_status_labels[i],
        &device_level_bars[i]);

    // Always apply the correct colors/state for each tile (not just ON)
    ui_update_device_status(i, devices[i].status);

    safe_wdt_reset(); // Feed while creating 100 tiles
  }

  // Restore whichever tab the user was last on, matched by name so it still
  // lands correctly when rooms have been added, removed or re-sorted.
  if (s_active_tab_name[0]) {
    for (int t = 0; t < s_tab_count; t++) {
      if (strcmp(s_tab_names[t], s_active_tab_name) == 0) {
        lv_tabview_set_act(room_tabview, (uint16_t)t, LV_ANIM_OFF);
        break;
      }
    }
  }
  lv_obj_add_event_cb(room_tabview, tabview_changed_cb, LV_EVENT_VALUE_CHANGED,
                      NULL);

  // Show placeholder in Home tab when no favorites are set
  if (fav_count == 0) {
    lv_obj_t *hint = lv_label_create(fav_container);
    static char fav_buf[128];
    snprintf(fav_buf, sizeof(fav_buf), LV_SYMBOL_SETTINGS "  %s", L(L_FAV_HINT));
    lv_label_set_text(hint, fav_buf);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(hint, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_set_style_pad_top(hint, 48, 0);
  }
}

// ========================================================
//  DEVICE STATUS UPDATE
// ========================================================
void ui_update_device_status(int index, bool state) {
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

  char stat_buf[40]; // lv_label_set_text() copies, so a local is fine
  if (state && devices[index].dimmer_topic[0] != '\0') {
    snprintf(stat_buf, sizeof(stat_buf), "%s  %d%%%s", L(L_ON),
             devices[index].brightness, flag);
  } else {
    snprintf(stat_buf, sizeof(stat_buf), "%s%s", state ? L(L_ON) : L(L_OFF),
             flag);
  }
  lv_label_set_text(device_status_labels[index], stat_buf);

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

  // --- Update Favorite tile mirror (Home tab) ---
  if (fav_tiles[index] != NULL) {
    lv_obj_set_style_bg_color(fav_tiles[index], state ? bg_on : bg_off, 0);
    lv_obj_set_style_bg_grad_dir(fav_tiles[index], LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_opa(fav_tiles[index], state ? bg_on_opa : LV_OPA_90, 0);
    lv_obj_set_style_border_width(fav_tiles[index], 1, 0);
    lv_obj_set_style_border_color(fav_tiles[index],
                                  state ? ic_bg_on : lv_color_hex(CLR_HEX_HAIRLINE), 0);
    lv_obj_set_style_border_opa(fav_tiles[index], state ? LV_OPA_70 : LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_color(fav_tiles[index],
                                  state ? ic_bg_on : lv_color_black(), 0);
    lv_obj_set_style_shadow_width(fav_tiles[index], state ? 18 : 14, 0);
    lv_obj_set_style_shadow_ofs_y(fav_tiles[index], state ? 0 : 4, 0);
    lv_obj_set_style_shadow_opa(fav_tiles[index], state ? LV_OPA_30 : LV_OPA_40, 0);
    lv_obj_set_style_bg_color(fav_icon_containers[index], state ? ic_bg_on : ic_bg_off, 0);
    lv_obj_set_style_bg_opa(fav_icon_containers[index],
                            pending ? LV_OPA_50 : LV_OPA_COVER, 0);
    if (fav_level_bars[index]) {
      lv_bar_set_value(fav_level_bars[index], devices[index].brightness,
                       LV_ANIM_OFF);
      lv_obj_set_style_bg_opa(fav_level_bars[index],
                              state ? LV_OPA_COVER : LV_OPA_30, LV_PART_INDICATOR);
    }
    lv_obj_set_style_text_color(fav_icons[index], state ? ico_on : ico_off, 0);
    lv_obj_set_style_text_color(fav_labels[index], state ? txt_on : txt_off, 0);
    lv_obj_set_style_text_color(fav_status_labels[index], state ? sub_on : sub_off, 0);
    lv_label_set_text(fav_status_labels[index], lv_label_get_text(device_status_labels[index]));
    // Fan spin on fav tile too
    if (devices[index].icon_type == ICON_FAN) {
      if (state) fan_spin_start(fav_icon_containers[index]);
      else       fan_spin_stop(fav_icon_containers[index]);
    }
    lv_obj_invalidate(fav_tiles[index]);
  }

  // --- Update the house summary on the weather card ---
  if (home_on_lbl && home_on_pill) {
    int cnt = 0, total = 0;
    ui_count_visible_devices(&cnt, &total);
    static char on_cnt_buf[32];
    snprintf(on_cnt_buf, sizeof(on_cnt_buf), "%d of %d on", cnt, total);
    lv_label_set_text(home_on_lbl, on_cnt_buf);
    lv_obj_set_style_bg_color(home_on_pill,
                              cnt > 0 ? lv_color_hex(CLR_HEX_ACCENT)
                                      : lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_set_style_text_color(home_on_lbl,
                                cnt > 0 ? lv_color_hex(CLR_HEX_TEXT_MID)
                                        : lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  }
}

// ========================================================
//  HOME DASHBOARD UPDATER
// ========================================================
void update_home_dashboard() {
  // Update date label
  if (home_date_label) {
    lv_label_set_text(home_date_label, currentDate);
    lv_obj_align(home_date_label, LV_ALIGN_LEFT_MID, 0, weatherValid ? -11 : 0);
  }
  // Big temperature on the weather hero card
  if (home_weather_temp_label) {
    if (weatherValid) {
      static char tbuf[16];
      snprintf(tbuf, sizeof(tbuf), "%.0f\xC2\xB0", weatherTemp);
      lv_label_set_text(home_weather_temp_label, tbuf);
    } else {
      lv_label_set_text(home_weather_temp_label, "--\xC2\xB0");
    }
  }
  // City on the weather hero card
  if (home_weather_city_label) {
    char city_buf[64];
    strncpy(city_buf,
            weatherValid && weatherCityName[0] ? weatherCityName : "Weather",
            sizeof(city_buf) - 1);
    city_buf[sizeof(city_buf) - 1] = '\0';
    sanitize_visible_text(city_buf);
    lv_label_set_text(home_weather_city_label, city_buf);
  }
  // Description / condition
  if (home_weather_label) {
    lv_label_set_text(home_weather_label,
                      weatherValid ? weatherDesc : "No data");
  }
}
