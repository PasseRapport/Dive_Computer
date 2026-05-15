#include "ui_layout.h"
#include <stdio.h>

LV_IMG_DECLARE(turn_fish);
LV_IMG_DECLARE(fish); 
LV_IMG_DECLARE(sanic); 
LV_IMG_DECLARE(icon_sun); 

// --- Variables Globales ---
lv_obj_t * boot_screen;
lv_obj_t * main_screen;
lv_obj_t * water_screen;
lv_obj_t * stats_screen;
lv_obj_t * brightness_screen;
static lv_obj_t * brightness_bar; // Rendu static car seul layout a besoin de le voir

// --- Pointeurs internes pour la mise à jour ---
static lv_obj_t * label_time_val;
static lv_obj_t * label_recovery_time;
static lv_obj_t * label_temp_val;
static lv_obj_t * label_depth_val;
static lv_obj_t * label_depth_unit;
static lv_obj_t * label_max_val;
static lv_obj_t * label_clear;
static lv_obj_t * label_salt;
static lv_obj_t * run_h_val;
static lv_obj_t * run_h_unit;
static lv_obj_t * run_m_val;
static lv_obj_t * run_m_unit;
static lv_obj_t * label_stat_divetime_val;
static lv_obj_t * label_stat_max_val;
static lv_obj_t * label_stat_max_unit;
static lv_obj_t * label_stat_dives_val;

// --- Callbacks d'Animation de Démarrage ---
static void boot_fade_in_cb(lv_timer_t * timer) {
    lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_FADE_ON, 800, 0, true);
    lv_timer_del(timer);
}

static void boot_fade_out_cb(lv_timer_t * timer) {
    lv_obj_t * temp_black_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(temp_black_screen, lv_color_black(), 0);
    lv_obj_clear_flag(temp_black_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_scr_load_anim(temp_black_screen, LV_SCR_LOAD_ANIM_FADE_ON, 800, 0, true);
    boot_screen = NULL; 

    lv_timer_create(boot_fade_in_cb, 1000, NULL);
    lv_timer_del(timer);
}

void build_ui_state_machine(void) {
// ----------------------------------------CREATE BOOT SCREEN ------------------------------------
    boot_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(boot_screen, lv_color_black(), 0);
    
    lv_obj_t * logo = lv_gif_create(boot_screen);
    lv_gif_set_src(logo, &turn_fish); 
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, 0);

    // ---------------------------------------- CREATE MAIN SCREEN ------------------------------------
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_black(), 0);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(main_screen, lv_color_white(), 0);

    // QUADRANT 1: TOP LEFT (TIME)
    lv_obj_t * label_time_title = lv_label_create(main_screen);
    lv_label_set_text(label_time_title, "TIME");
    lv_obj_set_style_text_font(label_time_title, &lv_font_montserrat_12, 0);       
    lv_obj_set_style_text_color(label_time_title, lv_color_hex(0xAAAAAA), 0);      
    lv_obj_align(label_time_title, LV_ALIGN_TOP_LEFT, 16, 1); 

    label_time_val = lv_label_create(main_screen);
    lv_label_set_text(label_time_val, "0:00");
    lv_obj_set_style_text_font(label_time_val, &lv_font_montserrat_24, 0);         
    lv_obj_align_to(label_time_val, label_time_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    // QUADRANT 2: TOP RIGHT (TEMP)
    lv_obj_t * label_temp_title = lv_label_create(main_screen);
    lv_label_set_text(label_temp_title, "TEMP");
    lv_obj_set_style_text_font(label_temp_title, &lv_font_montserrat_12, 0);       
    lv_obj_set_style_text_color(label_temp_title, lv_color_hex(0xAAAAAA), 0);      
    lv_obj_align(label_temp_title, LV_ALIGN_TOP_RIGHT, -10, 1);

    label_temp_val = lv_label_create(main_screen);
    lv_label_set_text(label_temp_val, "00°C");
    lv_obj_set_style_text_font(label_temp_val, &lv_font_montserrat_24, 0);         
    lv_obj_align_to(label_temp_val, label_temp_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    // QUADRANT 3: BOTTOM LEFT (DEPTH)
    lv_obj_t * label_depth_title = lv_label_create(main_screen);
    lv_label_set_text(label_depth_title, "DEPTH");
    lv_obj_set_style_text_font(label_depth_title, &lv_font_montserrat_12, 0);      
    lv_obj_set_style_text_color(label_depth_title, lv_color_hex(0xAAAAAA), 0);     
    lv_obj_align(label_depth_title, LV_ALIGN_TOP_LEFT, 12, 48);

    label_depth_val = lv_label_create(main_screen);
    lv_label_set_text(label_depth_val, "0.0");                                    
    lv_obj_set_style_text_font(label_depth_val, &lv_font_montserrat_34, 0);        
    lv_obj_align_to(label_depth_val, label_depth_title, LV_ALIGN_OUT_BOTTOM_MID, 0, -2);

    label_depth_unit = lv_label_create(main_screen);
    lv_label_set_text(label_depth_unit, "m");
    lv_obj_set_style_text_font(label_depth_unit, &lv_font_montserrat_14, 0);       
    lv_obj_align_to(label_depth_unit, label_depth_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);

    // QUADRANT 4: BOTTOM RIGHT (LASTDEPTH)
    lv_obj_t * label_max_title = lv_label_create(main_screen);
    lv_label_set_text(label_max_title, "LAST");
    lv_obj_set_style_text_font(label_max_title, &lv_font_montserrat_12, 0);        
    lv_obj_set_style_text_color(label_max_title, lv_color_hex(0x999999), 0);       
    lv_obj_align(label_max_title, LV_ALIGN_BOTTOM_RIGHT, -5, -16);

    label_max_val = lv_label_create(main_screen);
    lv_label_set_text(label_max_val, "0.0");
    lv_obj_set_style_text_font(label_max_val, &lv_font_montserrat_14, 0);          
    lv_obj_align_to(label_max_val, label_max_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);


    // RIGHT CENTER (RECOVERY TIME)
    label_recovery_time = lv_label_create(main_screen);
    lv_label_set_text(label_recovery_time, "");
    lv_obj_set_style_text_font(label_recovery_time, &lv_font_montserrat_16, 0); 
    lv_obj_set_style_text_color(label_recovery_time, lv_color_hex(0x00FF00), 0);         
    lv_obj_align(label_recovery_time, LV_ALIGN_BOTTOM_RIGHT, -15, -32);

    // ---------------------------------------- CREATE WATER TYPE SCREEN ------------------------------------
    water_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(water_screen, lv_color_black(), 0);
    lv_obj_clear_flag(water_screen, LV_OBJ_FLAG_SCROLLABLE); 

    label_clear = lv_label_create(water_screen);
    lv_label_set_text(label_clear, "CLEAR WATER");
    lv_obj_set_style_text_font(label_clear, &lv_font_montserrat_16, 0); 
    lv_obj_set_style_text_color(label_clear, lv_color_white(), 0);
    lv_obj_align(label_clear, LV_ALIGN_CENTER, 0, -20); 

    label_salt = lv_label_create(water_screen);
    lv_label_set_text(label_salt, "SALT WATER");
    lv_obj_set_style_text_font(label_salt, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_salt, lv_color_hex(0x888888), 0);
    lv_obj_align(label_salt, LV_ALIGN_CENTER, 0, 20);

   // ---------------------------------------- CREATE STATS SCREEN ------------------------------------
    stats_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(stats_screen, lv_color_black(), 0);
    lv_obj_clear_flag(stats_screen, LV_OBJ_FLAG_SCROLLABLE);

    // --- QUADRANT 1: MAX TIME (En haut à gauche) ---
    lv_obj_t * label_stat_divetime_title = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_divetime_title, "MAX TIME");
    lv_obj_set_style_text_font(label_stat_divetime_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(label_stat_divetime_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_stat_divetime_title, LV_ALIGN_TOP_LEFT, 0, 0);

    label_stat_divetime_val = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_divetime_val, "0:00");
    lv_obj_set_style_text_font(label_stat_divetime_val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_stat_divetime_val, lv_color_white(), 0);
    lv_obj_align_to(label_stat_divetime_val, label_stat_divetime_title, LV_ALIGN_OUT_BOTTOM_MID, 0, -2);

// --- QUADRANT 2: MAX DEPTH (En haut à droite) ---
    lv_obj_t * label_stat_max_title = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_max_title, "MAX DEPTH");
    lv_obj_set_style_text_font(label_stat_max_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(label_stat_max_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_stat_max_title, LV_ALIGN_TOP_RIGHT, 0, 0); // Légèrement décalé du bord droit

    // 1. On crée le "m" d'abord, et on le fixe sous la droite du titre
    label_stat_max_unit = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_max_unit, "m");
    lv_obj_set_style_text_font(label_stat_max_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_stat_max_unit, lv_color_white(), 0);
    lv_obj_align_to(label_stat_max_unit, label_stat_max_title, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 7);

    // 2. On crée la valeur, et on l'accroche à GAUCHE du "m"
    label_stat_max_val = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_max_val, "0.0");
    lv_obj_set_style_text_font(label_stat_max_val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_stat_max_val, lv_color_white(), 0);
    lv_obj_align_to(label_stat_max_val, label_stat_max_unit, LV_ALIGN_OUT_LEFT_BOTTOM, -2, 2);

    // --- QUADRANT 3: RUN TIME (Désormais en bas à gauche) ---
    lv_obj_t * label_stat_run_title = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_run_title, "RUN TIME");
    lv_obj_set_style_text_font(label_stat_run_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_stat_run_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_stat_run_title, LV_ALIGN_TOP_LEFT, 0, 55); 

    run_h_val = lv_label_create(stats_screen);
    lv_label_set_text(run_h_val, "0");
    lv_obj_set_style_text_font(run_h_val, &lv_font_montserrat_24, 0); 
    lv_obj_set_style_text_color(run_h_val, lv_color_white(), 0);
    lv_obj_align_to(run_h_val, label_stat_run_title, LV_ALIGN_OUT_BOTTOM_MID, -23, -2); 

    run_h_unit = lv_label_create(stats_screen);
    lv_label_set_text(run_h_unit, "h");
    lv_obj_set_style_text_font(run_h_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(run_h_unit, lv_color_white(), 0);
    lv_obj_align_to(run_h_unit, run_h_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 1, -2);

    run_m_val = lv_label_create(stats_screen);
    lv_label_set_text(run_m_val, "00");
    lv_obj_set_style_text_font(run_m_val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(run_m_val, lv_color_white(), 0);
    lv_obj_align_to(run_m_val, run_h_unit, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 2);

    run_m_unit = lv_label_create(stats_screen);
    lv_label_set_text(run_m_unit, "min");
    lv_obj_set_style_text_font(run_m_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(run_m_unit, lv_color_white(), 0);
    lv_obj_align_to(run_m_unit, run_m_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 1, -2);

    // --- QUADRANT 4: DIVES  ---
    lv_obj_t * label_stat_dives_title = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_dives_title, "DIVES");
    lv_obj_set_style_text_font(label_stat_dives_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_stat_dives_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_stat_dives_title, LV_ALIGN_TOP_RIGHT, -5, 55);

    label_stat_dives_val = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_dives_val, "0");
    lv_obj_set_style_text_font(label_stat_dives_val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_stat_dives_val, lv_color_white(), 0);
    lv_obj_align_to(label_stat_dives_val, label_stat_dives_title, LV_ALIGN_OUT_BOTTOM_MID, 0, -2);

    // ---------------------------------------- CREATE BRIGHTNESS SCREEN ------------------------------------
    brightness_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(brightness_screen, lv_color_black(), 0);
    lv_obj_clear_flag(brightness_screen, LV_OBJ_FLAG_SCROLLABLE); 

    lv_obj_t * label_bright_title = lv_label_create(brightness_screen);
    lv_label_set_text(label_bright_title, "BRIGHTNESS");
    lv_obj_set_style_text_font(label_bright_title, &lv_font_montserrat_14, 0); 
    lv_obj_set_style_text_color(label_bright_title, lv_color_white(), 0);
    lv_obj_align(label_bright_title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t * label_minus = lv_label_create(brightness_screen);
    lv_label_set_text(label_minus, "-"); 
    lv_obj_set_style_text_font(label_minus, &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(label_minus, lv_color_white(), 0);
    lv_obj_align(label_minus, LV_ALIGN_CENTER, -40, 0); 

    lv_obj_t * img_sun = lv_img_create(brightness_screen);
    lv_img_set_src(img_sun, &icon_sun); 
    lv_obj_align(img_sun, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * label_plus = lv_label_create(brightness_screen);
    lv_label_set_text(label_plus, "+"); 
    lv_obj_set_style_text_font(label_plus, &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(label_plus, lv_color_white(), 0);
    lv_obj_align(label_plus, LV_ALIGN_CENTER, 40, 0);

    // --- BARRE DE LUMINOSITÉ ---
    brightness_bar = lv_bar_create(brightness_screen);
    lv_obj_set_size(brightness_bar, 100, 12); 
    lv_obj_align(brightness_bar, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_bar_set_range(brightness_bar, 0, 5);
    lv_bar_set_value(brightness_bar, 3, LV_ANIM_OFF); 
    lv_obj_set_style_radius(brightness_bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(brightness_bar, lv_color_black(), 0);
    lv_obj_set_style_border_color(brightness_bar, lv_color_white(), 0);
    lv_obj_set_style_border_width(brightness_bar, 2, 0);
    lv_obj_set_style_bg_color(brightness_bar, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(brightness_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    // ---------------------------------------- START THE SEQUENCE ------------------------------------
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_scr_load_anim(boot_screen, LV_SCR_LOAD_ANIM_FADE_ON, 800, 0, false);
    
    lv_timer_create(boot_fade_out_cb, 3500, NULL);

    
}

// =========================================================================
// === FONCTIONS D'ACCÈS POUR LE CONTRÔLEUR (Setters) ======================
// =========================================================================

void ui_refresh_runtime_display(uint32_t h, uint32_t m) {
    lv_label_set_text_fmt(run_h_val, "%lu", h);
    lv_label_set_text_fmt(run_m_val, "%02lu", m);
    // On réaligne pour éviter que les chiffres ne se chevauchent s'ils s'allongent
    lv_obj_align_to(run_h_unit, run_h_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 1, -2);
    lv_obj_align_to(run_m_val, run_h_unit, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 2);
    lv_obj_align_to(run_m_unit, run_m_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 1, -2);
}

void ui_refresh_brightness_bar(uint8_t level) {
    if(level > 5) level = 5;
    lv_bar_set_value(brightness_bar, level, LV_ANIM_ON);
}

void ui_refresh_water_selection(bool is_salt) {
    if (is_salt) {
        lv_obj_set_style_text_color(label_clear, lv_color_hex(0x008B8B), 0); 
        lv_obj_set_style_text_color(label_salt, lv_color_white(), 0);              
    } else {
        lv_obj_set_style_text_color(label_clear, lv_color_white(), 0);               
        lv_obj_set_style_text_color(label_salt, lv_color_hex(0x008B8B), 0); 
    }
}


void ui_refresh_dive_time(uint32_t total_secs) {
    uint32_t m = total_secs / 60;
    uint32_t s = total_secs % 60;
    
    // Format "M:SS" (ex: 1:35)
    lv_label_set_text_fmt(label_time_val, "%lu:%02lu", m, s);
}



// Met à jour l'affichage de la profondeur
void ui_refresh_depth(float depth) {
    char buf[16]; // On crée un petit espace mémoire temporaire (buffer)
    
    // Le vrai C transforme le float en texte et le range dans 'buf'
    snprintf(buf, sizeof(buf), "%.1f", depth); 
    
    // On donne le texte brut à LVGL (on n'utilise plus _fmt)
    lv_label_set_text(label_depth_val, buf); 
    lv_obj_align_to(label_depth_unit, label_depth_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);
}

// Met à jour l'affichage de la température
void ui_refresh_temp(float temp) {
    char buf[16];
    
    // On ajoute le °C directement dans la conversion C
    snprintf(buf, sizeof(buf), "%.0f°C", temp); 
    
    lv_label_set_text(label_temp_val, buf);
}


void ui_update_recovery_display(uint32_t seconds, bool visible, bool is_red) {
    // 1. Gérer la visibilité
    if (!visible) {
        lv_obj_add_flag(label_recovery_time, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    lv_obj_clear_flag(label_recovery_time, LV_OBJ_FLAG_HIDDEN);
    
    // 2. Formater le texte (M:SS)
    uint32_t m = seconds / 60;
    uint32_t s = seconds % 60;
    lv_label_set_text_fmt(label_recovery_time, "%lu:%02lu", m, s);
    
    // 3. Gérer la couleur
    if (is_red) {
        lv_obj_set_style_text_color(label_recovery_time, lv_color_hex(0xFF0000), 0); // Rouge
    } else {
        lv_obj_set_style_text_color(label_recovery_time, lv_color_hex(0x00FF00), 0); // Vert
    }
}



void ui_refresh_last_depth(float depth) {
    char buf[16];
    // On utilise snprintf comme pour la profondeur actuelle pour éviter le "f"
    snprintf(buf, sizeof(buf), "%.1f", depth);
    lv_label_set_text(label_max_val, buf);
}

void ui_refresh_stats(uint32_t max_time_s, float max_depth, uint32_t total_dives) {
    char buf[16];
    
    // 1. Max Time (Format M:SS)
    uint32_t m = max_time_s / 60;
    uint32_t s = max_time_s % 60;
    lv_label_set_text_fmt(label_stat_divetime_val, "%lu:%02lu", m, s);
    
    // 2. Max Depth (On utilise snprintf pour éviter le fameux "f")
    snprintf(buf, sizeof(buf), "%.1f", max_depth);
    lv_label_set_text(label_stat_max_val, buf);
    lv_obj_align_to(label_stat_max_val, label_stat_max_unit, LV_ALIGN_OUT_LEFT_BOTTOM, -2, 2);

    // 3. Compteur total de plongées
    lv_label_set_text_fmt(label_stat_dives_val, "%lu", total_dives);
}