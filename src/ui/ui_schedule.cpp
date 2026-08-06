#include "../../include/schedule.h"
#include "../../include/scene.h"
#include "../lang.h"
#include "ui_helpers.h"
#include "ui_screens.h"
#include <stdio.h>
#include <string.h>

// Screen pointer (declared in ui_screens.h)
lv_obj_t *ui_ScreenSchedules = NULL;

// Persistent widgets
static lv_obj_t *sched_list_container = NULL;

static const char *day_labels[] = {nullptr}; // set at runtime via L()

static const char *get_day_label(int d) {
  static const LangKey day_keys[] = {L_DAY_SU, L_DAY_MO, L_DAY_TU, L_DAY_WE, L_DAY_TH, L_DAY_FR, L_DAY_SA};
  return L(day_keys[d]);
}

void cleanup_schedule_screen() {
  if (ui_ScreenSchedules) {
    lv_anim_del(ui_ScreenSchedules, NULL);
    lv_obj_del(ui_ScreenSchedules);
    ui_ScreenSchedules = NULL;
    sched_list_container = NULL;
  }
}

static void btn_back_from_schedules(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    sched_list_container = NULL;
    lv_scr_load_anim(ui_ScreenSettings, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  }
}

static void toggle_schedule_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= scheduleCount) return;
  schedules[idx].enabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  saveSchedules();
}

void build_schedule_list_screen() {
  // Free other sub-screens first to keep LVGL heap healthy
  cleanup_scene_screen();
  cleanup_device_screen();
  // Free screensaver if lingering
  if (ui_ScreenSaver) { lv_anim_del(ui_ScreenSaver, NULL); lv_obj_del(ui_ScreenSaver); ui_ScreenSaver = NULL; }

  // ── If screen already exists, just refresh the list ──
  if (ui_ScreenSchedules && sched_list_container) {
    lv_obj_clean(sched_list_container);
    goto populate;
  }

  // Delete stale screen (back was pressed, container cleared)
  if (ui_ScreenSchedules) {
    lv_anim_del(ui_ScreenSchedules, NULL);
    lv_obj_del(ui_ScreenSchedules);
    ui_ScreenSchedules = NULL;
  }

  // ── First time: create screen + header (persists) ──
  ui_ScreenSchedules = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ui_ScreenSchedules, CLR_BG_DEEP, 0);

  {
  // ── Header ──
  lv_obj_t *header = lv_obj_create(ui_ScreenSchedules);
  lv_obj_set_size(header, 480, 50);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(CLR_HEX_CARD_BG), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_90, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_shadow_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *btn_back = lv_btn_create(header);
  lv_obj_set_size(btn_back, 80, 36);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_bg_color(btn_back, lv_color_hex(CLR_HEX_PILL_BG), 0);
  lv_obj_set_style_radius(btn_back, 12, 0);
  lv_obj_set_style_shadow_width(btn_back, 0, 0);
  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text_fmt(lbl_back, LV_SYMBOL_LEFT " %s", L(L_BACK));
  lv_obj_set_style_text_color(lbl_back, CLR_PRIMARY, 0);
  lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_12, 0);
  lv_obj_center(lbl_back);
  lv_obj_add_event_cb(btn_back, btn_back_from_schedules, LV_EVENT_CLICKED, NULL);

  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text_fmt(title, LV_SYMBOL_LOOP " %s", L(L_SCHEDULES));
  lv_obj_set_style_text_color(title, CLR_TEXT_TITLE, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

  // ── Body ──
  sched_list_container = lv_obj_create(ui_ScreenSchedules);
  lv_obj_set_size(sched_list_container, 480, 270);
  lv_obj_align(sched_list_container, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(sched_list_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(sched_list_container, 0, 0);
  lv_obj_set_style_pad_all(sched_list_container, 8, 0);
  lv_obj_set_flex_flow(sched_list_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(sched_list_container, 8, 0);
  lv_obj_set_scrollbar_mode(sched_list_container, LV_SCROLLBAR_MODE_AUTO);
  } // end header block

populate:
  if (scheduleCount == 0) {
    lv_obj_t *empty = lv_label_create(sched_list_container);
    lv_label_set_text(empty, L(L_NO_SCHEDULES));
    lv_obj_set_style_text_color(empty, CLR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(empty, 460);
    lv_obj_set_style_pad_top(empty, 40, 0);
  }

  for (int i = 0; i < scheduleCount; i++) {
    Schedule &sc = schedules[i];

    lv_obj_t *card = lv_obj_create(sched_list_container);
    lv_obj_set_size(card, 460, 64);
    ui_style_surface(card, 14);
    // Enabled schedules carry the accent on their outline
    lv_obj_set_style_border_color(card, sc.enabled ? lv_color_hex(CLR_HEX_ACCENT)
                                                   : lv_color_hex(CLR_HEX_HAIRLINE), 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Time
    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", sc.hour, sc.minute);
    lv_obj_t *time_lbl = lv_label_create(card);
    lv_label_set_text(time_lbl, time_buf);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(time_lbl, sc.enabled ? lv_color_hex(CLR_HEX_ACCENT_HI)
                                                 : lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_align(time_lbl, LV_ALIGN_LEFT_MID, 0, -8);

    // Scene name
    const char *scene_name = (sc.scene_index >= 0 && sc.scene_index < sceneCount)
                                 ? scenes[sc.scene_index].name : L(L_UNKNOWN);
    lv_obj_t *scene_lbl = lv_label_create(card);
    lv_label_set_text(scene_lbl, scene_name);
    lv_obj_set_style_text_font(scene_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(scene_lbl, CLR_TEXT_DIM, 0);
    lv_obj_align(scene_lbl, LV_ALIGN_LEFT_MID, 0, 14);

    // Days text
    char days_buf[24] = "";
    for (int d = 0; d < 7; d++) {
      if (sc.days & (1 << d)) {
        if (strlen(days_buf) > 0) strcat(days_buf, " ");
        strcat(days_buf, get_day_label(d));
      }
    }
    if (sc.days == 0x7F) strcpy(days_buf, L(L_EVERY_DAY));

    lv_obj_t *days_lbl = lv_label_create(card);
    lv_label_set_text(days_lbl, days_buf);
    lv_obj_set_style_text_font(days_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(days_lbl, CLR_TEXT_DIM, 0);
    lv_obj_align(days_lbl, LV_ALIGN_CENTER, 20, -8);

    // Enable/Disable switch
    lv_obj_t *sw = lv_switch_create(card);
    lv_obj_set_size(sw, 44, 24);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    ui_style_switch(sw);
    if (sc.enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, toggle_schedule_cb, LV_EVENT_VALUE_CHANGED, (void*)(ptrdiff_t)i);
  }

  lv_scr_load_anim(ui_ScreenSchedules, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}
