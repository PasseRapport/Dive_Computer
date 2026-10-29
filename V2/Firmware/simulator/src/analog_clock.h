#pragma once
#include <lvgl.h>
#include "watch_data.h"

enum watch_dial { DIAL_MINIMALIST, DIAL_DAUPHINE, DIAL_AUTOMATIC, DIAL_COUNT };
lv_obj_t *analog_clock_create(lv_obj_t *parent);
void analog_clock_set_dial(enum watch_dial dial);
void analog_clock_update(const struct watch_data *data);
lv_obj_t *analog_clock_logo(void);
int analog_clock_logo_y(void);
int analog_clock_logo_scale(void);
