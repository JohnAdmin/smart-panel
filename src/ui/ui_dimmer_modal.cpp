#include "ui_dimmer_modal.h"
#include "../config.h"
#include "../mqtt_manager.h"
#include "../wifi_manager.h" // For mqtt publishing
#include "ui_helpers.h"
#include "ui_screens.h"
#include "../lang.h"
#include <Arduino.h>
#include <lvgl.h>

lv_obj_t *ui_DimmerModal = NULL;
lv_obj_t *dimmer_slider = NULL;
lv_obj_t *dimmer_pct_label = NULL;
int current_dimmer_device_index = -1;

// --- Callbacks ---

// Fade-in driver. lv_obj_set_style_bg_opa() takes (obj, value, selector) — it
// cannot be cast to lv_anim_exec_xcb_t and called with two arguments, which
// leaves the selector reading whatever happens to be in the register.
static void modal_fade_anim_cb(void *obj, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void modal_close_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    hide_dimmer_modal();
  }
}

static void slider_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *slider = lv_event_get_target(e);

  // Update percentage label in real-time
  if (code == LV_EVENT_VALUE_CHANGED) {
    int val = lv_slider_get_value(slider);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(dimmer_pct_label, buf);
  }

  // Publish MQTT when user releases the slider
  if (code == LV_EVENT_RELEASED) {
    if (current_dimmer_device_index >= 0 &&
        current_dimmer_device_index < MAX_DEVICES) {
      int val = lv_slider_get_value(slider);

      // Use configured dimmer_topic; fall back to cmnd_topic-derived path
      const char *dtopic = devices[current_dimmer_device_index].dimmer_topic;
      String cmndTopic;
      if (dtopic[0] != '\0') {
        cmndTopic = dtopic;
      } else {
        cmndTopic = devices[current_dimmer_device_index].cmnd_topic;
        cmndTopic.replace("/POWER", "/Dimmer");
        cmndTopic.replace("/power", "/dimmer");
      }

      char payload[8];
      snprintf(payload, sizeof(payload), "%d", val);

      mqtt_publish_string(cmndTopic.c_str(), payload);
    }
  }
}

// --- UI Builders ---

void hide_dimmer_modal() {
  if (ui_DimmerModal) {
    lv_obj_del(ui_DimmerModal);
    ui_DimmerModal = NULL;
    current_dimmer_device_index = -1;
  }
}

void update_dimmer_modal_value(int device_index, int brightness) {
  if (ui_DimmerModal && current_dimmer_device_index == device_index &&
      dimmer_slider) {
    lv_slider_set_value(dimmer_slider, brightness, LV_ANIM_OFF);
    if (dimmer_pct_label) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d%%", brightness);
      lv_label_set_text(dimmer_pct_label, buf);
    }
  }
}

void build_dimmer_modal(int device_index) {
  if (device_index < 0 || device_index >= MAX_DEVICES)
    return;

  // If already open, close it first to recreate
  if (ui_DimmerModal) {
    hide_dimmer_modal();
  }

  current_dimmer_device_index = device_index;

  // 1. Full-screen transparent overlay blocks touches to background
  ui_DimmerModal = lv_obj_create(lv_scr_act());
  lv_obj_set_size(ui_DimmerModal, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(ui_DimmerModal, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(ui_DimmerModal, LV_OPA_70,
                          0); // Darken background slightly
  lv_obj_set_style_border_width(ui_DimmerModal, 0, 0);
  lv_obj_set_style_radius(ui_DimmerModal, 0, 0);
  lv_obj_set_style_pad_all(ui_DimmerModal, 0, 0);
  lv_obj_clear_flag(ui_DimmerModal, LV_OBJ_FLAG_SCROLLABLE);

  // Click overlay to close
  lv_obj_add_event_cb(ui_DimmerModal, modal_close_cb, LV_EVENT_CLICKED, NULL);

  // 2. Modal card — same surface language as a tile, one step more elevated.
  // The old version ringed the card in amber and glowed it; amber is reserved
  // for state, so here it appears only on the slider fill and the readout.
  lv_obj_t *panel = lv_obj_create(ui_DimmerModal);
  lv_obj_set_size(panel, UI_MODAL_W, UI_MODAL_H);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(CLR_HEX_SURFACE_1), 0);
  lv_obj_set_style_bg_grad_color(panel, lv_color_hex(CLR_HEX_SURFACE_0), 0);
  lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(CLR_HEX_HAIRLINE), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(panel, UI_CARD_RADIUS, 0);
  lv_obj_set_style_shadow_color(panel, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(panel, 40, 0);
  lv_obj_set_style_shadow_ofs_y(panel, 10, 0);
  lv_obj_set_style_shadow_opa(panel, LV_OPA_60, 0);
  lv_obj_set_style_pad_all(panel, 20, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  // Absorb clicks so clicking the panel itself doesn't close it
  lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);

  // 3. Header — what is being controlled, then what the control does
  lv_obj_t *title = lv_label_create(panel);
  lv_label_set_text(title, devices[device_index].name);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(CLR_HEX_TEXT_HI), 0);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title, UI_MODAL_W - 80);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *subtitle = lv_label_create(panel);
  lv_label_set_text(subtitle, L(L_BRIGHTNESS_CTRL));
  lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(subtitle, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 0, 24);

  // 4. Percentage readout — top right, the value you are steering
  dimmer_pct_label = lv_label_create(panel);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", devices[device_index].brightness);
  lv_label_set_text(dimmer_pct_label, buf);
  lv_obj_set_style_text_font(dimmer_pct_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(dimmer_pct_label, lv_color_hex(CLR_HEX_ACCENT), 0);
  lv_obj_align(dimmer_pct_label, LV_ALIGN_TOP_RIGHT, 0, 0);

  // 5. Slider — thick enough to drive with a thumb
  dimmer_slider = lv_slider_create(panel);
  lv_obj_set_size(dimmer_slider, UI_MODAL_W - 40, 38);
  lv_obj_align(dimmer_slider, LV_ALIGN_CENTER, 0, 6);
  lv_slider_set_range(dimmer_slider, 0, 100);
  lv_slider_set_value(dimmer_slider, devices[device_index].brightness,
                      LV_ANIM_OFF);
  ui_style_slider(dimmer_slider);
  lv_obj_set_style_pad_all(dimmer_slider, 4, LV_PART_KNOB); // knob > track
  lv_obj_add_event_cb(dimmer_slider, slider_event_cb, LV_EVENT_ALL, NULL);

  // 6. Dismiss hint — the overlay closes on tap, which is otherwise invisible
  lv_obj_t *hint = lv_label_create(panel);
  lv_label_set_text(hint, L(L_TAP_OUTSIDE_CLOSE));
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(CLR_HEX_TEXT_LOW), 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);

  // Smooth Fade-in animation for the panel
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, panel);
  lv_anim_set_values(&a, 0, LV_OPA_COVER); // fade the card in
  lv_anim_set_time(&a, 200);
  lv_anim_set_exec_cb(&a, modal_fade_anim_cb);
  lv_anim_start(&a);
}
