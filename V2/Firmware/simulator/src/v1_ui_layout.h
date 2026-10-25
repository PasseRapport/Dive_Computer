#ifndef V1_UI_LAYOUT_H
#define V1_UI_LAYOUT_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

enum {
    V1_PAGE_HOME_X = 48,
    V1_PAGE_HOME_Y = 66,
    V1_PAGE_TRAVEL = 338, /* Full 320-pixel viewport plus the inter-page gap. */
};

// --- Pointeurs Globaux des Écrans ---
extern lv_obj_t * boot_screen;
extern lv_obj_t * main_screen;
extern lv_obj_t * water_screen;
extern lv_obj_t * stats_screen;
extern lv_obj_t * brightness_screen;
extern lv_obj_t * sleep_screen;

// --- Prototypes d'initialisation ---
extern lv_obj_t *app_screen;
void v1_ui_build(lv_obj_t *parent);
void ui_skip_boot(void);
lv_obj_t *ui_create_dive_mode_page(void);
void ui_refresh_dive_mode(bool freediving);

// --- Fonctions d'accès pour modifier l'affichage (Setters) ---
void ui_refresh_runtime_display(uint32_t h, uint32_t m);
void ui_refresh_brightness_bar(uint8_t level);
void ui_refresh_water_selection(bool is_salt);
void ui_refresh_dive_time(uint32_t total_secs);
void ui_refresh_depth(float depth);
void ui_refresh_temp(float temp);
void ui_update_recovery_display(uint32_t seconds, bool visible, bool is_red);
void ui_refresh_last_depth(float depth);
void ui_refresh_stats(uint32_t max_time_s, float max_depth, uint32_t total_dives);
void ui_slide_pages(lv_obj_t * page_out, lv_obj_t * page_in, int direction);
void ui_refresh_battery(uint8_t percentage);

// Tu pourras ajouter plus tard : void ui_refresh_depth(float depth); etc.

#endif // V1_UI_LAYOUT_H

bool ui_water_selection_within_page(void);
