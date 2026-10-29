#pragma once
#include "analog_clock.h"
enum watch_clock_style { CLOCK_DIGITAL, CLOCK_ANALOG };
/* Values are stable preference identifiers; append new faces, never reorder. */
enum digital_dial { DIGITAL_EMBLEM, DIGITAL_TERMINAL, DIGITAL_TOTEM, DIGITAL_SPORT, DIGITAL_SEAL, DIGITAL_COUNT };
int watch_face_count(enum watch_clock_style type);
const char *watch_face_name(enum watch_clock_style type,int dial);
lv_obj_t *watch_face_create(lv_obj_t *parent,enum watch_clock_style type,int dial);
void watch_face_update(lv_obj_t *view,const struct watch_data *data);
lv_obj_t *watch_face_logo(lv_obj_t *view);
int watch_face_logo_x(lv_obj_t *view);
int watch_face_logo_y(lv_obj_t *view);
int watch_face_logo_scale(lv_obj_t *view);
