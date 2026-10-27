/* V1 layout rendered at native V2 pixel sizes, retaining Montserrat and colours.
 * Do not scale whole pages: that interpolates text and makes it blurry. */
#include "v1_ui_layout.h"
#include "selection.h"
#include "battery_icon.h"
#include "native_fonts.h"
#include <stdio.h>

LV_IMG_DECLARE(turn_fish);
LV_IMG_DECLARE(fish); 
LV_IMG_DECLARE(sanic); 
LV_IMG_DECLARE(icon_sun);
LV_IMG_DECLARE(icon_drop);
LV_IMG_DECLARE(icon_wave);
LV_IMAGE_DECLARE(icon_tank);
LV_IMAGE_DECLARE(icon_apnea);

// --- Variables Globales ---
lv_obj_t * boot_screen;
lv_obj_t * app_screen; // <--- NOUVEAU : La toile globale
lv_obj_t * main_screen;
lv_obj_t * water_screen;
lv_obj_t * stats_screen;
lv_obj_t * brightness_screen;
lv_obj_t * sleep_screen;
static lv_obj_t * brightness_bar;
static lv_obj_t *dive_mode_cursor;
static lv_obj_t *dive_mode_labels[2], *dive_mode_icons[2];

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
    lv_obj_set_size(page, 224, 168); 
    lv_obj_set_style_bg_color(page, lv_color_black(), 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE); 

    lv_obj_set_pos(page, V1_PAGE_HOME_X + V1_PAGE_TRAVEL, V1_PAGE_HOME_Y);
}

lv_obj_t *ui_create_dive_mode_page(void)
{
    lv_obj_t *page = lv_obj_create(app_screen);
    format_as_page(page);
    /* Native icons plus the unbroken mode names need a wider menu canvas. */
    lv_obj_set_width(page, 280);
    lv_obj_set_x(page, 20 + V1_PAGE_TRAVEL);
    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "MODE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    dive_mode_cursor = lv_obj_create(page);
    lv_obj_remove_style_all(dive_mode_cursor);
    lv_obj_set_size(dive_mode_cursor, 203, 52);
    lv_obj_set_style_radius(dive_mode_cursor, 15, 0);
    lv_obj_set_style_bg_color(dive_mode_cursor, lv_color_hex(0x008b8b), 0);
    lv_obj_set_style_bg_opa(dive_mode_cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dive_mode_cursor, 2, 0);
    lv_obj_set_style_border_color(dive_mode_cursor, lv_color_white(), 0);
    lv_obj_remove_flag(dive_mode_cursor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(dive_mode_cursor, 10, 32);

    static const char *const names[] = {"SCUBADIVING", "FREEDIVING"};
    const lv_image_dsc_t *icons[] = {&icon_tank, &icon_apnea};
    for (int i = 0; i < 2; i++) {
        dive_mode_labels[i] = lv_label_create(page);
        lv_label_set_text(dive_mode_labels[i], names[i]);
        lv_obj_set_style_text_font(dive_mode_labels[i], &lv_font_montserrat_24, 0);
        dive_mode_icons[i] = selection_place_row(dive_mode_labels[i], icons[i], 67 + 63 * i);
        selection_row_color(dive_mode_labels[i], dive_mode_icons[i], i == 0);
    }
    selection_fit(dive_mode_cursor, dive_mode_labels[0], dive_mode_icons[0], false);
    return page;
}

void ui_refresh_dive_mode(bool freediving)
{
    selection_fit(dive_mode_cursor, dive_mode_labels[freediving ? 1 : 0],
                  dive_mode_icons[freediving ? 1 : 0], true);
    for (int i = 0; i < 2; i++) {
        selection_row_color(dive_mode_labels[i], dive_mode_icons[i], i == (int)freediving);
    }
}

/* Boot stays inside the watch, preserving the desktop control panel. */
static lv_timer_t *boot_timer;
void ui_skip_boot(void)
{
    if (boot_timer) { lv_timer_delete(boot_timer); boot_timer = NULL; }
    if (boot_screen) { lv_obj_delete(boot_screen); boot_screen = NULL; }
}
static void boot_finished(lv_timer_t *timer)
{
    (void)timer;
    ui_skip_boot();
}

void v1_ui_build(lv_obj_t *parent) {
    
    // Force le fond de l'écran matériel en noir absolu
    /* Display ownership stays with the simulator. */

    // 1. CRÉER LA TOILE GLOBALE
    app_screen = lv_obj_create(parent);
    lv_obj_remove_style_all(app_screen);
    /* Slides clip at the circular viewport; all page content is native resolution. */
    lv_obj_set_size(app_screen, 320, 300);
    lv_obj_set_pos(app_screen, 0, 0);
    lv_obj_set_style_bg_opa(app_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(app_screen, lv_color_black(), 0);
    lv_obj_clear_flag(app_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ---------------------------------------- CREATE MAIN SCREEN ------------------------------------
    main_screen = lv_obj_create(app_screen); // Attaché à la toile
    format_as_page(main_screen);
    lv_obj_set_x(main_screen, V1_PAGE_HOME_X);
    lv_obj_set_style_text_color(main_screen, lv_color_white(), 0);

    // QUADRANT 1: TOP LEFT (TIME)
    lv_obj_t * label_time_title = lv_label_create(main_screen);
    lv_label_set_text(label_time_title, "TIME");
    lv_obj_set_style_text_font(label_time_title, &lv_font_montserrat_22, 0);       
    lv_obj_set_style_text_color(label_time_title, lv_color_white(), 0);
    lv_obj_align(label_time_title, LV_ALIGN_TOP_LEFT, 28, 2); 

    label_time_val = lv_label_create(main_screen);
    lv_label_set_text(label_time_val, "0:00");
    lv_obj_set_style_text_font(label_time_val, &lv_font_montserrat_42, 0);         
    lv_obj_align_to(label_time_val, label_time_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    // QUADRANT 2: TOP RIGHT (TEMP)
    lv_obj_t * label_temp_title = lv_label_create(main_screen);
    lv_label_set_text(label_temp_title, "TEMP");
    lv_obj_set_style_text_font(label_temp_title, &lv_font_montserrat_22, 0);       
    lv_obj_set_style_text_color(label_temp_title, lv_color_white(), 0);
    lv_obj_align(label_temp_title, LV_ALIGN_TOP_RIGHT, -18, 2);

    label_temp_val = lv_label_create(main_screen);
    lv_label_set_text(label_temp_val, "00°C");
    lv_obj_set_style_text_font(label_temp_val, &lv_font_montserrat_42, 0);         
    lv_obj_align_to(label_temp_val, label_temp_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    // QUADRANT 3: BOTTOM LEFT (DEPTH)
    lv_obj_t * label_depth_title = lv_label_create(main_screen);
    lv_label_set_text(label_depth_title, "DEPTH");
    lv_obj_set_style_text_font(label_depth_title, &lv_font_montserrat_22, 0);      
    lv_obj_set_style_text_color(label_depth_title, lv_color_white(), 0);
    lv_obj_align(label_depth_title, LV_ALIGN_TOP_LEFT, 21, 84);

    label_depth_val = lv_label_create(main_screen);
    lv_label_set_text(label_depth_val, "0.0");                                     
    lv_obj_set_style_text_font(label_depth_val, &watch_montserrat_60, 0);        
    lv_obj_align_to(label_depth_val, label_depth_title, LV_ALIGN_OUT_BOTTOM_MID, 0, -4);

    label_depth_unit = lv_label_create(main_screen);
    lv_label_set_text(label_depth_unit, "m");
    lv_obj_set_style_text_font(label_depth_unit, &lv_font_montserrat_24, 0);       
    lv_obj_align_to(label_depth_unit, label_depth_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -7);

    // QUADRANT 4: BOTTOM RIGHT (LASTDEPTH)
    lv_obj_t * label_max_title = lv_label_create(main_screen);
    lv_label_set_text(label_max_title, "LAST");
    lv_obj_set_style_text_font(label_max_title, &lv_font_montserrat_22, 0);        
    lv_obj_set_style_text_color(label_max_title, lv_color_white(), 0);
    lv_obj_align(label_max_title, LV_ALIGN_BOTTOM_RIGHT, -9, -28);

    label_max_val = lv_label_create(main_screen);
    lv_label_set_text(label_max_val, "0.0");
    lv_obj_set_style_text_font(label_max_val, &lv_font_montserrat_24, 0);          
    lv_obj_align_to(label_max_val, label_max_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);


    // RIGHT CENTER (RECOVERY TIME)
    label_recovery_time = lv_label_create(main_screen);
    lv_label_set_text(label_recovery_time, "");
    lv_obj_set_style_text_font(label_recovery_time, &lv_font_montserrat_28, 0); 
    lv_obj_set_style_text_color(label_recovery_time, lv_color_hex(0x00FF00), 0);         
    lv_obj_align(label_recovery_time, LV_ALIGN_BOTTOM_RIGHT, -26, -56);

   // ---------------------------------------- CREATE WATER TYPE SCREEN ------------------------------------
    water_screen = lv_obj_create(app_screen);
    format_as_page(water_screen);

    // --- NOUVEAU : TITRE ---
    lv_obj_t * label_water_title = lv_label_create(water_screen);
    lv_label_set_text(label_water_title, "WATER TYPE");
    lv_obj_set_style_text_font(label_water_title, &lv_font_montserrat_22, 0); 
    lv_obj_set_style_text_color(label_water_title, lv_color_white(), 0);
    lv_obj_align(label_water_title, LV_ALIGN_TOP_MID, 0, 0); // Placé tout en haut

    // 1. LE CURSEUR GLISSANT (Créé en premier pour être en arrière-plan)
    cursor_box = lv_obj_create(water_screen);
    lv_obj_set_size(cursor_box, 203, 52); // Taille de la boîte de sélection
    lv_obj_set_style_radius(cursor_box, 15, 0); // Bords parfaitement arrondis (moitié de la hauteur: 30/2)
    lv_obj_set_style_bg_color(cursor_box, lv_color_hex(0x008B8B), 0); // Fond Cyan
    lv_obj_set_style_border_width(cursor_box, 2, 0);
    lv_obj_set_style_border_color(cursor_box, lv_color_hex(0xFFFFFF), 0); // Bordure pour faire ressortir
    lv_obj_clear_flag(cursor_box, LV_OBJ_FLAG_SCROLLABLE);
    // Nouvelle position initiale (Sur Clear Water, décalé vers le bas : Y = 18)
    lv_obj_align(cursor_box, LV_ALIGN_TOP_MID, 0, 32); 


    // 2. OPTION 1 : CLEAR WATER (Par-dessus le curseur)
    img_clear = lv_img_create(water_screen);
    lv_img_set_src(img_clear, &icon_drop);
    lv_obj_set_size(img_clear, 44, 44);
    lv_image_set_inner_align(img_clear, LV_IMAGE_ALIGN_CENTER); 
    lv_obj_align(img_clear, LV_ALIGN_TOP_LEFT, 26, 45); // Ajusté (+2 par rapport à la box)

    label_clear = lv_label_create(water_screen);
    lv_label_set_text(label_clear, "FRESH");
    lv_obj_set_style_text_font(label_clear, &lv_font_montserrat_28, 0); 
    lv_obj_align(label_clear, LV_ALIGN_TOP_LEFT, 79, 54); // Ajusté (+7 par rapport à la box pour centrage)


    // 3. OPTION 2 : SALT WATER
    img_salt = lv_img_create(water_screen);
    lv_img_set_src(img_salt, &icon_wave);
    lv_obj_set_size(img_salt, 44, 44);
    lv_image_set_inner_align(img_salt, LV_IMAGE_ALIGN_CENTER); 
    // Rapproché de 5px et descendu de 3px : Ancien 58 -> Nouveau 56
    lv_obj_align(img_salt, LV_ALIGN_TOP_LEFT, 26, 108); 

    label_salt = lv_label_create(water_screen);
    lv_label_set_text(label_salt, "MARINE");
    lv_obj_set_style_text_font(label_salt, &lv_font_montserrat_28, 0);
    // Alignement parfait par rapport à l'icône
    lv_obj_align(label_salt, LV_ALIGN_TOP_LEFT, 79, 117);


   // ---------------------------------------- CREATE STATS SCREEN ------------------------------------
    stats_screen = lv_obj_create(app_screen);
    format_as_page(stats_screen);

    // --- QUADRANT 1: MAX TIME (En haut à gauche) ---
    lv_obj_t * label_stat_divetime_title = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_divetime_title, "MAX TIME");
    lv_obj_set_style_text_font(label_stat_divetime_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_stat_divetime_title, lv_color_white(), 0);
    lv_obj_align(label_stat_divetime_title, LV_ALIGN_TOP_LEFT, 0, 0);

    label_stat_divetime_val = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_divetime_val, "0:00");
    lv_obj_set_style_text_font(label_stat_divetime_val, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(label_stat_divetime_val, lv_color_white(), 0);
    lv_obj_align_to(label_stat_divetime_val, label_stat_divetime_title, LV_ALIGN_OUT_BOTTOM_MID, 0, -4);

    // --- QUADRANT 2: MAX DEPTH (En haut à droite) ---
    lv_obj_t * label_stat_max_title = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_max_title, "MAX DEPTH");
    lv_obj_set_style_text_font(label_stat_max_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_stat_max_title, lv_color_white(), 0);
    lv_obj_align(label_stat_max_title, LV_ALIGN_TOP_RIGHT, 0, 0); 

    // 1. On crée le "m" d'abord
    label_stat_max_unit = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_max_unit, "m");
    lv_obj_set_style_text_font(label_stat_max_unit, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_stat_max_unit, lv_color_white(), 0);
    lv_obj_align_to(label_stat_max_unit, label_stat_max_title, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 12);

    // 2. On crée la valeur
    label_stat_max_val = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_max_val, "0.0");
    lv_obj_set_style_text_font(label_stat_max_val, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(label_stat_max_val, lv_color_white(), 0);
    lv_obj_align_to(label_stat_max_val, label_stat_max_unit, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 4);

    // --- QUADRANT 3: RUN TIME (Désormais en bas à gauche) ---
    lv_obj_t * label_stat_run_title = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_run_title, "RUN TIME");
    lv_obj_set_style_text_font(label_stat_run_title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(label_stat_run_title, lv_color_white(), 0);
    lv_obj_align(label_stat_run_title, LV_ALIGN_TOP_LEFT, 0, 96); 

    run_h_val = lv_label_create(stats_screen);
    lv_label_set_text(run_h_val, "0");
    lv_obj_set_style_text_font(run_h_val, &lv_font_montserrat_42, 0); 
    lv_obj_set_style_text_color(run_h_val, lv_color_white(), 0);
    lv_obj_align_to(run_h_val, label_stat_run_title, LV_ALIGN_OUT_BOTTOM_MID, -40, -4); 

    run_h_unit = lv_label_create(stats_screen);
    lv_label_set_text(run_h_unit, "h");
    lv_obj_set_style_text_font(run_h_unit, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(run_h_unit, lv_color_white(), 0);
    lv_obj_align_to(run_h_unit, run_h_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);

    run_m_val = lv_label_create(stats_screen);
    lv_label_set_text(run_m_val, "00");
    lv_obj_set_style_text_font(run_m_val, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(run_m_val, lv_color_white(), 0);
    lv_obj_align_to(run_m_val, run_h_unit, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, 4);

    run_m_unit = lv_label_create(stats_screen);
    lv_label_set_text(run_m_unit, "min");
    lv_obj_set_style_text_font(run_m_unit, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(run_m_unit, lv_color_white(), 0);
    lv_obj_align_to(run_m_unit, run_m_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);

    // --- QUADRANT 4: DIVES  ---
    lv_obj_t * label_stat_dives_title = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_dives_title, "DIVES");
    lv_obj_set_style_text_font(label_stat_dives_title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(label_stat_dives_title, lv_color_white(), 0);
    lv_obj_align(label_stat_dives_title, LV_ALIGN_TOP_RIGHT, -9, 96);

    label_stat_dives_val = lv_label_create(stats_screen);
    lv_label_set_text(label_stat_dives_val, "0");
    lv_obj_set_style_text_font(label_stat_dives_val, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(label_stat_dives_val, lv_color_white(), 0);
    lv_obj_align_to(label_stat_dives_val, label_stat_dives_title, LV_ALIGN_OUT_BOTTOM_MID, 0, -4);

    // ---------------------------------------- CREATE BRIGHTNESS SCREEN ------------------------------------
    brightness_screen = lv_obj_create(app_screen);
    format_as_page(brightness_screen);

    lv_obj_t * label_bright_title = lv_label_create(brightness_screen);
    lv_label_set_text(label_bright_title, "BRIGHTNESS");
    lv_obj_set_style_text_font(label_bright_title, &lv_font_montserrat_24, 0); 
    lv_obj_set_style_text_color(label_bright_title, lv_color_white(), 0);
    lv_obj_align(label_bright_title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t * label_minus = lv_label_create(brightness_screen);
    lv_label_set_text(label_minus, "-"); 
    lv_obj_set_style_text_font(label_minus, &watch_montserrat_60, 0);
    lv_obj_set_style_text_color(label_minus, lv_color_white(), 0);
    lv_obj_align(label_minus, LV_ALIGN_CENTER, -70, -10); 

    lv_obj_t * img_sun = lv_img_create(brightness_screen);
    lv_img_set_src(img_sun, &icon_sun);
    lv_obj_set_size(img_sun, 56, 56);
    lv_image_set_inner_align(img_sun, LV_IMAGE_ALIGN_STRETCH); 
    lv_obj_align(img_sun, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t * label_plus = lv_label_create(brightness_screen);
    lv_label_set_text(label_plus, "+"); 
    lv_obj_set_style_text_font(label_plus, &watch_montserrat_60, 0);
    lv_obj_set_style_text_color(label_plus, lv_color_white(), 0);
    lv_obj_align(label_plus, LV_ALIGN_CENTER, 70, -10);

    // --- BARRE DE LUMINOSITÉ ---
    brightness_bar = lv_bar_create(brightness_screen);
    lv_obj_set_size(brightness_bar, 175, 21); 
    lv_obj_align(brightness_bar, LV_ALIGN_BOTTOM_MID, 0, -38);
    lv_bar_set_range(brightness_bar, 0, 10);
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
    lv_obj_set_style_text_font(label_sleep_title, &lv_font_montserrat_22, 0); 
    lv_obj_set_style_text_color(label_sleep_title, lv_color_hex(0xFFFFFF), 0); // Blanc pour bien ressortir
    
    // On le place bien au centre de l'écran (légèrement remonté pour laisser la place au sous-titre)
    lv_obj_align(label_sleep_title, LV_ALIGN_CENTER, 0, -18); 

    // --- SOUS-TITRE (Indication d'action) ---
    lv_obj_t * label_sleep_hint = lv_label_create(sleep_screen);
    lv_label_set_text(label_sleep_hint, "Tap to power off");
    lv_obj_set_style_text_font(label_sleep_hint, &lv_font_montserrat_22, 0); 
    lv_obj_set_style_text_color(label_sleep_hint, lv_color_hex(0x888888), 0); // Gris clair
    
    // Placé juste en dessous du titre principal
    lv_obj_align(label_sleep_hint, LV_ALIGN_CENTER, 0, 14);

    // --- BATTERIE ---
    label_battery_val = lv_label_create(sleep_screen);
    lv_label_set_text(label_battery_val, "BAT: 100%");
    lv_obj_set_style_text_font(label_battery_val, &lv_font_montserrat_22, 0); 
    lv_obj_set_style_text_color(label_battery_val, lv_color_hex(0x00FF00), 0);
    lv_obj_align(label_battery_val, LV_ALIGN_CENTER, 0, 42);

    /* V1 animated logo; a crown/tap event skips it immediately. */
    boot_screen = lv_obj_create(app_screen);
    format_as_page(boot_screen);
    lv_obj_set_pos(boot_screen, V1_PAGE_HOME_X, V1_PAGE_HOME_Y);
    lv_obj_t *logo = lv_gif_create(boot_screen);
    lv_gif_set_src(logo, &turn_fish);
    lv_obj_center(logo);
    boot_timer = lv_timer_create(boot_finished, 3500, NULL);
    ui_refresh_water_selection(false);

}

void ui_refresh_battery(uint8_t percentage) {
    lv_label_set_text_fmt(label_battery_val,"BAT: %u%%",percentage);
    lv_obj_set_style_text_color(label_battery_val,lv_color_hex(battery_icon_color(percentage)),0);
}

// =========================================================================
// === FONCTIONS D'ACCÈS POUR LE CONTRÔLEUR (Setters) ======================
// =========================================================================

void ui_refresh_runtime_display(uint32_t h, uint32_t m) {
    lv_label_set_text_fmt(run_h_val, "%lu", (unsigned long)h);
    lv_label_set_text_fmt(run_m_val, "%02lu", (unsigned long)m);
    lv_obj_align_to(run_h_unit, run_h_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);
    lv_obj_align_to(run_m_val, run_h_unit, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, 4);
    lv_obj_align_to(run_m_unit, run_m_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);
}

void ui_refresh_brightness_bar(uint8_t level) {
    if(level > 10) level = 10;
    lv_bar_set_value(brightness_bar, level, LV_ANIM_ON);
}

void ui_refresh_water_selection(bool is_salt) {
    // 1. Couleurs simples (sans inversion)
    lv_color_t color_active = lv_color_white();           // Blanc pur quand sélectionné
    lv_color_t color_inactive = lv_color_hex(0x888888);   // Gris foncé quand non sélectionné

    selection_fit(cursor_box, is_salt ? label_salt : label_clear,
                  is_salt ? img_salt : img_clear, true);

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
    lv_label_set_text_fmt(label_time_val, "%lu:%02lu", (unsigned long)m, (unsigned long)s);
}

void ui_refresh_depth(float depth) {
    char buf[16]; 
    snprintf(buf, sizeof(buf), "%.1f", (double)depth); 
    lv_label_set_text(label_depth_val, buf); 
    lv_obj_align_to(label_depth_unit, label_depth_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -7);
}

void ui_refresh_temp(float temp) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f°C", (double)temp); 
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
    lv_label_set_text_fmt(label_recovery_time, "%lu:%02lu", (unsigned long)m, (unsigned long)s);
    
    if (is_red) {
        lv_obj_set_style_text_color(label_recovery_time, lv_color_hex(0xFF0000), 0);
    } else {
        lv_obj_set_style_text_color(label_recovery_time, lv_color_hex(0x00FF00), 0);
    }
}

void ui_refresh_last_depth(float depth) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", (double)depth);
    lv_label_set_text(label_max_val, buf);
}

void ui_refresh_stats(uint32_t max_time_s, float max_depth, uint32_t total_dives) {
    char buf[16];
    
    uint32_t m = max_time_s / 60;
    uint32_t s = max_time_s % 60;
    lv_label_set_text_fmt(label_stat_divetime_val, "%lu:%02lu", (unsigned long)m, (unsigned long)s);
    
    snprintf(buf, sizeof(buf), "%.1f", (double)max_depth);
    lv_label_set_text(label_stat_max_val, buf);
    lv_obj_align_to(label_stat_max_val, label_stat_max_unit, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 4);

    lv_label_set_text_fmt(label_stat_dives_val, "%lu", (unsigned long)total_dives);
}

// =========================================================================
// === ANIMATION DE TRANSITION (Poussée Fluide avec Espace) ================
// =========================================================================

void ui_slide_pages(lv_obj_t * page_out, lv_obj_t * page_in, int direction) {
    int16_t offset = V1_PAGE_TRAVEL * direction;
    
    // Entrée depuis le côté choisi, au-delà de la zone visible de la montre.
    lv_obj_set_x(page_in, V1_PAGE_HOME_X + offset);
    
    // Sortie depuis la position centrée jusqu'au-delà du bord opposé.
    lv_anim_t a_out;
    lv_anim_init(&a_out);
    lv_anim_set_var(&a_out, page_out);
    lv_anim_set_values(&a_out, V1_PAGE_HOME_X, V1_PAGE_HOME_X - offset);
    lv_anim_set_duration(&a_out, 250);
    lv_anim_set_path_cb(&a_out, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a_out, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&a_out);
    
    // Entrée jusqu'à la position centrée, dans les coordonnées de la V2.
    lv_anim_t a_in;
    lv_anim_init(&a_in);
    lv_anim_set_var(&a_in, page_in);
    lv_anim_set_values(&a_in, V1_PAGE_HOME_X + offset, V1_PAGE_HOME_X);
    lv_anim_set_duration(&a_in, 250);
    lv_anim_set_path_cb(&a_in, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a_in, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&a_in);
}

bool ui_water_selection_within_page(void) {
    lv_obj_update_layout(water_screen);
    lv_area_t box, page;
    lv_obj_get_coords(cursor_box, &box);
    lv_obj_get_coords(water_screen, &page);
    return box.x1 >= page.x1 && box.x2 <= page.x2;
}
