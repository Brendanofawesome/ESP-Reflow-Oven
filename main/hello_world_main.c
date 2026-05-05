//General Purpose Includes
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <math.h> //NaN

#include "esp_log.h"
#include "esp_check.h"

//task management
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//IO
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_timer.h"

//UI
#include "display_driver.h" //custom display initializer
#include "lvgl_port.h" //custom lvgl library
#include "UI.h"
#include "lvgl.h"

//temperature sensor
#include "max_31856_driver.h" //custom

#include "profile.h"

#include "PID.h"

static const char* LOG_TAG = "app_main";

//global variables
max_31856_t temperature_sensor;
ui_t ui_struct;

_Atomic float target_temperature;
_Atomic float current_hotside_temperature;

// SPI pin definitions
//BUS2: Display
#define LCD_HOST SPI2_HOST
#define LCD_SPI_MOSI 13
#define LCD_MISO 12
#define LCD_SPI_CLK 14

//BUS3 Touch Controller and Temperature Sensor
#define AUX_HOST SPI3_HOST
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

//Optocoupler
#define HEATER_PIN 22
// globals declared above

// Heater timer handler: runs at ~60Hz and performs PID computation and
// phase-angle style output control
static void Heater_Timer_Handler(lv_timer_t* timer) {
    static int64_t last_time_us = 0;
    static uint8_t prev_setpoint = 0;
    static uint8_t prev_gpio_state = 0;
    (void)timer;

    int64_t now = esp_timer_get_time(); // microseconds
    uint32_t dt_ms = 100; // default if first run
    if (last_time_us != 0) {
        int64_t delta_us = now - last_time_us;
        if (delta_us < 0) delta_us = 0;
        dt_ms = (uint32_t)(delta_us / 1000);
        if (dt_ms == 0) dt_ms = 1;
    }
    last_time_us = now;

    // current temperatures
    float cur = atomic_load_explicit(&current_hotside_temperature, memory_order_relaxed);

    // manage profile-driven requested temperature
    static const profile_t* running_profile = NULL;
    static uint16_t chased_point_index = 0;
    static int64_t point_start_time_us = 0;
    static uint32_t point_start_temp_x128 = 0;

    // detect start/stop transitions and profile changes
    static bool prev_active = false;
    bool now_active = ui_struct.active;
    if (now_active && !prev_active) {
        // starting run: capture current profile and temps
        running_profile = ui_struct.selected_profile;
        chased_point_index = 0;
        point_start_time_us = esp_timer_get_time();
        // convert current temperature to X128 fixed point
        point_start_temp_x128 = (uint32_t)((cur) * 128.0f);
    }
    if (!now_active) {
        running_profile = NULL;
    }
    prev_active = now_active;

    float req = NAN;
    if (running_profile != NULL && running_profile->points != NULL && chased_point_index < running_profile->num_points) {
        // elapsed since this point started
        int64_t now_us = esp_timer_get_time();
        uint32_t elapsed_ms = 0;
        if (point_start_time_us != 0) {
            int64_t d = now_us - point_start_time_us;
            if (d < 0) d = 0;
            elapsed_ms = (uint32_t)(d / 1000);
        }

        key_point_t* next_point = &running_profile->points[chased_point_index];
        uint32_t target_x128 = get_target_temperature(elapsed_ms, point_start_temp_x128, next_point);
        req = (float)target_x128 / 128.0f;

        // check if point satisfied; if so, advance
        key_point_t* maybe = next_point_if_satisfied(elapsed_ms, (uint32_t)(cur * 128.0f), (profile_t*)running_profile, chased_point_index);
        if (maybe != next_point) {
            // advance index
            if ((size_t)(chased_point_index + 1) < running_profile->num_points) {
                chased_point_index++;
                // reset start time and temp for next point
                point_start_time_us = now_us;
                point_start_temp_x128 = (uint32_t)(req * 128.0f);
            } else {
                // finished profile
                ui_struct.active = false;
                running_profile = NULL;
            }
        }
    } else {
        ui_struct.active = false;
    }

    uint8_t setpoint = get_PID_setpoint((uint32_t)req, (uint32_t)cur, 0, dt_ms, prev_setpoint);

    // software-driven phase counter (0..255) advanced by the timer
    static uint8_t phase_counter = 0;
    phase_counter++; // wraps naturally from 255 -> 0

    //ESP_LOGI(LOG_TAG, "PID Setpoint: %d (phase: %d/255), Req: %.2f°C, Cur: %.2f°C", setpoint, phase_counter, req, cur);

    if (ui_struct.active) {
        // when the phase counter reaches top (255) assert heater drive
        if (phase_counter == 255) {
            if (prev_gpio_state != 1) {
                ESP_LOGI(LOG_TAG, "Heater ON (GPIO set to 1)");
                prev_gpio_state = 1;
            }
            gpio_set_level(HEATER_PIN, 1);
        }
        // when the phase counter reaches the PID setpoint, turn off
        if (setpoint != 255 && phase_counter == setpoint) {
            if (prev_gpio_state != 0) {
                ESP_LOGI(LOG_TAG, "Heater OFF (GPIO set to 0)");
                prev_gpio_state = 0;
            }
            gpio_set_level(HEATER_PIN, 0);
        }
    } else {
        // ensure heater off when not active
        if (prev_gpio_state != 0) {
            ESP_LOGI(LOG_TAG, "Heater OFF (not active)");
            prev_gpio_state = 0;
        }
        gpio_set_level(HEATER_PIN, 0);
    }

    prev_setpoint = setpoint;

    // publish requested temperature for UI
    atomic_store_explicit(&target_temperature, req, memory_order_relaxed);
}

//LVGL temperature timer task
void UI_Update_Temperatures(lv_timer_t* timer){
    uint8_t flags;
    float last_hotside_temp = max_31856_get_temperature_c(&temperature_sensor, &flags, NULL);

    if(flags != 0){ //some error was detected
    //    last_hotside_temp = NAN;
    }

    float target_temp = atomic_load_explicit(&target_temperature, memory_order_relaxed);

    ESP_LOGD(LOG_TAG, "Read Current Temperature: %3.2f°C, flags: 0x%02x", last_hotside_temp, flags);

    // publish current temperature for PID callback
    atomic_store_explicit(&current_hotside_temperature, last_hotside_temp, memory_order_relaxed);

    ui_set_target_temperature(&ui_struct, target_temp);
    ui_set_current_temperature(&ui_struct, last_hotside_temp, flags);
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
    gpio_set_level(22, 0);
    gpio_set_direction(22, GPIO_MODE_OUTPUT);

    atomic_init(&target_temperature, NAN);
    atomic_init(&current_hotside_temperature, NAN);

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

    //initialize the temperature sensor
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

    if (temp_timer == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create temperature update timer");
        lvgl_port_unlock();
        return;
    }

    // create heater control timer (~60Hz)
    lv_timer_t* heater_timer = lv_timer_create(Heater_Timer_Handler, 17, NULL);
    if (heater_timer == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to create heater timer");
        lvgl_port_unlock();
        return;
    }

    lvgl_port_unlock();


    ESP_LOGI(LOG_TAG, "LVGL worker running; app_main entering watchdog loop");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
