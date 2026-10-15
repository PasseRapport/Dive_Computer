#include "ui_layout.h"
#include <stdio.h>

LV_IMG_DECLARE(turn_fish);
LV_IMG_DECLARE(fish); 
LV_IMG_DECLARE(sanic); 
LV_IMG_DECLARE(icon_sun);
LV_IMG_DECLARE(icon_drop);
LV_IMG_DECLARE(icon_wave);

// --- Variables Globales ---
lv_obj_t * boot_screen;
lv_obj_t * app_screen; // <--- NOUVEAU : La toile globale
lv_obj_t * main_screen;
lv_obj_t * water_screen;
lv_obj_t * stats_screen;
lv_obj_t * brightness_screen;
lv_obj_t * sleep_screen;
static lv_obj_t * brightness_bar; 

// --- Pointeurs internes pour la mise à jour ---
static lv_obj_t * label_time_val;
static lv_obj_t * label_recovery_time;
static lv_obj_t * label_temp_val;
static lv_obj_t * label_depth_val;
static lv_obj_t * label_depth_unit;
static lv_obj_t * label_max_val;
static lv_obj_t * label_clear;
static lv_obj_t * label_salt;
static lv_obj_t * cursor_box;
static lv_obj_t * img_clear;
static lv_obj_t * img_salt;
static lv_obj_t * run_h_val;
static lv_obj_t * run_h_unit;
static lv_obj_t * run_m_val;
static lv_obj_t * run_m_unit;
static lv_obj_t * label_stat_divetime_val;
static lv_obj_t * label_stat_max_val;
static lv_obj_t * label_stat_max_unit;
static lv_obj_t * label_stat_dives_val;
static lv_obj_t * label_battery_val;

// --- Fonction utilitaire pour formater en page (128x96) ---
static void format_as_page(lv_obj_t * page) {
    lv_obj_set_size(page, 128, 96); 
    lv_obj_set_style_bg_color(page, lv_color_black(), 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE); 
    
    // On cache la page à droite (128 pixels) par défaut
    lv_obj_set_x(page, 128); 
}

// --- Callbacks d'Animation de Démarrage ---
static void boot_fade_in_cb(lv_timer_t * timer) {
    // On charge la toile globale (app_screen) qui contient toutes les pages
    lv_scr_load_anim(app_screen, LV_SCR_LOAD_ANIM_FADE_ON, 800, 0, true);
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
    
    // Force le fond de l'écran matériel en noir absolu
    lv_disp_set_bg_color(lv_disp_get_default(), lv_color_black());

    // 1. CRÉER LA TOILE GLOBALE
    app_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_screen, lv_color_black(), 0);
    lv_obj_clear_flag(app_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ----------------------------------------CREATE BOOT SCREEN ------------------------------------
    boot_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(boot_screen, lv_color_black(), 0);
    
    lv_obj_t * logo = lv_gif_create(boot_screen);
    lv_gif_set_src(logo, &turn_fish); 
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, 0);

    // ---------------------------------------- CREATE MAIN SCREEN ------------------------------------
    main_screen = lv_obj_create(app_screen); // Attaché à la toile
    format_as_page(main_screen);
    lv_obj_set_x(main_screen, 0); // Visible au centre au démarrage
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
    water_screen = lv_obj_create(app_screen);
    format_as_page(water_screen);

    // --- NOUVEAU : TITRE ---
    lv_obj_t * label_water_title = lv_label_create(water_screen);
    lv_label_set_text(label_water_title, "WATER TYPE");
    lv_obj_set_style_text_font(label_water_title, &lv_font_montserrat_12, 0); 
    lv_obj_set_style_text_color(label_water_title, lv_color_hex(0x888888), 0);
    lv_obj_align(label_water_title, LV_ALIGN_TOP_MID, 0, 0); // Placé tout en haut

    // 1. LE CURSEUR GLISSANT (Créé en premier pour être en arrière-plan)
    cursor_box = lv_obj_create(water_screen);
    lv_obj_set_size(cursor_box, 116, 30); // Taille de la boîte de sélection
    lv_obj_set_style_radius(cursor_box, 15, 0); // Bords parfaitement arrondis (moitié de la hauteur: 30/2)
    lv_obj_set_style_bg_color(cursor_box, lv_color_hex(0x008B8B), 0); // Fond Cyan
    lv_obj_set_style_border_width(cursor_box, 2, 0);
    lv_obj_set_style_border_color(cursor_box, lv_color_hex(0xFFFFFF), 0); // Bordure pour faire ressortir
    lv_obj_clear_flag(cursor_box, LV_OBJ_FLAG_SCROLLABLE);
    // Nouvelle position initiale (Sur Clear Water, décalé vers le bas : Y = 18)
    lv_obj_align(cursor_box, LV_ALIGN_TOP_MID, 0, 18); 


    // 2. OPTION 1 : CLEAR WATER (Par-dessus le curseur)
    img_clear = lv_img_create(water_screen);
    lv_img_set_src(img_clear, &icon_drop); 
    lv_obj_align(img_clear, LV_ALIGN_TOP_LEFT, 15, 20); // Ajusté (+2 par rapport à la box)

    label_clear = lv_label_create(water_screen);
    lv_label_set_text(label_clear, "FRESH");
    lv_obj_set_style_text_font(label_clear, &lv_font_montserrat_16, 0); 
    lv_obj_align(label_clear, LV_ALIGN_TOP_LEFT, 45, 25); // Ajusté (+7 par rapport à la box pour centrage)


    // 3. OPTION 2 : SALT WATER
    img_salt = lv_img_create(water_screen);
    lv_img_set_src(img_salt, &icon_wave); 
    // Rapproché de 5px et descendu de 3px : Ancien 58 -> Nouveau 56
    lv_obj_align(img_salt, LV_ALIGN_TOP_LEFT, 15, 56); 

    label_salt = lv_label_create(water_screen);
    lv_label_set_text(label_salt, "MARINE");
    lv_obj_set_style_text_font(label_salt, &lv_font_montserrat_16, 0);
    // Alignement parfait par rapport à l'icône
    lv_obj_align(label_salt, LV_ALIGN_TOP_LEFT, 45, 61);


   // ---------------------------------------- CREATE STATS SCREEN ------------------------------------
    stats_screen = lv_obj_create(app_screen);
    format_as_page(stats_screen);

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
    lv_obj_align(label_stat_max_title, LV_ALIGN_TOP_RIGHT, 0, 0); 

    // 1. On crée le "m" d'abord
    label_stat_max_unit = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_max_unit, "m");
    lv_obj_set_style_text_font(label_stat_max_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_stat_max_unit, lv_color_white(), 0);
    lv_obj_align_to(label_stat_max_unit, label_stat_max_title, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 7);

    // 2. On crée la valeur
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
    brightness_screen = lv_obj_create(app_screen);
    format_as_page(brightness_screen);

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

    // ---------------------------------------- CREATE SLEEP SCREEN ------------------------------------
    sleep_screen = lv_obj_create(app_screen);
    format_as_page(sleep_screen);

    // --- TEXTE PRINCIPAL (SLEEP) ---
    lv_obj_t * label_sleep_title = lv_label_create(sleep_screen);
    lv_label_set_text(label_sleep_title, "SLEEP");
    
    // Si tu as une police plus grande (ex: 16, 20 ou 24), utilise-la ici pour que ce soit bien lisible sous l'eau
    lv_obj_set_style_text_font(label_sleep_title, &lv_font_montserrat_12, 0); 
    lv_obj_set_style_text_color(label_sleep_title, lv_color_hex(0xFFFFFF), 0); // Blanc pour bien ressortir
    
    // On le place bien au centre de l'écran (légèrement remonté pour laisser la place au sous-titre)
    lv_obj_align(label_sleep_title, LV_ALIGN_CENTER, 0, -10); 

    // --- SOUS-TITRE (Indication d'action) ---
    lv_obj_t * label_sleep_hint = lv_label_create(sleep_screen);
    lv_label_set_text(label_sleep_hint, "Tap to power off");
    lv_obj_set_style_text_font(label_sleep_hint, &lv_font_montserrat_12, 0); 
    lv_obj_set_style_text_color(label_sleep_hint, lv_color_hex(0x888888), 0); // Gris clair
    
    // Placé juste en dessous du titre principal
    lv_obj_align(label_sleep_hint, LV_ALIGN_CENTER, 0, 8);

    // --- BATTERIE ---
    label_battery_val = lv_label_create(sleep_screen);
    lv_label_set_text(label_battery_val, "BAT: 100%");
    lv_obj_set_style_text_font(label_battery_val, &lv_font_montserrat_12, 0); 
    lv_obj_set_style_text_color(label_battery_val, lv_color_hex(0x00FF00), 0);
    lv_obj_align(label_battery_val, LV_ALIGN_CENTER, 0, 24);

    // ---------------------------------------- START THE SEQUENCE ------------------------------------
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_scr_load_anim(boot_screen, LV_SCR_LOAD_ANIM_FADE_ON, 800, 0, false);
    
    lv_timer_create(boot_fade_out_cb, 3500, NULL);
}

void ui_refresh_battery(uint8_t percentage) {
    lv_label_set_text_fmt(label_battery_val, "BAT: %d%%", percentage);
    if (percentage <= 20) {
        lv_obj_set_style_text_color(label_battery_val, lv_color_hex(0xFF0000), 0); // Red
    } else {
        lv_obj_set_style_text_color(label_battery_val, lv_color_hex(0x00FF00), 0); // Green
    }
}

// =========================================================================
// === FONCTIONS D'ACCÈS POUR LE CONTRÔLEUR (Setters) ======================
// =========================================================================

void ui_refresh_runtime_display(uint32_t h, uint32_t m) {
    lv_label_set_text_fmt(run_h_val, "%lu", h);
    lv_label_set_text_fmt(run_m_val, "%02lu", m);
    lv_obj_align_to(run_h_unit, run_h_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 1, -2);
    lv_obj_align_to(run_m_val, run_h_unit, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 2);
    lv_obj_align_to(run_m_unit, run_m_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 1, -2);
}

void ui_refresh_brightness_bar(uint8_t level) {
    if(level > 5) level = 5;
    lv_bar_set_value(brightness_bar, level, LV_ANIM_ON);
}

void ui_refresh_water_selection(bool is_salt) {
    // 1. Couleurs simples (sans inversion)
    lv_color_t color_active = lv_color_white();           // Blanc pur quand sélectionné
    lv_color_t color_inactive = lv_color_hex(0x555555);   // Gris foncé quand non sélectionné

    // 2. Position Y cible du curseur
    int target_y = is_salt ? 54 : 18; 

    // 3. Lancer l'animation fluide du curseur
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, cursor_box);
    lv_anim_set_values(&a, lv_obj_get_y(cursor_box), target_y);
    lv_anim_set_time(&a, 250); 
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_start(&a);

    // 4. Appliquer les couleurs (Actif = Blanc, Inactif = Gris)
    if (is_salt) {
        // Clear s'éteint
        lv_obj_set_style_text_color(label_clear, color_inactive, 0);
        lv_obj_set_style_img_recolor(img_clear, color_inactive, 0);
        lv_obj_set_style_img_recolor_opa(img_clear, 255, 0);

        // Salt s'allume
        lv_obj_set_style_text_color(label_salt, color_active, 0);
        lv_obj_set_style_img_recolor(img_salt, color_active, 0);
        lv_obj_set_style_img_recolor_opa(img_salt, 255, 0);
    } else {
        // Clear s'allume
        lv_obj_set_style_text_color(label_clear, color_active, 0);
        lv_obj_set_style_img_recolor(img_clear, color_active, 0);
        lv_obj_set_style_img_recolor_opa(img_clear, 255, 0);

        // Salt s'éteint
        lv_obj_set_style_text_color(label_salt, color_inactive, 0);
        lv_obj_set_style_img_recolor(img_salt, color_inactive, 0);
        lv_obj_set_style_img_recolor_opa(img_salt, 255, 0);
    }
}

void ui_refresh_dive_time(uint32_t total_secs) {
    uint32_t m = total_secs / 60;
    uint32_t s = total_secs % 60;
    lv_label_set_text_fmt(label_time_val, "%lu:%02lu", m, s);
}

void ui_refresh_depth(float depth) {
    char buf[16]; 
    snprintf(buf, sizeof(buf), "%.1f", depth); 
    lv_label_set_text(label_depth_val, buf); 
    lv_obj_align_to(label_depth_unit, label_depth_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);
}

void ui_refresh_temp(float temp) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f°C", temp); 
    lv_label_set_text(label_temp_val, buf);
}

void ui_update_recovery_display(uint32_t seconds, bool visible, bool is_red) {
    if (!visible) {
        lv_obj_add_flag(label_recovery_time, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    lv_obj_clear_flag(label_recovery_time, LV_OBJ_FLAG_HIDDEN);
    uint32_t m = seconds / 60;
    uint32_t s = seconds % 60;
    lv_label_set_text_fmt(label_recovery_time, "%lu:%02lu", m, s);
    
    if (is_red) {
        lv_obj_set_style_text_color(label_recovery_time, lv_color_hex(0xFF0000), 0);
    } else {
        lv_obj_set_style_text_color(label_recovery_time, lv_color_hex(0x00FF00), 0);
    }
}

void ui_refresh_last_depth(float depth) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", depth);
    lv_label_set_text(label_max_val, buf);
}

void ui_refresh_stats(uint32_t max_time_s, float max_depth, uint32_t total_dives) {
    char buf[16];
    
    uint32_t m = max_time_s / 60;
    uint32_t s = max_time_s % 60;
    lv_label_set_text_fmt(label_stat_divetime_val, "%lu:%02lu", m, s);
    
    snprintf(buf, sizeof(buf), "%.1f", max_depth);
    lv_label_set_text(label_stat_max_val, buf);
    lv_obj_align_to(label_stat_max_val, label_stat_max_unit, LV_ALIGN_OUT_LEFT_BOTTOM, -2, 2);

    lv_label_set_text_fmt(label_stat_dives_val, "%lu", total_dives);
}

// =========================================================================
// === ANIMATION DE TRANSITION (Poussée Fluide avec Espace) ================
// =========================================================================

void ui_slide_pages(lv_obj_t * page_out, lv_obj_t * page_in) {
    int16_t screen_width = 128; // Ta résolution
    int16_t gap = 10;           // L'espace noir entre les écrans
    int16_t offset = screen_width + gap; // Distance totale à parcourir (138)
    
    // 1. Placer la nouvelle page à droite, décalée de 10 pixels supplémentaires
    lv_obj_set_x(page_in, offset);
    
    // 2. Animation de SORTIE pour la page actuelle (de 0 vers -138)
    lv_anim_t a_out;
    lv_anim_init(&a_out);
    lv_anim_set_var(&a_out, page_out);
    lv_anim_set_values(&a_out, 0, -offset);
    lv_anim_set_time(&a_out, 600);
    lv_anim_set_path_cb(&a_out, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a_out, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&a_out);
    
    // 3. Animation d'ENTRÉE pour la nouvelle page (de 138 vers 0)
    lv_anim_t a_in;
    lv_anim_init(&a_in);
    lv_anim_set_var(&a_in, page_in);
    lv_anim_set_values(&a_in, offset, 0); // Elle s'arrête bien à 0, parfaitement centrée
    lv_anim_set_time(&a_in, 600);
    lv_anim_set_path_cb(&a_in, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a_in, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&a_in);
}