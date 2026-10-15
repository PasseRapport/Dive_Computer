#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl.h"

// --- Global UI Pointers ---
extern lv_obj_t * boot_screen;
extern lv_obj_t * main_screen;
extern lv_obj_t * water_screen;
extern lv_obj_t * stats_screen;



// --- Function Prototypes ---
void init_oled_display(void);
void build_ui_state_machine(void);
void set_display_brightness(uint8_t level);
void oled_display_power_off(void);

#endif // OLED_DISPLAY_H