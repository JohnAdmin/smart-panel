#include "../../include/scene.h"
#include "../mqtt_manager.h"
#include "../wifi_manager.h"
#include "../lang.h"
#include "ui_helpers.h"
#include "ui_screens.h"
#include <esp_task_wdt.h>
#include <string.h>

lv_obj_t *ui_ScreenScenes = NULL;
lv_obj_t *ui_ScreenEditScene = NULL;

// Edit state
static int editSceneIndex = -1;

// Edit screen widgets
static lv_obj_t *ta_scene_name = NULL;
static lv_obj_t *dd_scene_icon = NULL;
static lv_obj_t *ta_topics[MAX_SCENE_ACTIONS];
static lv_obj_t *ta_payloads[MAX_SCENE_ACTIONS];
static int visible_action_count = 3; // start with 3 action rows visible
static lv_obj_t *kb_scene = NULL;

// Persistent scene list widgets (avoid recreating screen)
static lv_obj_t *scene_list_container = NULL;
static lv_obj_t *scene_count_lbl = NULL;

// ── Callbacks ──────────────────────────────────────────

static void _del_old_screen(void *obj) { if (obj) lv_obj_del((lv_obj_t *)obj); }

void cleanup_scene_screen() {
  if (ui_ScreenEditScene) {
    lv_anim_del(ui_ScreenEditScene, NULL);
    lv_obj_del(ui_ScreenEditScene);
    ui_ScreenEditScene = NULL;
  }
  if (ui_ScreenScenes) {
    lv_anim_del(ui_ScreenScenes, NULL);
    lv_obj_del(ui_ScreenScenes);
    ui_ScreenScenes = NULL;
    scene_list_container = NULL;
    scene_count_lbl = NULL;
  }
}

static void btn_back_to_settings_from_scenes_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    scene_list_container = NULL;
    scene_count_lbl = NULL;
    lv_scr_load_anim(ui_ScreenSettings, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  }
}

static void btn_add_scene_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    editSceneIndex = -1;
    build_edit_scene_screen(-1);
    lv_scr_load_anim(ui_ScreenEditScene, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0,
                     false);
  }
}

static void btn_edit_scene_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    editSceneIndex = (int)(ptrdiff_t)lv_event_get_user_data(e);
    build_edit_scene_screen(editSceneIndex);
    lv_scr_load_anim(ui_ScreenEditScene, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0,
                     false);
  }
}

static void _async_refresh_scenes(void *) { build_scene_list_screen(); }

static void btn_delete_scene_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);
    deleteScene(idx);
    // Defer refresh — button being clicked will be deleted by lv_obj_clean
    lv_async_call(_async_refresh_scenes, NULL);
  }
}

static void btn_cancel_scene_edit_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *old = ui_ScreenEditScene;
    ui_ScreenEditScene = NULL;
    lv_scr_load_anim(ui_ScreenScenes, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    lv_async_call(_del_old_screen, old);
  }
}

static void btn_save_scene_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;

  const char *name = lv_textarea_get_text(ta_scene_name);
  int icon = lv_dropdown_get_selected(dd_scene_icon);

  // Color presets per icon type
  static const uint32_t icon_colors[] = {
      0xFBBF24, // Morning — warm yellow
      0x6366F1, // Night — indigo
      0x22C55E, // Leave — green
      0xEF4444, // Movie — red
      0xEC4899, // Party — pink
      0xF59E0B, // Custom — amber
  };
  uint32_t color = (icon < 6) ? icon_colors[icon] : 0xF59E0B;

  Scene *sc;
  if (editSceneIndex >= 0 && editSceneIndex < sceneCount) {
    sc = &scenes[editSceneIndex];
  } else {
    if (sceneCount >= MAX_SCENES)
      return;
    sc = &scenes[sceneCount];
    sceneCount++;
  }

  strlcpy(sc->name, name, sizeof(sc->name));
  sc->icon_index = icon;
  sc->color = color;
  sc->action_count = 0;

  for (int i = 0; i < MAX_SCENE_ACTIONS; i++) {
    if (ta_topics[i] == NULL)
      break;
    const char *topic = lv_textarea_get_text(ta_topics[i]);
    const char *payload = lv_textarea_get_text(ta_payloads[i]);
    if (strlen(topic) > 0) {
      strlcpy(sc->actions[sc->action_count].topic, topic,
              sizeof(sc->actions[0].topic));
      strlcpy(sc->actions[sc->action_count].payload, payload,
              sizeof(sc->actions[0].payload));
      sc->action_count++;
    }
  }

  saveScenes();
  build_scene_list_screen();
  lv_obj_t *old_edit = ui_ScreenEditScene;
  ui_ScreenEditScene = NULL;
  lv_scr_load_anim(ui_ScreenScenes, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  lv_async_call(_del_old_screen, old_edit);
}

static void ta_scene_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb_scene, ta);
    lv_obj_clear_flag(kb_scene, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb_scene);
    lv_obj_scroll_to_view(ta, LV_ANIM_ON);
  }
  if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY ||
      code == LV_EVENT_CANCEL) {
    lv_keyboard_set_textarea(kb_scene, NULL);
    lv_obj_add_flag(kb_scene, LV_OBJ_FLAG_HIDDEN);
  }
}

static void btn_add_action_cb(lv_event_t *e);

// ╔═══════════════════════════════════════════╗
// ║       SCENE LIST SCREEN                   ║
// ╚═══════════════════════════════════════════╝
void build_scene_list_screen() {
  // Free other sub-screens first to keep LVGL heap healthy
  cleanup_schedule_screen();
  cleanup_device_screen();
  // Free screensaver if lingering
  if (ui_ScreenSaver) { lv_anim_del(ui_ScreenSaver, NULL); lv_obj_del(ui_ScreenSaver); ui_ScreenSaver = NULL; }

  // ── If screen already exists, just refresh the list content ──
  if (ui_ScreenScenes && scene_list_container) {
    lv_obj_clean(scene_list_container);
    // Update count badge
    if (scene_count_lbl) {
      char cnt[8];
      snprintf(cnt, sizeof(cnt), "%d", sceneCount);
      lv_label_set_text(scene_count_lbl, cnt);
    }
    goto populate;
  }

  // Delete stale screen (back was pressed, container cleared)
  if (ui_ScreenScenes) {
    lv_anim_del(ui_ScreenScenes, NULL);
    lv_obj_del(ui_ScreenScenes);
    ui_ScreenScenes = NULL;
    scene_count_lbl = NULL;
  }

  // ── First time: create screen structure (header persists) ──
  ui_ScreenScenes = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ui_ScreenScenes, CLR_BG_DEEP, 0);

  {
  // ── Frosted glass header ────────────────────
  lv_obj_t *hdr = ui_create_frosted_header(ui_ScreenScenes, UI_SETTINGS_HDR_HEIGHT);

  // Back
  lv_obj_t *btn_back = ui_create_pill_btn(hdr, UI_PILL_BTN_W, UI_PILL_BTN_H,
                                          LV_SYMBOL_LEFT, CLR_PRIMARY,
                                          btn_back_to_settings_from_scenes_cb,
                                          NULL, LV_EVENT_CLICKED);
  lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 8, 0);

  // Title
  lv_obj_t *lbl_title = lv_label_create(hdr);
  lv_label_set_text(lbl_title, L(L_SCENES));
  lv_obj_set_style_text_color(lbl_title, CLR_TEXT_TITLE, 0);
  lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
  lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 62, 0);

  // Count badge
  lv_obj_t *badge = lv_obj_create(hdr);
  lv_obj_set_size(badge, 28, 22);
  lv_obj_align(badge, LV_ALIGN_LEFT_MID, 130, 0);
  lv_obj_set_style_bg_color(badge, lv_color_hex(CLR_HEX_BADGE_BG), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(badge, 0, 0);
  lv_obj_set_style_shadow_width(badge, 0, 0);
  lv_obj_set_style_pad_all(badge, 0, 0);
  lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  scene_count_lbl = lv_label_create(badge);
  lv_obj_set_style_text_color(scene_count_lbl, CLR_TEXT_DIM, 0);
  lv_obj_set_style_text_font(scene_count_lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(scene_count_lbl);

  // Add button
  lv_obj_t *btn_add = ui_create_accent_btn(hdr, 80, UI_PILL_BTN_H, "",
                                           btn_add_scene_cb, NULL,
                                           LV_EVENT_CLICKED);
  lv_obj_align(btn_add, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_label_set_text_fmt(lv_obj_get_child(btn_add, 0), LV_SYMBOL_PLUS " %s",
                        L(L_ADD));

  // ── Scrollable list ─────────────────────────
  scene_list_container = lv_obj_create(ui_ScreenScenes);
  lv_obj_set_size(scene_list_container, SCREEN_WIDTH, SCREEN_HEIGHT - 52);
  lv_obj_align(scene_list_container, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_flex_flow(scene_list_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scene_list_container, 10, 0);
  lv_obj_set_style_pad_row(scene_list_container, 6, 0);
  lv_obj_set_style_bg_opa(scene_list_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(scene_list_container, 0, 0);
  lv_obj_set_scrollbar_mode(scene_list_container, LV_SCROLLBAR_MODE_AUTO);

  // Set initial count
  {
    char cnt[8];
    snprintf(cnt, sizeof(cnt), "%d", sceneCount);
    lv_label_set_text(scene_count_lbl, cnt);
  }
  } // end header block

populate:
  if (sceneCount == 0) {
    lv_obj_t *empty = lv_label_create(scene_list_container);
    lv_label_set_text(empty, L(L_NO_SCENES));
    lv_obj_set_style_text_color(empty, CLR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(empty, LV_ALIGN_CENTER, 0, 40);
    return;
  }

  for (int i = 0; i < sceneCount; i++) {
    lv_color_t accent = lv_color_hex(scenes[i].color);

    // Glass card row
    lv_obj_t *row = lv_obj_create(scene_list_container);
    lv_obj_set_size(row, SCREEN_WIDTH - 24, 60);
    ui_style_surface(row, 14);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Color accent dot
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_set_size(dot, 36, 36);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(dot, accent, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_30, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_shadow_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ico = lv_label_create(dot);
    lv_label_set_text(ico, getSceneIconSymbol(scenes[i].icon_index));
    lv_obj_set_style_text_color(ico, accent, 0);
    lv_obj_center(ico);

    // Name + action count
    lv_obj_t *nm = lv_label_create(row);
    lv_label_set_text(nm, scenes[i].name);
    lv_obj_set_style_text_color(nm, CLR_TEXT_TITLE, 0);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, 52, -8);

    char info[24];
    snprintf(info, sizeof(info), "%d %s", scenes[i].action_count, L(L_ACTIONS));
    lv_obj_t *sub = lv_label_create(row);
    lv_label_set_text(sub, info);
    lv_obj_set_style_text_color(sub, CLR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
    lv_obj_align(sub, LV_ALIGN_LEFT_MID, 52, 10);

    // Edit
    lv_obj_t *btn_edit = ui_create_pill_btn(row, 48, UI_PILL_BTN_H,
                                            LV_SYMBOL_EDIT, CLR_TEXT_DIM,
                                            btn_edit_scene_cb,
                                            (void *)(ptrdiff_t)i,
                                            LV_EVENT_CLICKED);
    lv_obj_align(btn_edit, LV_ALIGN_RIGHT_MID, -58, 0);

    // Delete
    lv_obj_t *btn_del = ui_create_pill_btn(row, 48, UI_PILL_BTN_H,
                                           LV_SYMBOL_TRASH,
                                           lv_color_hex(CLR_HEX_DANGER),
                                           btn_delete_scene_cb,
                                           (void *)(ptrdiff_t)i,
                                           LV_EVENT_CLICKED);
    lv_obj_align(btn_del, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_border_color(btn_del, lv_color_hex(CLR_HEX_DANGER), 0);
  }
}

// ╔═══════════════════════════════════════════╗
// ║       EDIT SCENE SCREEN                   ║
// ╚═══════════════════════════════════════════╝
void build_edit_scene_screen(int index) {
  if (ui_ScreenEditScene)
    lv_obj_del(ui_ScreenEditScene);
  ui_ScreenEditScene = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ui_ScreenEditScene, CLR_BG_DEEP, 0);

  editSceneIndex = index;

  // Determine action count for display
  if (index >= 0 && index < sceneCount) {
    visible_action_count =
        scenes[index].action_count > 0 ? scenes[index].action_count : 3;
  } else {
    visible_action_count = 3;
  }
  if (visible_action_count > MAX_SCENE_ACTIONS)
    visible_action_count = MAX_SCENE_ACTIONS;

  // ── Frosted header ──────────────────────────
  lv_obj_t *hdr = ui_create_frosted_header(ui_ScreenEditScene, UI_SETTINGS_HDR_HEIGHT);

  // Cancel
  lv_obj_t *btn_cancel = ui_create_pill_btn(hdr, UI_PILL_BTN_W, UI_PILL_BTN_H,
                                            LV_SYMBOL_LEFT, CLR_PRIMARY,
                                            btn_cancel_scene_edit_cb);
  lv_obj_align(btn_cancel, LV_ALIGN_LEFT_MID, 8, 0);

  // Title
  lv_obj_t *lt = lv_label_create(hdr);
  lv_label_set_text(lt, index >= 0 ? L(L_EDIT_SCENE) : L(L_NEW_SCENE));
  lv_obj_set_style_text_color(lt, CLR_TEXT_TITLE, 0);
  lv_obj_set_style_text_font(lt, &lv_font_montserrat_18, 0);
  lv_obj_align(lt, LV_ALIGN_LEFT_MID, 62, 0);

  // Save
  lv_obj_t *btn_save = ui_create_accent_btn(hdr, 90, UI_PILL_BTN_H, "",
                                            btn_save_scene_cb);
  lv_obj_align(btn_save, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_label_set_text_fmt(lv_obj_get_child(btn_save, 0), LV_SYMBOL_OK " %s",
                        L(L_SAVE));

  // ── Scrollable form ─────────────────────────
  lv_obj_t *form = lv_obj_create(ui_ScreenEditScene);
  lv_obj_set_size(form, SCREEN_WIDTH, SCREEN_HEIGHT - 52);
  lv_obj_align(form, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(form, 14, 0);
  lv_obj_set_style_pad_row(form, 3, 0);
  lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(form, 0, 0);

  // Keyboard
  kb_scene = lv_keyboard_create(ui_ScreenEditScene);
  ui_style_keyboard(kb_scene);
  lv_obj_add_flag(kb_scene, LV_OBJ_FLAG_HIDDEN);

  auto mk_label = [&](lv_obj_t *parent, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, CLR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  };

  // ── Scene Name ──
  mk_label(form, L(L_SCENE_NAME));
  ta_scene_name = ui_create_form_textarea(form, 36, L(L_SCENE_NAME_HINT),
                                          ta_scene_event_cb);

  // ── Icon ──
  mk_label(form, L(L_SCENE_ICON));
  dd_scene_icon = lv_dropdown_create(form);
  lv_dropdown_set_options(dd_scene_icon, L(L_SCENE_ICONS));
  lv_obj_set_width(dd_scene_icon, lv_pct(100));
  lv_obj_set_height(dd_scene_icon, 36);
  ui_style_dropdown(dd_scene_icon);

  // ── Action rows ──
  mk_label(form, L(L_SCENE_ACTIONS));

  // Reset pointers
  memset(ta_topics, 0, sizeof(ta_topics));
  memset(ta_payloads, 0, sizeof(ta_payloads));

  for (int i = 0; i < visible_action_count; i++) {
    // Row container — horizontal layout
    lv_obj_t *row = lv_obj_create(form);
    lv_obj_set_size(row, lv_pct(100), 38);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Topic and payload share a row, so they override the factory's full width
    char ph[24];
    snprintf(ph, sizeof(ph), "%s %d", L(L_TOPIC), i + 1);
    ta_topics[i] = ui_create_form_textarea(row, 36, ph, ta_scene_event_cb);
    lv_obj_set_width(ta_topics[i], lv_pct(58));
    lv_obj_align(ta_topics[i], LV_ALIGN_LEFT_MID, 0, 0);

    ta_payloads[i] =
        ui_create_form_textarea(row, 36, L(L_PAYLOAD), ta_scene_event_cb);
    lv_obj_set_width(ta_payloads[i], lv_pct(38));
    lv_obj_align(ta_payloads[i], LV_ALIGN_RIGHT_MID, 0, 0);
  }

  // + Add Action button
  if (visible_action_count < MAX_SCENE_ACTIONS) {
    lv_obj_t *btn_more = ui_create_pill_btn(form, 120, 32, "", CLR_TEXT_DIM,
                                            btn_add_action_cb);
    lv_obj_t *lm = lv_obj_get_child(btn_more, 0);
    lv_label_set_text_fmt(lm, LV_SYMBOL_PLUS " %s", L(L_ACTION));
    lv_obj_set_style_text_font(lm, &lv_font_montserrat_12, 0);
  }

  // ── Pre-fill ──
  if (index >= 0 && index < sceneCount) {
    lv_textarea_set_text(ta_scene_name, scenes[index].name);
    lv_dropdown_set_selected(dd_scene_icon, scenes[index].icon_index);
    for (int i = 0; i < visible_action_count && i < scenes[index].action_count;
         i++) {
      lv_textarea_set_text(ta_topics[i], scenes[index].actions[i].topic);
      lv_textarea_set_text(ta_payloads[i], scenes[index].actions[i].payload);
    }
  } else {
    lv_textarea_set_text(ta_scene_name, "");
    lv_dropdown_set_selected(dd_scene_icon, 0);
  }
}

static void btn_add_action_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  if (visible_action_count < MAX_SCENE_ACTIONS) {
    visible_action_count++;
    // Rebuild the edit screen to add more rows
    build_edit_scene_screen(editSceneIndex);
    lv_scr_load(ui_ScreenEditScene);
  }
}

// ╔═══════════════════════════════════════════╗
// ║  SCENE TILES — For Main Screen Tab        ║
// ╚═══════════════════════════════════════════╝
void create_scene_tiles(lv_obj_t *parent) {
  for (int i = 0; i < sceneCount; i++) {
    lv_color_t accent = lv_color_hex(scenes[i].color);

    // Scene card — same surface language as a device tile, so the two grids
    // read as one system. The scene's own colour appears on the badge only.
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, UI_TILE_W, UI_TILE_H);
    lv_obj_clear_flag(card,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_bg_color(card, lv_color_hex(CLR_HEX_SURFACE_1), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(CLR_HEX_HAIRLINE), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, UI_TILE_RADIUS, 0);
    lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
    lv_obj_set_style_shadow_width(card, 14, 0);
    lv_obj_set_style_shadow_ofs_y(card, 4, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_border_color(card, accent, LV_STATE_PRESSED);

    // Icon circle
    lv_obj_t *ico_bg = lv_obj_create(card);
    lv_obj_set_size(ico_bg, 44, 44);
    lv_obj_align(ico_bg, LV_ALIGN_TOP_MID, 0, 3);
    lv_obj_set_style_bg_color(ico_bg, accent, 0);
    lv_obj_set_style_bg_opa(ico_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ico_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ico_bg, 0, 0);
    lv_obj_set_style_shadow_width(ico_bg, 0, 0);
    lv_obj_set_style_pad_all(ico_bg, 0, 0);
    lv_obj_clear_flag(ico_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ico_bg, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *ico = lv_label_create(ico_bg);
    lv_label_set_text(ico, getSceneIconSymbol(scenes[i].icon_index));
    // Scene colours run from indigo to yellow, so a fixed white glyph is
    // unreadable on the light ones. Pick the glyph colour from the badge's
    // perceived brightness instead.
    const uint32_t sc_rgb = scenes[i].color;
    const uint32_t luma = (((sc_rgb >> 16) & 0xFF) * 299 +
                           ((sc_rgb >> 8) & 0xFF) * 587 +
                           (sc_rgb & 0xFF) * 114) / 1000;
    lv_obj_set_style_text_color(
        ico, luma > 140 ? lv_color_hex(CLR_HEX_ON_ACCENT) : lv_color_white(), 0);
    lv_obj_center(ico);
    lv_obj_add_flag(ico, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Scene name
    lv_obj_t *nm = lv_label_create(card);
    lv_label_set_text(nm, scenes[i].name);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(nm, lv_color_hex(CLR_HEX_TEXT_HI), 0);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nm, UI_TILE_W - 18);
    lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(nm, LV_ALIGN_BOTTOM_MID, 0, -19);
    lv_obj_add_flag(nm, LV_OBJ_FLAG_EVENT_BUBBLE);

    // "Tap to run" label
    lv_obj_t *sub = lv_label_create(card);
    lv_label_set_text(sub, L(L_TAP_TO_RUN));
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_obj_add_flag(sub, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Touch handler — execute scene
    lv_obj_add_event_cb(
        card,
        [](lv_event_t *e) {
          int idx = (int)(ptrdiff_t)lv_event_get_user_data(e);
          executeScene(idx);

          // Visual feedback — brief flash on the card itself
          lv_obj_t *card = lv_event_get_current_target(e);
          lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
          lv_obj_set_style_bg_color(
              card, lv_color_hex(scenes[idx].color), 0);

          // Reset after 300ms via timer
          lv_timer_t *t = lv_timer_create(
              [](lv_timer_t *timer) {
                lv_obj_t *c = (lv_obj_t *)timer->user_data;
                lv_obj_set_style_bg_color(c, lv_color_hex(CLR_HEX_SURFACE_1), 0);
                lv_obj_set_style_bg_opa(c, LV_OPA_90, 0);
                lv_timer_del(timer);
              },
              300, card);
          (void)t;
        },
        LV_EVENT_SHORT_CLICKED, (void *)(ptrdiff_t)i);
  }
}
