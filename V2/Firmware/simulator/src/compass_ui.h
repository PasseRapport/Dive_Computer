#pragma once
#include <lvgl.h>

lv_obj_t *compass_ui_create(lv_obj_t *parent);
void compass_ui_update(uint16_t heading_degrees);

uint16_t compass_ui_heading(void);

lv_obj_t *compass_face_create(lv_obj_t *parent);
void compass_face_update(lv_obj_t *view, uint16_t heading);

void compass_face_set_direction_visible(lv_obj_t *view, bool visible);
