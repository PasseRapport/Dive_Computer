#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "ui_model.h"

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool cali_enabled = false;

void init_battery_adc(void) {
    // 1. Initialisation de l'unité ADC1
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config1, &adc1_handle);

    // 2. Configuration du canal avec l'atténuation 12dB (remplace 11dB obsolète)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &config);

    // 3. Configuration de la calibration matérielle (très important pour la précision)
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle) == ESP_OK) {
        cali_enabled = true;
    }
}

void update_battery_percentage(void) {
    int raw = 0;
    int voltage_mv = 0;
    
    // Lecture brute
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &raw);
    
    // Conversion en millivolts avec la courbe de calibration
    if (cali_enabled) {
        adc_cali_raw_to_voltage(adc_cali_handle, raw, &voltage_mv);
    } else {
        // Fallback ultra-basique si pas de calibration (ne devrait pas arriver)
        voltage_mv = raw * 3300 / 4095; 
    }
    
    // Pont diviseur par 2 => Tension de la batterie est le double
    float voltage = (voltage_mv * 2) / 1000.0f;
    
    // Map 3.0V -> 0% to 4.2V -> 100%
    float percent = (voltage - 3.0f) / (4.2f - 3.0f) * 100.0f;
    if (percent > 100.0f) percent = 100.0f;
    if (percent < 0.0f) percent = 0.0f;
    
    system_state.battery_percentage = (uint8_t)percent;
}
