#pragma once
#include <lvgl.h>
/* Native 320 x 300 scuba geometry, shared by the freediving data pages. */
void dive_layout_depth(lv_obj_t *depth,lv_obj_t *unit,uint16_t dm);
void dive_layout_rails(lv_obj_t *parent,uint32_t color);
