#pragma once
#include <lvgl.h>
#include "scuba_model.h"
#include "watch_data.h"
enum scuba_field { FIELD_TEMPERATURE, FIELD_MAX_DEPTH, FIELD_CLOCK, FIELD_BATTERY,
                   FIELD_SAFETY_STOP, FIELD_TTS, FIELD_PPO2, FIELD_HEADING,
                   FIELD_CEILING, FIELD_STOP_COMPLETE };
enum scuba_ring { RING_AUTO, RING_NDL, RING_TTS, RING_STOP, RING_COMPASS, SCUBA_RING_COUNT };
#define SCUBA_FIELD_COUNT 8
const char *scuba_field_name(enum scuba_field field);
const char *scuba_ring_name(enum scuba_ring ring);
void scuba_dashboard_set_field(lv_obj_t *dashboard, enum scuba_field field);
void scuba_dashboard_set_ring(lv_obj_t *dashboard, enum scuba_ring ring);
enum scuba_ring scuba_dashboard_ring(lv_obj_t *dashboard);
lv_obj_t *scuba_dashboard_create(lv_obj_t *parent);
void scuba_dashboard_update(lv_obj_t *dashboard,const struct scuba_model *model,const struct watch_data *data);
enum scuba_field scuba_dashboard_field(lv_obj_t *dashboard);
