#pragma once
#include <lvgl.h>
#include "watch_data.h"

enum watch_dial { DIAL_MINIMALIST, DIAL_DAUPHINE, DIAL_AUTOMATIC, DIAL_RADAR, DIAL_POLAR, DIAL_IVORY, DIAL_SOLAR, DIAL_COUNT };
lv_obj_t *analog_clock_create(lv_obj_t *parent);
void analog_clock_set_dial(lv_obj_t *view,enum watch_dial dial);
void analog_clock_update(lv_obj_t *view,const struct watch_data *data);
lv_obj_t *analog_clock_logo(lv_obj_t *view);
int analog_clock_logo_y(lv_obj_t *view);
int analog_clock_logo_scale(lv_obj_t *view);
