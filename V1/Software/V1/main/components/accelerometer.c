#include "accelerometer.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_rom_sys.h" // <-- Indispensable pour le chronométrage ultra-précis
#include <stdlib.h> // Pour abs()

extern void send_tap_event(char axis, bool is_positive);

static const char *TAG = "SMART_TAP";

#define LIS2DUX_ADDR        0x19 
#define I2C_MASTER_NUM      0    
#define I2C_MASTER_TIMEOUT_MS 100
#define ACCEL_INT1_PIN      GPIO_NUM_0

static QueueHandle_t tap_event_queue = NULL;

// --- Fonctions I2C ---
static esp_err_t accel_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, LIS2DUX_ADDR, write_buf, 2, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

esp_err_t get_acceleration(int16_t *x, int16_t *y, int16_t *z) {
    uint8_t raw_data[6];
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, LIS2DUX_ADDR, (uint8_t[]){0x28}, 1, raw_data, 6, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    if (ret == ESP_OK) {
        *x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
        *y = (int16_t)((raw_data[3] << 8) | raw_data[2]);
        *z = (int16_t)((raw_data[5] << 8) | raw_data[4]);
    }
    return ret;
}

// --- Interruptions ---
static void IRAM_ATTR accel_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(tap_event_queue, &gpio_num, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// --- L'Analyse Post-Choc (La Ruse logicielle) ---
static void tap_analyzer_task(void* arg) {
    uint32_t io_num;
    
    // On stocke 100 échantillons (soit ~100 millisecondes de capture)
    const int SAMPLES = 100; 

    while(1) {
        // 1. On attend le réveil matériel
        if(xQueueReceive(tap_event_queue, &io_num, portMAX_DELAY)) {
            
            int16_t x, y, z;
            int16_t max_x = -32000, max_y = -32000, max_z = -32000;
            int16_t min_x = 32000,  min_y = 32000,  min_z = 32000;

            // 2. L'ESP32 mitraille l'I2C pour capturer l'onde de choc (Le "Burst")
            for (int i = 0; i < SAMPLES; i++) {
                if (get_acceleration(&x, &y, &z) == ESP_OK) {
                    if (x > max_x) max_x = x;
                    if (x < min_x) min_x = x;
                    if (y > max_y) max_y = y;
                    if (y < min_y) min_y = y;
                    if (z > max_z) max_z = z;
                    if (z < min_z) min_z = z;
                }
                // <-- CORRECTION ICI : Pause matérielle garantie de 1 milliseconde
                esp_rom_delay_us(100); 
            }

            // 3. Calcul de l'amplitude
            int32_t delta_x = abs(max_x - min_x);
            int32_t delta_y = abs(max_y - min_y);
            int32_t delta_z = abs(max_z - min_z);

            // 4. Trouve l'axe vainqueur
            int32_t max_delta = delta_x;
            char winning_axis = 'X';
            if (delta_y > max_delta) { max_delta = delta_y; winning_axis = 'Y'; }
            if (delta_z > max_delta) { max_delta = delta_z; winning_axis = 'Z'; }

            // <-- CORRECTION ICI : Filtre anti-bruit pour les micro-vibrations
            // (Si l'amplitude est inférieure à 3000 unités, on annule)
            if (max_delta < 2000) {
                ESP_LOGW(TAG, "Mouvement ignoré (Trop faible: %ld)", max_delta);
            } 
            else {
                // 5. Détermination du signe
                bool is_positive = false;
                if (winning_axis == 'X') is_positive = (abs(max_x) > abs(min_x));
                if (winning_axis == 'Y') is_positive = (abs(max_y) > abs(min_y));
                if (winning_axis == 'Z') is_positive = (abs(max_z) > abs(min_z));

                ESP_LOGI(TAG, "💥 CHOC ANALYSÉ : Axe %c | Sens : %s | Force : %ld", 
                         winning_axis, is_positive ? "POSITIF (+)" : "NÉGATIF (-)", max_delta);

                // <-- CORRECTION ICI : Redirection de l'axe Y vers l'axe X
                // Absorbe les erreurs de torsion sur la table
                if (winning_axis == 'Z') {
                    ESP_LOGW(TAG, "Axe Z détecté -> Redirigé vers l'axe Y");
                    winning_axis = 'Y'; 
                }

                send_tap_event(winning_axis, is_positive);
            }

            // 6. On vide la file d'attente et on attend avant le prochain coup
            xQueueReset(tap_event_queue);
            vTaskDelay(pdMS_TO_TICKS(200)); 
        }
    }
}

// --- Initialisation ---
void init_accelerometer(void) {
    ESP_LOGI(TAG, "Démarrage du Moteur Hybride (Wake-Up + Analyse Logicielle)...");

    tap_event_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(tap_analyzer_task, "tap_analyzer", 4096, NULL, 10, NULL);

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .pin_bit_mask = (1ULL << ACCEL_INT1_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf);
    
    gpio_install_isr_service(0); 
    gpio_isr_handler_add(ACCEL_INT1_PIN, accel_isr_handler, (void*) ACCEL_INT1_PIN);

    // 1. Reset
    accel_write_reg(0x10, 0x20); 
    vTaskDelay(pdMS_TO_TICKS(100));

    // 2. BDU (Block Data Update) activé
    accel_write_reg(0x13, 0x20);

    // 3. Allumer à très haute fréquence ! ODR 800 Hz, ±8g (Ton réglage 0xB2)
    accel_write_reg(0x14, 0xB2); 

    // 4. Configuration du Tap de base (sonnette d'alarme)
    accel_write_reg(0x6F, 0xE0); // Activer Z, Y, X
    accel_write_reg(0x70, 0x10); // Pré-silence très bas
    accel_write_reg(0x71, 0x2C); // Max Time à 30ms et post-still-thresh à 2
    accel_write_reg(0x72, 0x47); // Post-silence très bas
    
    // Le seuil de déclenchement : très bas (0x02) pour réveiller l'ESP32 au moindre choc
    accel_write_reg(0x73, 0x0D); 
    
    accel_write_reg(0x74, 0x60); // Single Tap activé
    accel_write_reg(0x17, 0x01); // Interruptions globales activées
    accel_write_reg(0x1F, 0x08); // Route vers INT1

    ESP_LOGI(TAG, "Système d'analyse en attente d'impact !");
    
}