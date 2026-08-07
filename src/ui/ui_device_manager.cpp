#include "../mqtt_manager.h"
#include "../lang.h"
#include "../wifi_manager.h" // pulls in globals.h → deviceCount, devices, etc.
#include "ui_helpers.h"
#include "ui_nav_rail.h"
#include "ui_screens.h"
#include <string.h>

extern lv_obj_t *ta_dev_name;
extern lv_obj_t *ta_dev_stat;
extern lv_obj_t *ta_dev_cmnd;
extern lv_obj_t *ta_dev_dimmer;
extern lv_obj_t *dd_icon;
extern lv_obj_t *kb_edit;

static int editDeviceIndex = -1;
static int device_to_delete = -1;
static lv_obj_t *dev_count_lbl = NULL;

// ── Inline rename ───────────────────────────────────────
// Tapping a name swaps it for a textarea rather than opening the full edit
// form: renaming is the one thing people do often, and it does not need the
// topic fields. The full form is still one tap away on the Edit pill.
static lv_obj_t *kb_list = NULL;
static int rename_index = -1;

// Re-rendering destroys the widget that dispatched the event, so it waits for
// LVGL to finish the dispatch. Tearing the keyboard down here rather than in
// the callback is the same reason.
static void relist_async(void *unused) {
  (void)unused;
  if (kb_list) {
    lv_obj_del(kb_list);
    kb_list = NULL;
  }
  build_device_list_screen();
  rebuild_grid(); // tiles and room cards carry the name too
}

static void rename_ta_cb(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  const bool commit = (code == LV_EVENT_READY || code == LV_EVENT_DEFOCUSED);
  if (!commit && code != LV_EVENT_CANCEL) return;
  if (rename_index < 0) return; // the other event already handled it

  const int idx = rename_index;
  rename_index = -1; // READY is followed by DEFOCUSED — act once

  if (commit) {
    String n = lv_textarea_get_text(lv_event_get_target(e));
    n.trim();
    if (n.isEmpty()) {
      ui_show_toast(L(L_PANEL_NAME_EMPTY)); // keeps the old name
    } else if (idx < deviceCount) {
      strncpy(devices[idx].name, n.c_str(), sizeof(devices[0].name) - 1);
      devices[idx].name[sizeof(devices[0].name) - 1] = '\0';
      saveDevices();
    }
  }
  lv_async_call(relist_async, NULL);
}

static void name_tap_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= deviceCount) return;
  rename_index = idx;

  // The label is hidden, never deleted: it is the object currently dispatching
  // this event, and freeing it here would leave LVGL walking a dead pointer.
  // relist_async() rebuilds the row from scratch a moment later anyway.
  lv_obj_t *lbl = lv_event_get_target(e);
  lv_obj_t *row = lv_obj_get_parent(lbl);
  lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *ta = lv_textarea_create(row);
  lv_obj_set_size(ta, 196, 34);
  lv_obj_align(ta, LV_ALIGN_LEFT_MID, 50, 0);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_text(ta, devices[idx].name);
  lv_obj_set_style_bg_color(ta, lv_color_hex(CLR_HEX_SURFACE_2), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(CLR_HEX_ACCENT), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_radius(ta, 8, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, 0);
  ui_style_textarea_states(ta);
  lv_obj_add_event_cb(ta, rename_ta_cb, LV_EVENT_ALL, NULL);

  if (kb_list) lv_obj_del(kb_list);
  kb_list = lv_keyboard_create(lv_obj_get_screen(row));
  ui_style_keyboard(kb_list);
  lv_obj_set_size(kb_list, SCREEN_WIDTH, 136);
  lv_obj_align(kb_list, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_move_foreground(kb_list);
  lv_keyboard_set_textarea(kb_list, ta);
}

// Tapping the room chip moves the device to the next room. Cycling suits the
// three or four rooms a panel usually has; past about six it wants a picker,
// so the chip falls back to a no-op rather than becoming a guessing game.
static void room_chip_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= deviceCount || roomCount < 2) return;

  int cur = room_find(devices[idx].room);
  int next = (cur < 0) ? 0 : (cur + 1) % roomCount;
  strncpy(devices[idx].room, rooms[next].name, sizeof(devices[0].room) - 1);
  devices[idx].room[sizeof(devices[0].room) - 1] = '\0';
  saveDevices();
  lv_async_call(relist_async, NULL);
}

static void _del_old_screen(void *obj) { if (obj) lv_obj_del((lv_obj_t *)obj); }

void cleanup_device_screen() {
  if (ui_ScreenEditDevice) {
    lv_anim_del(ui_ScreenEditDevice, NULL);
    lv_obj_del(ui_ScreenEditDevice);
    ui_ScreenEditDevice = NULL;
  }
  if (ui_ScreenDevices) {
    lv_anim_del(ui_ScreenDevices, NULL);
    lv_obj_del(ui_ScreenDevices);
    ui_ScreenDevices = NULL;
    device_list_container = NULL;
    dev_count_lbl = NULL;
  }
  // The rename keyboard is parented to the screen, so it goes with it.
  kb_list = NULL;
  rename_index = -1;
}

void btn_back_to_settings_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    device_list_container = NULL;
    dev_count_lbl = NULL;
    lv_scr_load_anim(ui_ScreenSettings, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  }
}

static void btn_add_device_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    editDeviceIndex = -1;
    build_edit_device_screen();
    lv_scr_load_anim(ui_ScreenEditDevice, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0,
                     false);
  }
}

static void btn_edit_device_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    editDeviceIndex = (int)(ptrdiff_t)lv_event_get_user_data(e);
    build_edit_device_screen();
    lv_scr_load_anim(ui_ScreenEditDevice, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0,
                     false);
  }
}

static void msgbox_event_cb(lv_event_t *e) {
  lv_obj_t *msgbox = lv_event_get_current_target(e);
  if (lv_msgbox_get_active_btn(msgbox) == 0) { // Yes
    if (device_to_delete >= 0) {
      deleteDevice(device_to_delete);
      build_device_list_screen();
      lv_scr_load_anim(ui_ScreenDevices, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0,
                       false);
    }
  }
  lv_msgbox_close(msgbox);
}

static void btn_delete_device_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    device_to_delete = (int)(ptrdiff_t)lv_event_get_user_data(e);
    // The button map must outlive this call — lv_msgbox keeps the pointer.
    static const char *btns[3];
    btns[0] = L(L_YES);
    btns[1] = L(L_NO);
    btns[2] = "";
    lv_obj_t *mbox = lv_msgbox_create(
        ui_ScreenDevices, L(L_CONFIRM_DELETE),
        L(L_CONFIRM_DELETE_MSG), btns, false);
    ui_style_msgbox(mbox);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, msgbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  }
}

static void btn_save_device_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    const char *name = lv_textarea_get_text(ta_dev_name);
    const char *stat = lv_textarea_get_text(ta_dev_stat);
    const char *cmnd = lv_textarea_get_text(ta_dev_cmnd);
    const char *dimmer = lv_textarea_get_text(ta_dev_dimmer);
    int icon = lv_dropdown_get_selected(dd_icon);
    if (editDeviceIndex >= 0 && editDeviceIndex < deviceCount) {
      strncpy(devices[editDeviceIndex].name, name, sizeof(devices[0].name) - 1);
      strncpy(devices[editDeviceIndex].state_topic, stat,
              sizeof(devices[0].state_topic) - 1);
      strncpy(devices[editDeviceIndex].cmnd_topic, cmnd,
              sizeof(devices[0].cmnd_topic) - 1);
      strncpy(devices[editDeviceIndex].dimmer_topic, dimmer,
              sizeof(devices[0].dimmer_topic) - 1);
      devices[editDeviceIndex].icon_type = icon;
      saveDevices();
    } else {
      // Web Portal is used to configure Rooms. We default to "Living Room"
      // here.
      addDevice(name, "Living Room", stat, cmnd, dimmer, icon, false);
    }
    build_device_list_screen();
    lv_obj_t *old_edit = ui_ScreenEditDevice;
    ui_ScreenEditDevice = NULL;
    lv_scr_load_anim(ui_ScreenDevices, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    lv_async_call(_del_old_screen, old_edit);
  }
}

static void btn_cancel_edit_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *old = ui_ScreenEditDevice;
    ui_ScreenEditDevice = NULL;
    lv_scr_load_anim(ui_ScreenDevices, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    lv_async_call(_del_old_screen, old);
  }
}

static void ta_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  lv_obj_t *keyboard = (lv_obj_t *)lv_event_get_user_data(e);
  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(keyboard, ta);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keyboard);
    lv_obj_scroll_to_view(ta, LV_ANIM_ON);
  }
  if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY ||
      code == LV_EVENT_CANCEL) {
    lv_keyboard_set_textarea(keyboard, NULL);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

void build_device_list_screen() {
  // Free other sub-screens first to keep LVGL heap healthy
  cleanup_scene_screen();
  // Free screensaver if lingering
  if (ui_ScreenSaver) { lv_anim_del(ui_ScreenSaver, NULL); lv_obj_del(ui_ScreenSaver); ui_ScreenSaver = NULL; }

  // ── If screen already exists, just refresh the list ──
  if (ui_ScreenDevices && device_list_container) {
    lv_obj_clean(device_list_container);
    if (dev_count_lbl) {
      char cnt[8];
      snprintf(cnt, sizeof(cnt), "%d", deviceCount);
      lv_label_set_text(dev_count_lbl, cnt);
    }
    goto populate;
  }

  // Delete stale screen (back was pressed, container cleared)
  if (ui_ScreenDevices) {
    lv_anim_del(ui_ScreenDevices, NULL);
    lv_obj_del(ui_ScreenDevices);
    ui_ScreenDevices = NULL;
  }

  // ── First time: create screen + header (persists) ──
  ui_ScreenDevices = lv_obj_create(NULL);
  // Rail first, so the header and body that follow stack above it.
  ui_nav_rail_create(ui_ScreenDevices, UI_NAV_SETTINGS);

  // Deep black background
  lv_obj_set_style_bg_color(ui_ScreenDevices, CLR_BG_DEEP, 0);

  {
  // ── Frosted glass header ────────────────────
  lv_obj_t *hdr = ui_create_frosted_header(ui_ScreenDevices, UI_SETTINGS_HDR_HEIGHT);

  // Back — pill
  lv_obj_t *btn_back = ui_create_pill_btn(hdr, UI_PILL_BTN_W, UI_PILL_BTN_H,
                                          LV_SYMBOL_LEFT, CLR_PRIMARY,
                                          btn_back_to_settings_cb, NULL,
                                          LV_EVENT_CLICKED);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 8, 0);

  // Title
  lv_obj_t *lbl_title = lv_label_create(hdr);
  lv_label_set_text(lbl_title, L(L_DEVICES));
  lv_obj_set_style_text_color(lbl_title, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
  lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 62, 0);

  // Count badge
  char cnt[24];
  snprintf(cnt, sizeof(cnt), "%d", deviceCount);
  lv_obj_t *badge = lv_obj_create(hdr);
  lv_obj_set_size(badge, 28, 22);
  lv_obj_align(badge, LV_ALIGN_LEFT_MID, 138, 0);
  lv_obj_set_style_bg_color(badge, lv_color_hex(CLR_HEX_BADGE_BG), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(badge, 0, 0);
  lv_obj_set_style_shadow_width(badge, 0, 0);
  lv_obj_set_style_pad_all(badge, 0, 0);
  lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  dev_count_lbl = lv_label_create(badge);
  lv_label_set_text(dev_count_lbl, cnt);
  lv_obj_set_style_text_color(dev_count_lbl, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(dev_count_lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(dev_count_lbl);

  // Add button — amber accent
  lv_obj_t *btn_add = ui_create_accent_btn(hdr, 80, UI_PILL_BTN_H, "",
                                           btn_add_device_cb, NULL,
                                           LV_EVENT_CLICKED);
  lv_obj_align(btn_add, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_label_set_text_fmt(lv_obj_get_child(btn_add, 0), LV_SYMBOL_PLUS " %s",
                        L(L_ADD));

  // ── Scrollable list ─────────────────────────
  device_list_container = lv_obj_create(ui_ScreenDevices);
  lv_obj_set_size(device_list_container, UI_CONTENT_W, SCREEN_HEIGHT - 52);
  lv_obj_align(device_list_container, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_flex_flow(device_list_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(device_list_container, 10, 0);
  lv_obj_set_style_pad_row(device_list_container, 6, 0);
  lv_obj_set_style_bg_opa(device_list_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(device_list_container, 0, 0);
  lv_obj_set_scrollbar_mode(device_list_container, LV_SCROLLBAR_MODE_AUTO);
  } // end header block

populate:
  for (int i = 0; i < deviceCount; i++) {
    // Glass card row
    lv_obj_t *row = lv_obj_create(device_list_container);
    lv_obj_set_size(row, UI_CONTENT_W - 24, 54);
    ui_style_surface(row, 14);
    // Zero the padding explicitly. Every child below is placed with absolute
    // offsets from the row's edges, and LEFT/RIGHT alignment both measure from
    // the *content* box — so a theme-supplied pad of P shrinks the gap between
    // the room chip and the Edit pill by 2P and silently overlaps them.
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Icon in amber circle
    lv_obj_t *ico_bg = lv_obj_create(row);
    lv_obj_set_size(ico_bg, 36, 36);
    lv_obj_align(ico_bg, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(ico_bg, lv_color_hex(CLR_HEX_ACCENT_TINT), 0);
    lv_obj_set_style_bg_opa(ico_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ico_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ico_bg, 0, 0);
    lv_obj_set_style_shadow_width(ico_bg, 0, 0);
    lv_obj_set_style_pad_all(ico_bg, 0, 0);
    lv_obj_clear_flag(ico_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ico = lv_label_create(ico_bg);
    lv_label_set_text(ico, getIconSymbol(devices[i].icon_type));
    lv_obj_set_style_text_font(ico, &material_icons_font, 0);
    lv_obj_set_style_text_color(ico, CLR_PRIMARY, 0);
    lv_obj_center(ico);

    // Name — tap to rename in place
    lv_obj_t *nm = lv_label_create(row);
    lv_label_set_text(nm, devices[i].name);
    lv_obj_set_style_text_color(nm, lv_color_hex(CLR_HEX_TEXT_HI), 0);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
    lv_obj_set_width(nm, 142);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, 52, 0);
    // The label itself is the hit target, so give it some height to aim at.
    lv_obj_set_height(nm, 30);
    lv_obj_set_style_pad_top(nm, 7, 0);
    lv_obj_add_flag(nm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(nm, name_tap_cb, LV_EVENT_CLICKED,
                        (void *)(ptrdiff_t)i);

    // Room chip — tap cycles to the next room.
    // Row is UI_CONTENT_W − 24 = 404 wide with padding now zeroed, so these
    // offsets are exact: name 52..194, chip 200..284, Edit 298..346,
    // Delete 350..398. The 14 px before Edit is deliberate slack — this row is
    // the tightest in the UI and the last version overlapped by 4 px once the
    // theme's own padding was counted twice.
    lv_obj_t *chip = ui_create_chip(row, 84, 26);
    lv_obj_align(chip, LV_ALIGN_LEFT_MID, 200, 0);
    if (roomCount >= 2) {
      lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_border_color(chip, lv_color_hex(CLR_HEX_ACCENT),
                                    LV_STATE_PRESSED);
      lv_obj_add_event_cb(chip, room_chip_cb, LV_EVENT_CLICKED,
                          (void *)(ptrdiff_t)i);
    }
    lv_obj_t *chip_lbl = lv_label_create(chip);
    lv_label_set_text(chip_lbl, devices[i].room[0] ? devices[i].room : "--");
    lv_obj_set_style_text_font(chip_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chip_lbl, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_label_set_long_mode(chip_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(chip_lbl, 72);
    lv_obj_set_style_text_align(chip_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(chip_lbl);

    // Edit — dark pill
    lv_obj_t *btn_edit = ui_create_pill_btn(row, 48, UI_PILL_BTN_H,
                                            LV_SYMBOL_EDIT, lv_color_hex(CLR_HEX_TEXT_MID),
                                            btn_edit_device_cb,
                                            (void *)(ptrdiff_t)i,
                                            LV_EVENT_CLICKED);
    lv_obj_align(btn_edit, LV_ALIGN_RIGHT_MID, -58, 0);

    // Delete — same pill with the danger color on border and glyph
    lv_obj_t *btn_del = ui_create_pill_btn(row, 48, UI_PILL_BTN_H,
                                           LV_SYMBOL_TRASH,
                                           lv_color_hex(CLR_HEX_DANGER),
                                           btn_delete_device_cb,
                                           (void *)(ptrdiff_t)i,
                                           LV_EVENT_CLICKED);
    lv_obj_align(btn_del, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_border_color(btn_del, lv_color_hex(CLR_HEX_DANGER), 0);
  }
}

void build_edit_device_screen() {
  if (ui_ScreenEditDevice)
    lv_obj_del(ui_ScreenEditDevice);
  ui_ScreenEditDevice = lv_obj_create(NULL);
  // Rail first, so the header and body that follow stack above it.
  ui_nav_rail_create(ui_ScreenEditDevice, UI_NAV_SETTINGS);

  // Deep black background
  lv_obj_set_style_bg_color(ui_ScreenEditDevice, CLR_BG_DEEP, 0);

  // ── Frosted glass header ────────────────────
  lv_obj_t *hdr = ui_create_frosted_header(ui_ScreenEditDevice, UI_SETTINGS_HDR_HEIGHT);

  // Cancel — pill
  lv_obj_t *btn_cancel = ui_create_pill_btn(hdr, UI_PILL_BTN_W, UI_PILL_BTN_H,
                                            LV_SYMBOL_LEFT, CLR_PRIMARY,
                                            btn_cancel_edit_cb);
  lv_obj_align(btn_cancel, LV_ALIGN_LEFT_MID, 8, 0);

  // Title
  lv_obj_t *lt = lv_label_create(hdr);
  lv_label_set_text(lt, editDeviceIndex >= 0 ? L(L_EDIT_DEVICE) : L(L_NEW_DEVICE));
  lv_obj_set_style_text_color(lt, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_obj_set_style_text_font(lt, &lv_font_montserrat_18, 0);
  lv_obj_align(lt, LV_ALIGN_LEFT_MID, 62, 0);

  // Save — amber accent
  lv_obj_t *btn_save = ui_create_accent_btn(hdr, 90, UI_PILL_BTN_H, "",
                                            btn_save_device_cb);
  lv_obj_align(btn_save, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_label_set_text_fmt(lv_obj_get_child(btn_save, 0), LV_SYMBOL_OK " %s",
                        L(L_SAVE));

  // ── Scrollable form ─────────────────────────
  lv_obj_t *form = lv_obj_create(ui_ScreenEditDevice);
  lv_obj_set_size(form, UI_CONTENT_W, SCREEN_HEIGHT - 52);
  lv_obj_align(form, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(form, 14, 0);
  lv_obj_set_style_pad_row(form, 4, 0);
  lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(form, 0, 0);

  // Keyboard
  kb_edit = lv_keyboard_create(ui_ScreenEditDevice);
  ui_style_keyboard(kb_edit);
  lv_obj_add_flag(kb_edit, LV_OBJ_FLAG_HIDDEN);

  // ── Form fields — styled labels + dark textareas ──

  // Device Name
  lv_obj_t *l1 = lv_label_create(form);
  lv_label_set_text(l1, L(L_DEVICE_NAME));
  lv_obj_set_style_text_color(l1, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(l1, &lv_font_montserrat_12, 0);
  ta_dev_name = ui_create_form_textarea(form, 40, L(L_DEVICE_NAME_HINT),
                                     ta_event_cb, (void *)kb_edit);

  // State Topic
  lv_obj_t *l2 = lv_label_create(form);
  lv_label_set_text(l2, L(L_STATE_TOPIC));
  lv_obj_set_style_text_color(l2, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(l2, &lv_font_montserrat_12, 0);
  ta_dev_stat = ui_create_form_textarea(form, 40, "homebridge/name/stat",
                                     ta_event_cb, (void *)kb_edit);

  // Command Topic
  lv_obj_t *l3 = lv_label_create(form);
  lv_label_set_text(l3, L(L_CMD_TOPIC));
  lv_obj_set_style_text_color(l3, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(l3, &lv_font_montserrat_12, 0);
  ta_dev_cmnd = ui_create_form_textarea(form, 40, "homebridge/name/set",
                                     ta_event_cb, (void *)kb_edit);

  // Dimmer Topic
  lv_obj_t *l3b = lv_label_create(form);
  lv_label_set_text(l3b, L(L_DIMMER_TOPIC));
  lv_obj_set_style_text_color(l3b, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(l3b, &lv_font_montserrat_12, 0);
  ta_dev_dimmer = ui_create_form_textarea(form, 40, "homebridge/name/dimmer",
                                     ta_event_cb, (void *)kb_edit);

  // Icon Type
  lv_obj_t *l4 = lv_label_create(form);
  lv_label_set_text(l4, L(L_ICON_TYPE));
  lv_obj_set_style_text_color(l4, lv_color_hex(CLR_HEX_TEXT_MID), 0);
  lv_obj_set_style_text_font(l4, &lv_font_montserrat_12, 0);
  dd_icon = lv_dropdown_create(form);
  lv_dropdown_set_options(dd_icon, icon_names);
  lv_obj_set_width(dd_icon, lv_pct(100));
  lv_obj_set_height(dd_icon, 40);
  ui_style_dropdown(dd_icon);

  // Pre-fill
  if (editDeviceIndex >= 0 && editDeviceIndex < deviceCount) {
    lv_textarea_set_text(ta_dev_name, devices[editDeviceIndex].name);
    lv_textarea_set_text(ta_dev_stat, devices[editDeviceIndex].state_topic);
    lv_textarea_set_text(ta_dev_cmnd, devices[editDeviceIndex].cmnd_topic);
    lv_textarea_set_text(ta_dev_dimmer, devices[editDeviceIndex].dimmer_topic);
    lv_dropdown_set_selected(dd_icon, devices[editDeviceIndex].icon_type);
  } else {
    lv_textarea_set_text(ta_dev_name, "");
    lv_textarea_set_text(ta_dev_stat, "homebridge//stat");
    lv_textarea_set_text(ta_dev_cmnd, "homebridge//set");
    lv_textarea_set_text(ta_dev_dimmer, "");
    lv_dropdown_set_selected(dd_icon, ICON_SWITCH);
  }
}
