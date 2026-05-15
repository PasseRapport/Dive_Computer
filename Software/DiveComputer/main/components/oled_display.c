#include "oled_display.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_timer.h"
#include "lvgl.h"

// --- Hardware Definitions ---
#define SCLK_PIN  4
#define MOSI_PIN  6
#define DC_PIN    7
#define CS_PIN    2
#define RST_PIN   3

#define LCD_HOST       SPI2_HOST
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  96

static lv_disp_drv_t disp_drv;
// --- Variables de contrôle matériel ---
static esp_lcd_panel_io_handle_t io_handle = NULL; 

// --- Callbacks & Tasks ---

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    esp_lcd_panel_io_handle_t local_io_handle = (esp_lcd_panel_io_handle_t)drv->user_data;
    uint8_t col_data[] = { area->x1, area->x2 };
    esp_lcd_panel_io_tx_param(local_io_handle, 0x15, col_data, 2); 
    uint8_t row_data[] = { area->y1, area->y2 };
    esp_lcd_panel_io_tx_param(local_io_handle, 0x75, row_data, 2); 
    size_t len = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    esp_lcd_panel_io_tx_color(local_io_handle, 0x5C, color_map, len * sizeof(lv_color_t));
}

static void lv_tick_task(void *arg) {
    lv_tick_inc(2); 
}

static void lvgl_port_task(void *arg) {
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- Initialization Functions ---

void init_oled_display(void) {
    spi_bus_config_t buscfg = {
        .sclk_io_num = SCLK_PIN,
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t),
    };
    spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = DC_PIN,
        .cs_gpio_num = CS_PIN,
        .pclk_hz = 14 * 1000 * 1000, 
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = &disp_drv
    };
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle);

    gpio_reset_pin(RST_PIN);
    gpio_set_direction(RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Hardware Init Sequence
    esp_lcd_panel_io_tx_param(io_handle, 0xFD, (uint8_t[]){0x12}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xFD, (uint8_t[]){0xB1}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xAE, NULL, 0);              
    esp_lcd_panel_io_tx_param(io_handle, 0xB3, (uint8_t[]){0xF1}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xCA, (uint8_t[]){0x5F}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xA0, (uint8_t[]){0x74}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0x15, (uint8_t[]){0x00, 0x7F}, 2); 
    esp_lcd_panel_io_tx_param(io_handle, 0x75, (uint8_t[]){0x00, 0x5F}, 2); 
    esp_lcd_panel_io_tx_param(io_handle, 0xA1, (uint8_t[]){0x00}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xA2, (uint8_t[]){0x00}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xB5, (uint8_t[]){0x00}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xAB, (uint8_t[]){0x01}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xB1, (uint8_t[]){0x32}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xBE, (uint8_t[]){0x05}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xA6, NULL, 0);              
    esp_lcd_panel_io_tx_param(io_handle, 0xC1, (uint8_t[]){0xC8, 0x80, 0xC8}, 3); 
    esp_lcd_panel_io_tx_param(io_handle, 0xC7, (uint8_t[]){0x0F}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xB4, (uint8_t[]){0xA0, 0xB5, 0x55}, 3); 
    esp_lcd_panel_io_tx_param(io_handle, 0xB6, (uint8_t[]){0x01}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xAF, NULL, 0);              

    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[SCREEN_WIDTH * 10]; 
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_WIDTH * 10);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = io_handle; 
    lv_disp_drv_register(&disp_drv);

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lv_tick_task,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000);

    // Spawn the LVGL background task
    xTaskCreate(lvgl_port_task, "LVGL", 4096, NULL, 5, NULL);
}

// --- Contrôle Matériel ---
void set_display_brightness(uint8_t level) {
    if (level > 15) {
        level = 15; 
    }
    
    if (io_handle != NULL) {
        esp_lcd_panel_io_tx_param(io_handle, 0xC7, &level, 1);
    }
}