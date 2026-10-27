#pragma once
#include <lvgl.h>
lv_obj_t *compass_ring_create(lv_obj_t *parent);
void compass_ring_update(lv_obj_t *ring,unsigned heading);
