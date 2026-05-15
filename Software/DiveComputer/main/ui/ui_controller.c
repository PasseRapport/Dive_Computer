#include "ui_controller.h"
#include "ui_layout.h"
#include "ui_model.h"
#include "oled_display.h"
#include "esp_timer.h"

// Instanciation de la variable globale du Modèle avec les nouveaux champs
dive_state_t system_state = {
    .current_depth = 0.0f,
    .max_depth = 0.0f,
    .session_max_depth = 0.0f,
    .temperature = 15.0f,
    .max_dive_time_s = 0,
    .total_dives = 0,
    .is_salt_water = false,
    .brightness_lvl = 3,
    .has_dived_once = false, // Initialement faux
    .last_dive_time_s = 0
};

// --- Logique du Chronomètre Run Time ---
static void update_runtime_cb(lv_timer_t * timer) {
    int64_t uptime_us = esp_timer_get_time();
    uint32_t total_mins = uptime_us / (1000000ULL * 60);
    uint32_t h = total_mins / 60;
    uint32_t m = total_mins % 60;
    ui_refresh_runtime_display(h, m);
}

// --- Logique de Détection de Plongée (Apnée) ---
static void dive_logic_task(lv_timer_t * timer) {
    
    // Mise à jour constante de la profondeur et température
    ui_refresh_depth(system_state.current_depth);
    ui_refresh_temp(system_state.temperature);

    // 1. DÉTECTION DÉBUT DE PLONGÉE
    if (!system_state.is_diving && system_state.current_depth > 0.5f) {
        system_state.is_diving = true;
        system_state.dive_start_us = esp_timer_get_time();
        system_state.total_dives++;
        system_state.max_depth = 0.0f;
        // On cache le recovery time pendant qu'on plonge
        ui_update_recovery_display(0, false, false);
    }

    // 2. PHASE DE PLONGÉE
    if (system_state.is_diving) {
        int64_t now = esp_timer_get_time();
        system_state.current_dive_s = (uint32_t)((now - system_state.dive_start_us) / 1000000ULL);
        
        ui_refresh_dive_time(system_state.current_dive_s);

        if (system_state.current_depth > system_state.max_depth) {
            system_state.max_depth = system_state.current_depth;
        }

        
        if (system_state.current_depth > system_state.session_max_depth) {
            system_state.session_max_depth = system_state.current_depth;
        }
        // DÉTECTION FIN DE PLONGÉE (SORTIE D'EAU)
        if (system_state.current_depth < 0.3f) {
            system_state.is_diving = false;
            system_state.has_dived_once = true; // On active le flag pour le recovery
            system_state.last_dive_time_s = system_state.current_dive_s; // On mémorise la durée
            system_state.surface_start_us = esp_timer_get_time(); // On lance le chrono de surface
            ui_refresh_last_depth(system_state.max_depth); // Met à jour le "LAST" avec la profondeur max
            ui_refresh_stats(system_state.max_dive_time_s, system_state.max_depth, system_state.total_dives);
            
            if (system_state.current_dive_s > system_state.max_dive_time_s) {
                system_state.max_dive_time_s = system_state.current_dive_s;
            }

            ui_refresh_last_depth(system_state.max_depth);
            ui_refresh_stats(system_state.max_dive_time_s, system_state.session_max_depth, system_state.total_dives);
            
        }
    } 
    // 3. PHASE DE SURFACE (RECOVERY TIME)
    else if (system_state.has_dived_once) {
        int64_t now = esp_timer_get_time();
        uint32_t recovery_s = (uint32_t)((now - system_state.surface_start_us) / 1000000ULL);
        
        // Gestion de la visibilité (disparaît après 9min59s / 600s)
        if (recovery_s >= 600) {
            ui_update_recovery_display(recovery_s, false, false);
        } else {
            // Logique de couleur : Rouge si < 2 * temps de plongée, sinon Vert
            bool is_red = (recovery_s < (system_state.last_dive_time_s * 2));
            ui_update_recovery_display(recovery_s, true, is_red);
        }
    }
}

void init_ui_controllers(void) {
    lv_timer_create(update_runtime_cb, 60000, NULL);
    lv_timer_create(dive_logic_task, 100, NULL);
    update_runtime_cb(NULL);
}

// --- Logique de la Luminosité ---
void handle_brightness_change(uint8_t new_level) {
    if (new_level > 5) new_level = 5;
    system_state.brightness_lvl = new_level;
    ui_refresh_brightness_bar(new_level);
    uint8_t hw_level = new_level * 3; 
    set_display_brightness(hw_level);
}