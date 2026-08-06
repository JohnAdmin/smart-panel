#ifndef UI_DIMMER_MODAL_H
#define UI_DIMMER_MODAL_H

#include <lvgl.h>

extern lv_obj_t *ui_DimmerModal;

void build_dimmer_modal(int device_index);
void hide_dimmer_modal();
void update_dimmer_modal_value(int device_index, int brightness);

#endif // UI_DIMMER_MODAL_H
