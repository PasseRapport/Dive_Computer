#include "sensor_manager.h"
#include "lps28dfw_reg.h"
#include "ui_model.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SENSOR";

// --- Configuration I2C pour ESP32-C3 ---
#define I2C_MASTER_SCL_IO           10   
#define I2C_MASTER_SDA_IO           8   
#define I2C_MASTER_NUM              0    
#define I2C_MASTER_FREQ_HZ          100000 
#define I2C_MASTER_TIMEOUT_MS       1000

#define LPS28DFW_I2C_ADD            0x5C 

// Pression de surface par défaut (à calibrer plus tard)
static float surface_pressure_hPa = 1013.25f; 
static lps28dfw_md_t global_md;

// --- Wrappers I2C pour le driver STMicroelectronics ---
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LPS28DFW_I2C_ADD << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, bufp, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? 0 : -1;
}

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LPS28DFW_I2C_ADD << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LPS28DFW_I2C_ADD << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, bufp, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? 0 : -1;
}

// Contexte du driver ST
static stmdev_ctx_t dev_ctx = {
    .write_reg = platform_write,
    .read_reg = platform_read,
    .handle = NULL
};

// --- Initialisation ---
void init_dive_sensor(void) {

    // 1. Initialisation matérielle du bus I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    // 2. Vérification du capteur 
    lps28dfw_id_t id; // On utilise la structure officielle de ST
    lps28dfw_id_get(&dev_ctx, &id);
    
    // Le vrai ID est lu dans le champ whoami de la structure
    if (id.whoami != LPS28DFW_ID) {
        ESP_LOGE(TAG, "Capteur LPS28DFW introuvable ! (ID lu: %02X)", id.whoami);
        return;
    }
    ESP_LOGI(TAG, "LPS28DFW détecté avec succès.");

    // 3. Configuration du capteur (Mode continu, 10 Hz)
    lps28dfw_md_t md;
    lps28dfw_mode_get(&dev_ctx, &md);
    md.odr = LPS28DFW_10Hz;
    md.avg = LPS28DFW_16_AVG;
    // Ligne problématique supprimée ici
    lps28dfw_mode_set(&dev_ctx, &md);
}


void calibrate_surface_pressure(void) {
    ESP_LOGI(TAG, "Calibration de surface en cours... Gardez l'appareil à l'air libre.");
    
    float sum = 0;
    int samples = 150;
    lps28dfw_data_t data;

    for (int i = 0; i < samples; i++) {
        lps28dfw_data_get(&dev_ctx, &global_md, &data);
        sum += data.pressure.hpa;
        vTaskDelay(pdMS_TO_TICKS(20)); // Attendre un nouveau cycle de mesure
    }

    surface_pressure_hPa = sum / samples;
    ESP_LOGI(TAG, "Pression de surface calibrée : %.2f hPa", surface_pressure_hPa);
}


// --- Tâche de lecture continue ---
void dive_sensor_task(void *pvParameters) {
    lps28dfw_data_t data;
    lps28dfw_md_t md; // CORRIGÉ : On déclare md ici pour la tâche
    
    // On récupère le mode actuel configuré dans l'init
    lps28dfw_mode_get(&dev_ctx, &md);
    
    while (1) {
        // Lecture des données brutes
        lps28dfw_data_get(&dev_ctx, &md, &data);
        
        float pressure_hPa = data.pressure.hpa;
        float temp_celsius = data.heat.deg_c;

        // Choix de la densité de l'eau selon l'UI
        float density = system_state.is_salt_water ? 1025.0f : 1000.0f;
        float gravity = 9.80665f;

        // Calcul de la profondeur
        float delta_P_Pa = (pressure_hPa - surface_pressure_hPa) * 100.0f;
        float depth = delta_P_Pa / (density * gravity);
        
        if (depth < 0.0f) depth = 0.0f; 

        // Mise à jour de notre modèle de données global
        system_state.current_depth = depth;
        system_state.temperature = temp_celsius;

        // Pause de 100ms (10 Hz)
        vTaskDelay(pdMS_TO_TICKS(100));


               printf(">Temperature:%.2f\n", system_state.temperature);
        printf(">Pressure:%.2f\n", pressure_hPa);
        
        // On affiche la profondeur avec 3 décimales pour bien voir le bruit (les millimètres)
        printf(">Depth:%.3f\n", system_state.current_depth);
    }
}