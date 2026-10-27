#pragma once
#include "watch_ui.h"
#include "scuba_model.h"
#include "scuba_dashboard.h"

enum scuba_screen { SCUBA_PREP, SCUBA_MAIN, SCUBA_OPTIONS_GATE, SCUBA_OPTIONS, SCUBA_BACKLIGHT, SCUBA_SENSITIVITY, SCUBA_RING_SELECT, SCUBA_FIELD_SELECT, SCUBA_SUMMARY, SCUBA_LEAVE, SCUBA_SETTINGS, SCUBA_SETTING, SCUBA_BRIGHTNESS, SCUBA_WATER };
lv_obj_t *scuba_ui_create(lv_obj_t *parent);
void scuba_ui_enter(uint64_t now_ms);
void scuba_ui_update(const struct watch_data *data, uint64_t now_ms, bool visible);
/* True when the completed exit ring requests the main menu. */
bool scuba_ui_input(enum watch_input input, int steps);
void scuba_ui_set_scenario(enum scuba_scenario scenario);
void scuba_ui_set_speed(uint8_t speed);
void scuba_ui_manual(void);
void scuba_ui_set_battery(uint8_t battery);
const struct scuba_model *scuba_ui_model(void);
enum scuba_screen scuba_ui_screen(void);
uint32_t scuba_ui_haptic_count(void);

bool scuba_ui_haptic_active(void);

enum scuba_field scuba_ui_field(void);

enum scuba_ring scuba_ui_ring(void);
