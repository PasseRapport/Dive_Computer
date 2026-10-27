#pragma once
#include <lvgl.h>
lv_obj_t *light_editor_create(lv_obj_t *parent,const char *title,int maximum);
void light_editor_set_value(lv_obj_t *editor,int value);
