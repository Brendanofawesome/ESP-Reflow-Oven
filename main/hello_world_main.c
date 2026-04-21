#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

#include "lvgl_port.h"

static const char *LOG_TAG = "app_main";

static void touch_event_handler(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) {
        return;
    }

    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }

    lv_point_t point = { 0 };
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        ESP_LOGI(LOG_TAG, "Touch down at x=%d y=%d", (int)point.x, (int)point.y);
    } else if (code == LV_EVENT_PRESSING) {
        ESP_LOGI(LOG_TAG, "Touch move at x=%d y=%d", (int)point.x, (int)point.y);
    } else {
        ESP_LOGI(LOG_TAG, "Touch up at x=%d y=%d", (int)point.x, (int)point.y);
    }
}

void app_main(void)
{
    display_controller_config_t display_config = {
        .device_handle = NULL,
        .io_handle = NULL,
        .pin_num_sclk = 14,
        .pin_num_mosi = 13,
        .pin_num_miso = 12,
        .pin_num_lcd_cs = 15,
        .pin_num_lcd_dc = 2,
        .pin_num_lcd_rst = -1,
        .pin_num_bkl = 21,
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
        .pin_num_sclk = 25,
        .pin_num_mosi = 32,
        .pin_num_miso = 39,
        .pin_num_touch_cs = 33,
        .pin_num_touch_int = 36,
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
    ESP_ERROR_CHECK(display_init(SPI2_HOST, &display_config));

    ESP_LOGI(LOG_TAG, "Bringing up touchscreen");
    ESP_ERROR_CHECK(touch_init(SPI3_HOST, &touch_config));

    ESP_LOGI(LOG_TAG, "Bringing up LVGL");
    ESP_ERROR_CHECK(lvgl_port_init(&display_config, &touch_config, &lvgl_config, &handles));
    

    //verify panel rendering and touch
    ESP_ERROR_CHECK(lvgl_port_lock(-1));
    if (handles.touch != NULL) {
        lv_indev_add_event_cb(handles.touch, touch_event_handler, LV_EVENT_PRESSED, NULL);
        lv_indev_add_event_cb(handles.touch, touch_event_handler, LV_EVENT_PRESSING, NULL);
        lv_indev_add_event_cb(handles.touch, touch_event_handler, LV_EVENT_RELEASED, NULL);
    }

    lv_obj_t *screen = lv_screen_active();

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Hello, world!\nLVGL is running.");
    lv_obj_center(label);
    lvgl_port_unlock();

    ESP_LOGI(LOG_TAG, "LVGL demo running");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
