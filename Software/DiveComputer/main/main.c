#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "oled_display.h"
#include "ui_layout.h"
#include "ui_controller.h"
#include "ui_model.h"          // Ajouté pour accéder au type dive_state_t
#include "pressure_sensor.h" 
#include "accelerometer.h"
#include "esp_sleep.h"
#include "battery.h"

static const char *TAG = "DiveComputer";

// Permet à main.c d'accéder à la variable d'état globale définie dans ui_controller.c
extern dive_state_t system_state;

int16_t acc_x, acc_y, acc_z;

// --- Variables d'état de l'interface ---
typedef enum {
    SCREEN_MAIN,
    SCREEN_STATS,
    SCREEN_WATER_TYPE,
    SCREEN_BRIGHTNESS,
    SCREEN_SLEEP
} app_screen_t;

// --- Système d'événements des Taps ---
typedef enum {
    TAP_NONE = 0,
    TAP_LEFT,   // X+
    TAP_RIGHT,  // X-
    TAP_ACTION  // Z-
} tap_event_t;

// Variable globale pour stocker l'événement en attente
volatile tap_event_t current_tap_event = TAP_NONE;

// Nouvelle fonction de communication (Appelée par accelerometer.c)
void send_tap_event(char axis, bool is_positive) {
    if (axis == 'X' && is_positive) {
        current_tap_event = TAP_LEFT;
    } else if (axis == 'X' && !is_positive) {
        current_tap_event = TAP_RIGHT;
    } else if (axis == 'Y' && !is_positive) {
        current_tap_event = TAP_ACTION;
    }
}


void enter_deep_sleep() {
    ESP_LOGI(TAG, "Mise en veille profonde imminente...");

    // 1. Éteindre l'écran OLED
    oled_display_power_off();

    gpio_reset_pin(GPIO_NUM_8);
    gpio_reset_pin(GPIO_NUM_9);

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 2. Dire à l'ESP32 quelle broche va le réveiller.
   esp_deep_sleep_enable_gpio_wakeup((1ULL << GPIO_NUM_0), ESP_GPIO_WAKEUP_GPIO_HIGH);

    ESP_LOGI(TAG, "Bonne nuit ! 💤");

    // 3. Coupure du processeur
    esp_deep_sleep_start();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Booting Dive Computer...");

    // 1. Initialisation matérielle et UI
    init_oled_display();
    set_display_brightness(15);
    build_ui_state_machine();
    init_ui_controllers();

    // 2. Initialisation des capteurs
    init_dive_sensor();
    calibrate_surface_pressure();
    init_battery_adc();
    
    // Initialise et lance l'écoute des Taps (FSM / Smart Tap)
    init_accelerometer(); 

    // Lance la lecture de la pression en tâche de fond (Core 1)
    xTaskCreatePinnedToCore(dive_sensor_task, "SensorTask", 4096, NULL, 5, NULL, 1);

    // 3. Variables pour la machine d'état des écrans
    app_screen_t current_screen = SCREEN_MAIN; 

    // 4. Boucle Principale
    while(1) {
        
        // Si un événement Tap a été reçu
        if (current_tap_event != TAP_NONE) {
            
            // On sauvegarde l'événement et on libère le drapeau immédiatement
            tap_event_t action = current_tap_event;
            current_tap_event = TAP_NONE; 
            
           
            // --- ACTION : DEFILER VERS LA DROITE ---
            if (action == TAP_RIGHT) {
                ESP_LOGI(TAG, "👉 Navigation Droite");
                if (current_screen == SCREEN_MAIN) {
                    ui_slide_pages(main_screen, stats_screen);
                    current_screen = SCREEN_STATS;
                } else if (current_screen == SCREEN_STATS) {
                    ui_slide_pages(stats_screen, water_screen);
                    current_screen = SCREEN_WATER_TYPE;
                } else if (current_screen == SCREEN_WATER_TYPE) {
                    ui_slide_pages(water_screen, brightness_screen);
                    current_screen = SCREEN_BRIGHTNESS;
                } else if (current_screen == SCREEN_BRIGHTNESS) {
                    // 👉 On va vers l'écran Sleep
                    ui_slide_pages(brightness_screen, sleep_screen); 
                    current_screen = SCREEN_SLEEP;
                } else if (current_screen == SCREEN_SLEEP) {
                    // 👉 On boucle vers le Main
                    ui_slide_pages(sleep_screen, main_screen); 
                    current_screen = SCREEN_MAIN;
                }
            } 
            // --- ACTION : DEFILER VERS LA GAUCHE ---
            else if (action == TAP_LEFT) {
                ESP_LOGI(TAG, "👈 Navigation Gauche");
                if (current_screen == SCREEN_MAIN) {
                    // 👉 On va vers l'écran Sleep à l'envers
                    ui_slide_pages(main_screen, sleep_screen);
                    current_screen = SCREEN_SLEEP;
                } else if (current_screen == SCREEN_SLEEP) {
                    // 👉 On recule vers Brightness
                    ui_slide_pages(sleep_screen, brightness_screen);
                    current_screen = SCREEN_BRIGHTNESS;
                } else if (current_screen == SCREEN_BRIGHTNESS) {
                    ui_slide_pages(brightness_screen, water_screen);
                    current_screen = SCREEN_WATER_TYPE;
                } else if (current_screen == SCREEN_WATER_TYPE) {
                    ui_slide_pages(water_screen, stats_screen);
                    current_screen = SCREEN_STATS;
                } else if (current_screen == SCREEN_STATS) {
                    ui_slide_pages(stats_screen, main_screen);
                    current_screen = SCREEN_MAIN;
                }
            }
            // --- ACTION : VALIDER / CHANGER ---
            else if (action == TAP_ACTION) {
                ESP_LOGI(TAG, "👇 Action de Validation");
                
                if (current_screen == SCREEN_SLEEP) {
                    // 👉 LE DÉCLENCHEUR DU DEEP SLEEP
                    enter_deep_sleep();
                    
                } else if (current_screen == SCREEN_WATER_TYPE) {
                    ESP_LOGI(TAG, "Changement du type d'eau !");
                    system_state.is_salt_water = !system_state.is_salt_water;
                    ui_refresh_water_selection(system_state.is_salt_water);
                    
                } else if (current_screen == SCREEN_BRIGHTNESS) {
                    ESP_LOGI(TAG, "Changement de luminosité !");
                    uint8_t new_lvl = system_state.brightness_lvl + 1;
                    if (new_lvl > 5) new_lvl = 1;
                    handle_brightness_change(new_lvl);
                    
                } else {
                    ESP_LOGI(TAG, "Rien à valider sur cet écran.");
                }
            }
        }

        // --- Optionnel : Lecture continue pour Teleplot (commenté si non utilisé) ---
        /*
        if (get_acceleration(&acc_x, &acc_y, &acc_z) == ESP_OK) {
            //printf(">AccX_g:%.3f\n", acc_x * 0.000244f);
            //printf(">AccY_g:%.3f\n", acc_y * 0.000244f);
            //printf(">AccZ_g:%.3f\n", acc_z * 0.000244f);
        }
        */

        // Petite pause pour laisser respirer le processeur
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}






