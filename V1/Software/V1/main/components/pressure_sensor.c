#include "pressure_sensor.h"
#include "ui_model.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MS5837";

// --- Configuration I2C pour ESP32-C3 ---
#define I2C_MASTER_SCL_IO           9    
#define I2C_MASTER_SDA_IO           8   
#define I2C_MASTER_NUM              0    
#define I2C_MASTER_FREQ_HZ          100000 
#define I2C_MASTER_TIMEOUT_MS       100

// --- Commandes du MS5837-30BA ---
#define MS5837_ADDR                 0x76
#define MS5837_RESET                0x1E
#define MS5837_ADC_READ             0x00
#define MS5837_PROM_READ            0xA0
#define MS5837_CONVERT_D1_8192      0x4A 
#define MS5837_CONVERT_D2_8192      0x5A 

// Variables globales
static uint16_t C[8]; 
static float surface_pressure_hPa = 1013.25f; 

// --- Fonctions I2C Bas niveau ---

static esp_err_t ms5837_write_command(uint8_t cmd) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (MS5837_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(handle);
    return ret;
}

static esp_err_t ms5837_read_prom(uint8_t reg, uint16_t *value) {
    ms5837_write_command(MS5837_PROM_READ + (reg * 2));
    uint8_t buf[2] = {0};
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (MS5837_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(handle, buf, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(handle);
    *value = (buf[0] << 8) | buf[1];
    return ret;
}

static esp_err_t ms5837_read_adc(uint32_t *value) {
    ms5837_write_command(MS5837_ADC_READ);
    uint8_t buf[3] = {0};
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (MS5837_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(handle, buf, 3, I2C_MASTER_LAST_NACK);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(handle);
    *value = (buf[0] << 16) | (buf[1] << 8) | buf[2];
    return ret;
}

// --- Logique Mathématique SÉCURISÉE ---

static bool ms5837_read_and_calculate(float *out_pressure_hPa, float *out_temp_c) {
    uint32_t D1 = 0, D2 = 0;

    // 1. Conversion Pression (D1)
    if (ms5837_write_command(MS5837_CONVERT_D1_8192) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(30)); // 30ms = Sécurité absolue contre le décalage des ticks
    if (ms5837_read_adc(&D1) != ESP_OK || D1 == 0) return false;

    // 2. Conversion Température (D2)
    if (ms5837_write_command(MS5837_CONVERT_D2_8192) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(30)); 
    if (ms5837_read_adc(&D2) != ESP_OK || D2 == 0) return false;

    // 3. Calculs mathématiques
    int32_t dT = D2 - ((uint32_t)C[5] * 256);
    int32_t TEMP = 2000 + ((int64_t)dT * C[6]) / 8388608;

    int64_t OFF = ((int64_t)C[2] * 65536) + (((int64_t)C[4] * dT) / 128);
    int64_t SENS = ((int64_t)C[1] * 32768) + (((int64_t)C[3] * dT) / 256);

    // Compensation 2nd ordre
    int64_t Ti = 0, OFFi = 0, SENSi = 0;
    if (TEMP < 2000) { 
        Ti = (3 * (int64_t)dT * dT) / 8589934592LL;
        OFFi = 3 * (TEMP - 2000) * (TEMP - 2000) / 2;
        SENSi = 5 * (TEMP - 2000) * (TEMP - 2000) / 8;
        if (TEMP < -1500) { 
            OFFi = OFFi + 7 * (TEMP + 1500) * (TEMP + 1500);
            SENSi = SENSi + 4 * (TEMP + 1500) * (TEMP + 1500);
        }
    } else { 
        Ti = 2 * ((int64_t)dT * dT) / 137438953472LL;
        OFFi = 1 * (TEMP - 2000) * (TEMP - 2000) / 16;
        SENSi = 0;
    }

    TEMP = TEMP - Ti;
    OFF = OFF - OFFi;
    SENS = SENS - SENSi;

    int32_t P = (((D1 * SENS) / 2097152) - OFF) / 8192;

    *out_pressure_hPa = (float)P / 10.0f;
    *out_temp_c = (float)TEMP / 100.0f;
    return true;
}

// --- Fonctions Publiques ---

void init_dive_sensor(void) {
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

    ms5837_write_command(MS5837_RESET);
    vTaskDelay(pdMS_TO_TICKS(20)); // Laisse bien le temps au capteur de booter

    for (uint8_t i = 0; i < 7; i++) {
        ms5837_read_prom(i, &C[i]);
    }
}

void calibrate_surface_pressure(void) {
    ESP_LOGI(TAG, "Calibration de surface...");
    float sum_pressure = 0;
    float current_pressure, current_temp;
    int valid_samples = 0; 

    // On ignore délibérément les 2 premières lectures (poubelle)
    ms5837_read_and_calculate(&current_pressure, &current_temp);
    ms5837_read_and_calculate(&current_pressure, &current_temp);

    for (int i = 0; i < 50; i++) {
        if (ms5837_read_and_calculate(&current_pressure, &current_temp)) {
            sum_pressure += current_pressure;
            valid_samples++;
        }
    }

    if (valid_samples > 0) {
        surface_pressure_hPa = sum_pressure / valid_samples;
        ESP_LOGI(TAG, "Calibrée : %.2f hPa", surface_pressure_hPa);
    } else {
        ESP_LOGE(TAG, "Echec calibration !");
    }
}

void dive_sensor_task(void *pvParameters) {
    float pressure_hPa = 0;
    float temp_celsius = 0;

    while (1) {
        if (ms5837_read_and_calculate(&pressure_hPa, &temp_celsius)) {
            float density = system_state.is_salt_water ? 1025.0f : 1000.0f;
            float gravity = 9.80665f;

            float delta_P_Pa = (pressure_hPa - surface_pressure_hPa) * 100.0f;
            float depth = delta_P_Pa / (density * gravity);
            
            if (depth < 0.0f) depth = 0.0f; 

            system_state.current_depth = depth;
            system_state.temperature = temp_celsius;

            //printf(">Temperature:%.2f\n", system_state.temperature);
           // printf(">Pressure:%.2f\n", pressure_hPa);
           // printf(">Depth:%.3f\n", system_state.current_depth);
        } else {
            ESP_LOGW(TAG, "Erreur lecture I2C ignorée.");
        }
        
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}