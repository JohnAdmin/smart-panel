#include "../../include/schedule.h"
#include "../../include/scene.h"
#include "../lang.h"
#include "ui_helpers.h"
#include "ui_screens.h"
#include <stdio.h>
#include <string.h>

// Schedules used to own a full screen reached from its own rail slot. They are
// now the second tab of the Scenes destination and render into whatever
// container the caller hands over — scenes and schedules answer the same
// question ("what runs, and when"), and the split cost a rail slot for a list
// most panels open rarely.

static const char *get_day_label(int d) {
  static const LangKey day_keys[] = {L_DAY_SU, L_DAY_MO, L_DAY_TU,
                                     L_DAY_WE, L_DAY_TH, L_DAY_FR, L_DAY_SA};
  return L(day_keys[d]);
}

static void toggle_schedule_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= scheduleCount) return;
  schedules[idx].enabled =
      lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  saveSchedules();
}

// Deleting a schedule can't be undone, and the row it lives on also carries a
// switch — so a mis-tap is easy. Confirm first, the same as device deletion
// and the room-wide off sweep.
static int s_pending_delete = -1;

// Re-rendering tears down main_body_container, and the rows being destroyed are
// siblings of the widget currently dispatching this event — so it has to happen
// after LVGL finishes the dispatch, not inside it.
static void rerender_schedule_async(void *unused) {
  (void)unused;
  ui_show_main_view(UI_VIEW_SCHEDULE);
}

static void delete_msgbox_cb(lv_event_t *e) {
  lv_obj_t *mbox = lv_event_get_current_target(e);
  const bool yes = (lv_msgbox_get_active_btn(mbox) == 0);
  if (yes && s_pending_delete >= 0) {
    deleteSchedule(s_pending_delete);
    lv_async_call(rerender_schedule_async, NULL);
  }
  s_pending_delete = -1;
  lv_msgbox_close(mbox);
}

static void delete_schedule_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= scheduleCount) return;
  s_pending_delete = idx;

  // The button map must outlive this call — lv_msgbox keeps the pointer.
  static const char *btns[3];
  btns[0] = L(L_YES);
  btns[1] = L(L_NO);
  btns[2] = "";

  lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), L(L_CONFIRM_DELETE),
                                    L(L_CONFIRM_DELETE_SCHED), btns, false);
  ui_style_msgbox(mbox);
  lv_obj_center(mbox);
  lv_obj_add_event_cb(mbox, delete_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void build_schedule_view(lv_obj_t *parent) {
  lv_obj_t *list = lv_obj_create(parent);
  lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_set_style_pad_left(list, UI_CONTENT_PAD, 0);
  lv_obj_set_style_pad_right(list, UI_CONTENT_PAD, 0);
  lv_obj_set_style_pad_bottom(list, 8, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  ui_style_scrollbar(list);

  if (scheduleCount == 0) {
    lv_obj_t *empty = lv_label_create(list);
    lv_label_set_text(empty, L(L_NO_SCHEDULES));
    lv_obj_set_style_text_color(empty, lv_color_hex(CLR_HEX_TEXT_MID), 0);
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(empty, LV_PCT(100));
    lv_obj_set_style_pad_top(empty, 50, 0);
    return;
  }

  for (int i = 0; i < scheduleCount; i++) {
    Schedule &sc = schedules[i];

    // Rows are LV_PCT(100) rather than a fixed width: the old screen hard-coded
    // 460 px, which overflowed once the nav rail took the content area down to
    // 428.
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(row, lv_color_hex(CLR_HEX_SURFACE_1), 0);
    lv_obj_set_style_bg_grad_dir(row, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_90, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_opa(row, LV_OPA_COVER, 0);
    // An enabled schedule carries the accent on its outline
    lv_obj_set_style_border_color(row, sc.enabled
                                           ? lv_color_hex(CLR_HEX_ACCENT)
                                           : lv_color_hex(CLR_HEX_HAIRLINE), 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Time — the thing you scan the list for, so it leads and stays largest
    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", sc.hour, sc.minute);
    lv_obj_t *time_lbl = lv_label_create(row);
    lv_label_set_text(time_lbl, time_buf);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(time_lbl,
                                sc.enabled ? lv_color_hex(CLR_HEX_ACCENT_HI)
                                           : lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_align(time_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    // Scene name
    const char *scene_name =
        (sc.scene_index >= 0 && sc.scene_index < sceneCount)
            ? scenes[sc.scene_index].name
            : L(L_UNKNOWN);
    lv_obj_t *scene_lbl = lv_label_create(row);
    lv_label_set_text(scene_lbl, scene_name);
    lv_obj_set_style_text_font(scene_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(scene_lbl, lv_color_hex(CLR_HEX_TEXT_HI), 0);
    lv_label_set_long_mode(scene_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(scene_lbl, 150);
    lv_obj_align(scene_lbl, LV_ALIGN_LEFT_MID, 56, -7);

    // Days
    char days_buf[32] = "";
    if (sc.days == DAY_ALL) {
      strncpy(days_buf, L(L_EVERY_DAY), sizeof(days_buf) - 1);
    } else {
      for (int d = 0; d < 7; d++) {
        if (!(sc.days & (1 << d))) continue;
        if (days_buf[0]) strncat(days_buf, " ",
                                 sizeof(days_buf) - strlen(days_buf) - 1);
        strncat(days_buf, get_day_label(d),
                sizeof(days_buf) - strlen(days_buf) - 1);
      }
    }
    lv_obj_t *days_lbl = lv_label_create(row);
    lv_label_set_text(days_lbl, days_buf);
    lv_obj_set_style_text_font(days_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(days_lbl, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_label_set_long_mode(days_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(days_lbl, 150);
    lv_obj_align(days_lbl, LV_ALIGN_LEFT_MID, 56, 8);

    // Delete — outlined, never a filled danger button
    lv_obj_t *btn_del = lv_btn_create(row);
    lv_obj_set_size(btn_del, 26, 24);
    lv_obj_align(btn_del, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_del, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn_del, lv_color_hex(CLR_HEX_HAIRLINE), 0);
    lv_obj_set_style_border_width(btn_del, 1, 0);
    lv_obj_set_style_radius(btn_del, 7, 0);
    lv_obj_set_style_shadow_width(btn_del, 0, 0);
    lv_obj_set_style_bg_color(btn_del, lv_color_hex(CLR_HEX_DANGER),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn_del, LV_OPA_40, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_del, delete_schedule_cb, LV_EVENT_CLICKED,
                        (void *)(ptrdiff_t)i);
    lv_obj_t *lbl_del = lv_label_create(btn_del);
    lv_label_set_text(lbl_del, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(lbl_del, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_del, lv_color_hex(CLR_HEX_DANGER_HI), 0);
    lv_obj_center(lbl_del);

    // Enable / disable
    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 38, 21);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -34, 0);
    ui_style_switch(sw);
    if (sc.enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, toggle_schedule_cb, LV_EVENT_VALUE_CHANGED,
                        (void *)(ptrdiff_t)i);
  }
}
