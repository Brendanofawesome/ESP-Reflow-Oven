//General Purpose Includes
#include <stdio.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_check.h"

//task management
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//IO
#include "driver/spi_master.h"
#include "driver/gpio.h"

//UI
#include "display_driver.h" //custom display initializer
#include "lvgl_port.h" //custom lvgl library
#include "UI.h"
#include "lvgl.h"

//temperature sensor
#include "max_31856_driver.h" //custom

// SPI pin definitions
//BUS2: Display
#define LCD_HOST SPI3_HOST
#define LCD_SPI_MOSI 13
#define LCD_MISO 12
#define LCD_SPI_CLK 14

//BUS3 Touch Controller and Temperature Sensor
#define AUX_HOST SPI2_HOST
#define AUX_MOSI 32
#define AUX_MISO 39
#define AUX_CLK 25

//other control pins
#define MAX31856_CS 27

#define LCD_CS 15
#define LCD_DC 2
#define LCD_BKL 21

#define TOUCH_CS 33
#define TOUCH_INT 36

static const char* LOG_TAG = "app_main";
max_31856_t temperature_sensor;
ui_t ui_struct;

//LVGL temperature timer task
void UI_Update_Temperatures(lv_timer_t* timer){
    float last_hotside_temp = max_31856_get_temperature_c(&temperature_sensor, NULL, NULL);
    float target_temp = -1;

    ESP_LOGD(LOG_TAG, "Current Temperature: %3.2f°C", last_hotside_temp);

    ui_set_target_temperature(&ui_struct, target_temp);
    ui_set_current_temperature(&ui_struct, last_hotside_temp);
}


static void touch_event_debug_handler(lv_event_t *e){
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED) {
        return;
    }

    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }

    lv_point_t point = { 0 };
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        ESP_LOGD(LOG_TAG, "Touch down at x=%d y=%d", (int)point.x, (int)point.y);
    } else {
        ESP_LOGD(LOG_TAG, "Touch up at x=%d y=%d", (int)point.x, (int)point.y);
    }
}

void app_main(void)
{
    display_controller_config_t display_config = {
        .device_handle = NULL,
        .io_handle = NULL,
        .pin_num_sclk = LCD_SPI_CLK,
        .pin_num_mosi = LCD_SPI_MOSI,
        .pin_num_miso = LCD_MISO,
        .pin_num_lcd_cs = LCD_CS,
        .pin_num_lcd_dc = LCD_DC,
        .pin_num_lcd_rst = -1,
        .pin_num_bkl = LCD_BKL,
        .bkl_state_on = true,
        .hres = 240,
        .vres = 320,
        .lcd_pixel_clock_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .max_transfer_sz = 240 * 40 * sizeof(uint16_t),
        .lcd_swap_xy = true,
        .lcd_mirror_x = false,
        .lcd_mirror_y = false,
        .buffersize = 240 * 40 * sizeof(uint16_t),
    };

    touch_controller_config_t touch_config = {
        .spi_host = NULL,
        .pin_num_sclk = AUX_CLK,
        .pin_num_mosi = AUX_MOSI,
        .pin_num_miso = AUX_MISO,
        .pin_num_touch_cs = TOUCH_CS,
        .pin_num_touch_int = TOUCH_INT,
        .pin_num_touch_rst = -1,
        .touch_hres = 240,
        .touch_vres = 320,
        .y_min = 29,
        .y_max = 295,
        .x_min = 23,
        .x_max = 214,
        .touch_swap_xy = false,
        .touch_mirror_x = false,
        .touch_mirror_y = true,
    };

    lvgl_port_config_t lvgl_config = LVGL_PORT_DEFAULT_CONFIG();
    lvgl_port_handles_t handles = { 0 };
    
    ESP_LOGI(LOG_TAG, "Bringing up LCD");
    ESP_ERROR_CHECK(display_init(LCD_HOST, &display_config));

    ESP_LOGI(LOG_TAG, "Bringing up touchscreen");
    ESP_ERROR_CHECK(touch_init(AUX_HOST, &touch_config));

    ESP_LOGI(LOG_TAG, "Bringing up LVGL adapter");
    ESP_ERROR_CHECK(lvgl_port_init(&display_config, &touch_config, &lvgl_config, &handles));

    //setup touch debugging
    ESP_LOGI(LOG_TAG, "Attatching Touch Debug Callback");
    ESP_ERROR_CHECK(lvgl_port_lock(-1));
    lv_indev_add_event_cb(handles.touch, touch_event_debug_handler,LV_EVENT_PRESSED, NULL);
    lv_indev_add_event_cb(handles.touch, touch_event_debug_handler, LV_EVENT_RELEASED, NULL);

    // Initialize the temperature sensor
    ESP_LOGI(LOG_TAG, "Initializing MAX31856 temperature sensor");
    
    esp_err_t ret = max_31856_init(AUX_HOST, MAX31856_CS, &temperature_sensor, MAX31856_AUTO, 500);
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to initialize MAX31856: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(LOG_TAG, "Sensor initialized successfully. Starting monitoring at 60Hz");

    ESP_LOGI(LOG_TAG, "Creating UI...");
    ui_init(&ui_struct);

    //create the temperature update timer
    ESP_LOGI(LOG_TAG, "attatching the temperature monitor");
    lv_timer_t* temp_timer = lv_timer_create(UI_Update_Temperatures, 1000/3, NULL);
    lvgl_port_unlock();

    if (temp_timer == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create temperature update timer");
        return;
    }

    ESP_LOGI(LOG_TAG, "LVGL worker running; app_main entering idle sleep");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
