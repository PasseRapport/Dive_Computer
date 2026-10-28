#pragma once
#include "watch_ui.h"
enum apnea_bottom_field { APNEA_TEMPERATURE, APNEA_SESSION_DEPTH, APNEA_SESSION_TIME, APNEA_CLOCK, APNEA_DIVE_COUNT, APNEA_BOTTOM_COUNT };
lv_obj_t *apnea_settings_create(lv_obj_t *parent);
void apnea_settings_update(bool underwater);
bool apnea_settings_input(enum watch_input input,int steps);
void apnea_settings_leave(void);
bool apnea_settings_salt_water(void);
enum apnea_bottom_field apnea_settings_bottom_field(void);
/* Startup adapter restores only applied preferences, never session data. */
void apnea_settings_restore(bool salt,enum apnea_bottom_field field);

bool apnea_settings_selection_within_page(void);
