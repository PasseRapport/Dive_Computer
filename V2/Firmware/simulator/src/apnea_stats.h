#pragma once
#include <lvgl.h>
#include "watch_data.h"
lv_obj_t *apnea_stats_create(lv_obj_t *parent);
void apnea_stats_update(lv_obj_t *view,const struct watch_data *data);
