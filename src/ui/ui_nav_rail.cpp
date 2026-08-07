#include "ui_nav_rail.h"
#include "../../include/globals.h"
#include "ui_helpers.h"
#include "ui_screens.h"
#include <Arduino.h>

// ── Live rail registry ──────────────────────────────────
// More than one screen can carry a rail, and each keeps its own monogram
// label. The registry exists so ui_nav_rail_refresh_logo() can update all of
// them without any screen having to publish its rail pointer. Slots are
// cleared on LV_EVENT_DELETE, so a screen that gets torn down (every
// sub-screen does — see the cleanup_* helpers) never leaves a dangling entry.
#define UI_RAIL_MAX 4
static lv_obj_t *s_logo_lbls[UI_RAIL_MAX] = {NULL};

static void rail_deleted_cb(lv_event_t *e) {
  lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
  for (int i = 0; i < UI_RAIL_MAX; i++)
    if (s_logo_lbls[i] == lbl) s_logo_lbls[i] = NULL;
}

static void rail_register_logo(lv_obj_t *rail, lv_obj_t *lbl) {
  for (int i = 0; i < UI_RAIL_MAX; i++) {
    if (s_logo_lbls[i] == NULL) {
      s_logo_lbls[i] = lbl;
      lv_obj_add_event_cb(rail, rail_deleted_cb, LV_EVENT_DELETE, lbl);
      return;
    }
  }
}

// First visible character of the panel name, as a NUL-terminated UTF-8 string.
// Thai and other multi-byte names would otherwise be cut mid-sequence and
// render as a replacement box.
static void panel_monogram(char *out, size_t out_sz) {
  const char *p = panelTitle;
  while (*p == ' ') p++;
  if (!*p) { snprintf(out, out_sz, "S"); return; }
  uint8_t b0 = (uint8_t)*p;
  int len = (b0 < 0x80) ? 1 : (b0 < 0xE0) ? 2 : (b0 < 0xF0) ? 3 : 4;
  if ((size_t)len >= out_sz) len = (int)out_sz - 1;
  memcpy(out, p, len);
  out[len] = '\0';
}

// ── Navigation ──────────────────────────────────────────
static void nav_btn_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  UiNavDest dest = (UiNavDest)(intptr_t)lv_event_get_user_data(e);

  // Home and Scenes are views inside ui_ScreenMain, not screens of their own —
  // switch the view first, then bring that screen forward if something else
  // is on top. Scenes here is the tap-to-run grid; the scene *editor* stays
  // where it was, behind Settings.
  switch (dest) {
  case UI_NAV_HOME:
  case UI_NAV_SCENES:
    ui_show_main_view(dest == UI_NAV_HOME ? UI_VIEW_HOME : UI_VIEW_SCENES);
    if (lv_scr_act() != ui_ScreenMain)
      lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
    break;
  case UI_NAV_SETTINGS:
    build_settings_screen();
    lv_scr_load_anim(ui_ScreenSettings, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
    break;
  default:
    break;
  }
}

static void saver_btn_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  show_screensaver();
}

// ── Button factory ──────────────────────────────────────
// 38×38 icon-only square. Selected state is an accent tint rather than a solid
// fill: the rail sits next to device tiles that use a solid accent to mean
// "this device is on", and a filled nav button would read as the same signal.
static lv_obj_t *rail_btn(lv_obj_t *rail, const char *glyph, int y,
                          bool active, lv_event_cb_t cb, void *user_data) {
  lv_obj_t *btn = lv_btn_create(rail);
  lv_obj_set_size(btn, 38, 38);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(CLR_HEX_ACCENT), 0);
  // ~12 % — the accent reads as a tint here, not as a lit surface.
  lv_obj_set_style_bg_opa(btn, active ? (lv_opa_t)31 : LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(btn, (lv_opa_t)56, LV_STATE_PRESSED);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, glyph);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(lbl,
                              lv_color_hex(active ? CLR_HEX_ACCENT
                                                  : CLR_HEX_TEXT_LOW), 0);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  return btn;
}

// ── Rail ────────────────────────────────────────────────
lv_obj_t *ui_nav_rail_create(lv_obj_t *screen, UiNavDest active) {
  lv_obj_t *rail = lv_obj_create(screen);
  lv_obj_set_size(rail, UI_RAIL_W, SCREEN_HEIGHT);
  lv_obj_align(rail, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_color(rail, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(rail, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_opa(rail, LV_OPA_60, 0);
  lv_obj_set_style_border_width(rail, 1, 0);
  lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_radius(rail, 0, 0);
  lv_obj_set_style_pad_all(rail, 0, 0);
  lv_obj_set_style_shadow_width(rail, 0, 0);
  lv_obj_clear_flag(rail, LV_OBJ_FLAG_SCROLLABLE);

  // Logo — 28×28 accent square carrying the panel name's first letter.
  lv_obj_t *logo = lv_obj_create(rail);
  lv_obj_set_size(logo, 28, 28);
  lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_bg_color(logo, lv_color_hex(CLR_HEX_ACCENT), 0);
  lv_obj_set_style_bg_opa(logo, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(logo, 0, 0);
  lv_obj_set_style_shadow_width(logo, 0, 0);
  lv_obj_set_style_radius(logo, 8, 0);
  lv_obj_set_style_pad_all(logo, 0, 0);
  lv_obj_clear_flag(logo, LV_OBJ_FLAG_SCROLLABLE);

  char mono[8];
  panel_monogram(mono, sizeof(mono));
  lv_obj_t *logo_lbl = lv_label_create(logo);
  lv_label_set_text(logo_lbl, mono);
  lv_obj_set_style_text_font(logo_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(logo_lbl, lv_color_hex(CLR_HEX_ON_ACCENT), 0);
  lv_obj_center(logo_lbl);
  rail_register_logo(rail, logo_lbl);

  // Four destinations, 38 px tall on a 42 px pitch, starting below the logo.
  static const char *glyphs[UI_NAV_COUNT] = {
      LV_SYMBOL_HOME,     // house
      LV_SYMBOL_VIDEO,    // film — scenes + schedule
      LV_SYMBOL_SETTINGS, // gear
  };
  for (int i = 0; i < UI_NAV_COUNT; i++)
    rail_btn(rail, glyphs[i], 48 + i * 42, active == (UiNavDest)i, nav_btn_cb,
             (void *)(intptr_t)i);

  // Screensaver — an action, parked at the far end of the rail so it never
  // reads as a fifth destination.
  lv_obj_t *saver = rail_btn(rail, LV_SYMBOL_EYE_CLOSE, 0, false,
                             saver_btn_cb, NULL);
  lv_obj_align(saver, LV_ALIGN_BOTTOM_MID, 0, -10);

  return rail;
}

void ui_nav_rail_refresh_logo() {
  char mono[8];
  panel_monogram(mono, sizeof(mono));
  for (int i = 0; i < UI_RAIL_MAX; i++)
    if (s_logo_lbls[i]) lv_label_set_text(s_logo_lbls[i], mono);
}
