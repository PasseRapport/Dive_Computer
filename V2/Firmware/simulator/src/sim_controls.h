#pragma once
#include <lvgl.h>

lv_obj_t *sim_controls_create(void);
void sim_controls_poll(void);
/* True only after the actual wheel and both tap buttons were exercised. */
bool sim_controls_check_passed(void);

uint16_t sim_controls_depth_dm(void);

uint16_t sim_controls_heading_degrees(void);

void sim_controls_set_depth_dm(uint16_t value);

int sim_controls_depth_rate(void);

uint32_t sim_controls_depth_mm(void);
