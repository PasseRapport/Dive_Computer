#pragma once
#include <lvgl.h>
lv_obj_t *battery_icon_create(lv_obj_t *parent,int x,int y);
void battery_icon_update(lv_obj_t *icon,unsigned percentage);
uint32_t battery_icon_color(unsigned percentage);
