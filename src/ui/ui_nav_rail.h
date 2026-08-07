#pragma once
#include <lvgl.h>

// ────────────────────────────────────────────────────────
//  LEFT NAVIGATION RAIL
// ────────────────────────────────────────────────────────
// The panel's only top-level navigation. Before this, Scenes, Schedules and
// Devices were all reachable only by going through Settings, which put three
// everyday destinations two taps deep behind a gear icon.
//
// Destinations are drawn top-to-bottom in the order of this enum. A further
// button (screensaver) sits at the bottom of the rail, separated from the
// destinations because it is an action, not a place.
//
// Scenes and schedules share one destination with two tabs: they are the same
// job — "what runs, and when" — and splitting them cost a rail slot for a
// screen most users open rarely. The freed slot is where Sensors goes.
enum UiNavDest {
  UI_NAV_HOME = 0,
  UI_NAV_SCENES, // scenes + schedule, as two tabs
  UI_NAV_SENSORS,
  UI_NAV_SETTINGS,
  UI_NAV_COUNT
};

// Builds the rail as a child of `screen` and returns it. `active` is drawn
// highlighted — pass UI_NAV_COUNT for a rail with nothing selected.
//
// Call it *after* ui_wallpaper_load() so the rail stacks above the wallpaper
// canvas. The rail keeps an opaque fill on purpose: it is the one fixed
// landmark on the screen and must not shift with whatever image the user
// uploaded.
lv_obj_t *ui_nav_rail_create(lv_obj_t *screen, UiNavDest active);

// Repaints the monogram on every live rail. Call after panelTitle changes.
void ui_nav_rail_refresh_logo();
