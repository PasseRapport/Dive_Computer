#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "oled_display.h"
#include "ui_layout.h"
#include "ui_controller.h"
#include "sensor_manager.h"

static const char *TAG = "DiveComputer";

// Le bouton "BOOT" sur l'ESP32-C3 est physiquement relié au GPIO 9
#define USER_BUTTON_PIN 9 

// --- Variables d'état de l'interface ---
typedef enum {
    SCREEN_MAIN,
    SCREEN_STATS,
    SCREEN_WATER_TYPE,
    SCREEN_BRIGHTNESS
} app_screen_t;

void app_main(void)
{
    ESP_LOGI(TAG, "Booting Dive Computer...");

    // 1. Configuration du bouton de l'ESP32-C3
    gpio_set_direction(USER_BUTTON_PIN, GPIO_MODE_INPUT);
    // On active la résistance interne. Le bouton mettra la broche à 0 (GND) quand il sera pressé.
    gpio_set_pull_mode(USER_BUTTON_PIN, GPIO_PULLUP_ONLY); 
    

    // 2. Initialisation
    init_oled_display();
    set_display_brightness(15);
    build_ui_state_machine();
    init_ui_controllers();

    init_dive_sensor();
    calibrate_surface_pressure();
    // Lance la lecture du capteur en tâche de fond (Core 1)
    xTaskCreatePinnedToCore(dive_sensor_task, "SensorTask", 4096, NULL, 5, NULL, 1);


    // 3. Variables pour la machine d'état du bouton
    app_screen_t current_screen = SCREEN_MAIN; // On commence sur l'écran principal
    int last_button_state = 1; // 1 = non pressé (à cause du pull-up)


    // 4. Boucle Principale
    while(1) {
        // Lecture de l'état du bouton
        int current_state = gpio_get_level(USER_BUTTON_PIN);
        

        // Si l'état passe de 1 (relâché) à 0 (pressé) = Appui Simple
        if (current_state == 0 && last_button_state == 1) {
            ESP_LOGI(TAG, "Button pressed! Animating to next screen...");

            // Navigation en Carrousel (Boucle infinie entre les 4 écrans)
            if (current_screen == SCREEN_MAIN) {
                // Main -> Stats
                lv_scr_load_anim(stats_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
                current_screen = SCREEN_STATS;
                
            } else if (current_screen == SCREEN_STATS) {
                // Stats -> Eau
                lv_scr_load_anim(water_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
                current_screen = SCREEN_WATER_TYPE;

            } else if (current_screen == SCREEN_WATER_TYPE) {
                // Eau -> Luminosité
                lv_scr_load_anim(brightness_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
                current_screen = SCREEN_BRIGHTNESS;
                
            } else if (current_screen == SCREEN_BRIGHTNESS) {
                // Luminosité -> Retour au Main
                lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
                current_screen = SCREEN_MAIN;
            }

            // Anti-rebond (debounce) : on attend 200ms pour éviter les faux contacts
            vTaskDelay(pdMS_TO_TICKS(200)); 
        }

        // On sauvegarde l'état pour la prochaine boucle
        last_button_state = current_state;

        // Petite pause pour ne pas surcharger le processeur
        vTaskDelay(pdMS_TO_TICKS(20));
        

  
    }
}