#pragma once
#include <stdint.h>
#include <lvgl.h>

#include "watch_data.h"

/* Physical input events: simulator controls and future board inputs use these. */
enum watch_input { WATCH_CROWN, WATCH_SINGLE_TAP, WATCH_DOUBLE_TAP };
enum watch_page { WATCH_MAIN, WATCH_STATS, WATCH_WATER, WATCH_DIVE_MODE, WATCH_BRIGHTNESS, WATCH_SLEEP, WATCH_SCUBA, WATCH_BLUETOOTH, WATCH_LEAVE, WATCH_SENSITIVITY, WATCH_HAPTICS,
                  WATCH_PAGE_COUNT };
enum watch_view { WATCH_CLOCK, WATCH_MODE_MENU, WATCH_DIVE_VIEW, WATCH_COMPASS_VIEW,
                  WATCH_OPTION_VIEW, WATCH_OPTION_DETAIL, WATCH_DIVE_SELECT, WATCH_BACKLIGHT_VIEW, WATCH_BOOT, WATCH_FACE_SELECT, WATCH_TOOL_LEAVE, WATCH_SLEEP_TRANSITION, WATCH_SCUBA_VIEW };
enum watch_mode { WATCH_WATCH, WATCH_DIVING, WATCH_COMPASS, WATCH_OPTION, WATCH_MODE_COUNT };
enum watch_dive_mode { DIVE_SCUBA, DIVE_FREE };
void watch_ui_create(lv_obj_t *parent);
void watch_ui_update(const struct watch_data *data);
void watch_ui_input(enum watch_input input, int steps);
enum watch_page watch_ui_page(void);
const char *watch_ui_page_name(void);
bool watch_ui_is_sleeping(void);
bool watch_ui_salt_water(void);
uint8_t watch_ui_brightness(void);
enum watch_view watch_ui_view(void);
enum watch_mode watch_ui_mode(void);
enum watch_dive_mode watch_ui_dive_mode(void);
int watch_ui_option(void);

int watch_ui_sleep_progress(void);
bool watch_ui_bluetooth(void);

enum watch_backlight_mode { BACKLIGHT_ON, BACKLIGHT_OFF, BACKLIGHT_AUTO };
enum watch_backlight_mode watch_ui_backlight_mode(void);
/* Relative light threshold, 0 to 100 percent. */
uint16_t watch_ui_sensitivity(void);
int watch_ui_backlight_option(void);
int watch_ui_leave_progress(void);
int watch_ui_bluetooth_option(void);
bool watch_ui_bluetooth_dfu(void);

enum watch_clock_style { CLOCK_DIGITAL, CLOCK_ANALOG };
enum watch_clock_style watch_ui_clock_style(void);
int watch_ui_face_option(void);

enum watch_pulse { PULSE_SHORT, PULSE_MEDIUM, PULSE_LONG };
bool watch_ui_haptics(void);
enum watch_pulse watch_ui_pulse(void);
int watch_ui_haptics_option(void);

void watch_ui_set_brightness(uint8_t value);

void watch_ui_set_backlight_mode(enum watch_backlight_mode mode);
void watch_ui_set_sensitivity(uint16_t value);
