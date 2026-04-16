#include "display_driver.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "lvgl.h"

#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch_xpt2046.h"

static const char *LOG_TAG = "ui_hw_init";

esp_err_t display_init(spi_host_device_t* host_id, display_controller_config_t* config){
    ESP_LOGI(LOG_TAG, "Starting Display Initialization");

    if(config == NULL) return ESP_ERR_INVALID_ARG;
    const spi_host_device_t selected_host = (host_id != NULL) ? *host_id : SPI2_HOST;

    //initialize the SPI peripheral
    ESP_LOGD(LOG_TAG, "Initializng Display Controller SPI Bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = config->pin_num_sclk,
        .miso_io_num = config->pin_num_miso,
        .mosi_io_num = config->pin_num_mosi,
        .quadhd_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .max_transfer_sz = config->buffersize
    };
    esp_err_t err = spi_bus_initialize(selected_host, &buscfg, SPI_DMA_CH_AUTO);
    if(err != ESP_OK) return err;

    //initialize the IO driver
    ESP_LOGD(LOG_TAG, "Initializing Panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = config->pin_num_lcd_dc,
        .cs_gpio_num = config->pin_num_lcd_cs,
        .pclk_hz = config->lcd_pixel_clock_hz,
        .lcd_cmd_bits = config->lcd_cmd_bits,
        .lcd_param_bits = config->lcd_param_bits,
        .spi_mode = 0,
        .trans_queue_depth = 10
    };
    err = esp_lcd_new_panel_io_spi(selected_host, &io_config, &config->io_handle);
    if(err != ESP_OK){
        spi_bus_free(selected_host);
        return err;
    }

    //initialize specific panel interface
    ESP_LOGD(LOG_TAG, "Installing LCD");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = config->pin_num_lcd_rst,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG
    };
    err = esp_lcd_new_panel_ili9341(config->io_handle, &panel_config, &config->device_handle);
    if(err != ESP_OK) {
        esp_lcd_panel_io_del(config->io_handle);
        spi_bus_free(selected_host);
        config->io_handle = NULL;
        config->device_handle = NULL;
        return err;
    }

    ESP_LOGI(LOG_TAG, "Enabling Backlight Driver");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << config->pin_num_bkl
    };
    err = gpio_config(&bk_gpio_config);
    if(err != ESP_OK){
        esp_lcd_panel_del(config->device_handle);
        esp_lcd_panel_io_del(config->io_handle);
        spi_bus_free(selected_host);
        config->io_handle = NULL;
        config->device_handle = NULL;
        return err;
    }

    ESP_LOGD(LOG_TAG, "Initializing Display");
    err = esp_lcd_panel_reset(config->device_handle);
    if(err != ESP_OK){
        esp_lcd_panel_del(config->device_handle);
        esp_lcd_panel_io_del(config->io_handle);
        spi_bus_free(selected_host);
        config->io_handle = NULL;
        config->device_handle = NULL;
        return err;
    }
    err = esp_lcd_panel_init(config->device_handle);
    if(err != ESP_OK){
        esp_lcd_panel_del(config->device_handle);
        esp_lcd_panel_io_del(config->io_handle);
        spi_bus_free(selected_host);
        config->io_handle = NULL;
        config->device_handle = NULL;
        return err;
    }

    ESP_LOGI(LOG_TAG, "Display Mirror Mode: X:%d, Y:%d, swap:%d", config->lcd_mirror_x, config->lcd_mirror_y, config->lcd_swap_xy);
    esp_lcd_panel_mirror(config->device_handle, config->lcd_mirror_x, config->lcd_mirror_y);
    esp_lcd_panel_swap_xy(config->device_handle, config->lcd_swap_xy);

    ESP_LOGD(LOG_TAG, "Turning on LCD backlight");
    gpio_set_level(config->pin_num_bkl, config->bkl_state_on);

    ESP_LOGI(LOG_TAG, "Display Initialization Complete");
    return ESP_OK;
}

esp_err_t touch_init(spi_host_device_t* host_id, touch_controller_config_t* config){
    ESP_LOGI(LOG_TAG, "Starting Touch Initialization");

    if(config == NULL) return ESP_ERR_INVALID_ARG;
    const spi_host_device_t selected_host = (host_id != NULL) ? *host_id : SPI2_HOST;

    //initialize the SPI peripheral
    ESP_LOGD(LOG_TAG, "Initializng Touch Controller SPI Bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = config->pin_num_sclk,
        .miso_io_num = config->pin_num_miso,
        .mosi_io_num = config->pin_num_mosi,
        .quadhd_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 0
    };
    esp_err_t err = spi_bus_initialize(selected_host, &buscfg, SPI_DMA_CH_AUTO);
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    //initialize the IO driver
    ESP_LOGD(LOG_TAG, "Initializing Touch IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(config->pin_num_touch_cs);
    err = esp_lcd_new_panel_io_spi(selected_host, &io_config, &io_handle);
    if(err != ESP_OK) return err;

    //initialize specific touch interface
    ESP_LOGI(LOG_TAG, "Installing Touch Controller XPT2046");
    esp_lcd_touch_config_t touch_config = {
        .x_max = config->x_max,
        .y_max = config->y_max,
        .rst_gpio_num = config->pin_num_touch_rst,
        .int_gpio_num = config->pin_num_touch_int,
        .flags = {
            .swap_xy = config->touch_swap_xy,
            .mirror_x = config->touch_mirror_x,
            .mirror_y = config->touch_mirror_y,
        },
    };
    err = esp_lcd_touch_new_spi_xpt2046(io_handle, &touch_config, &config->spi_host);
    if(err != ESP_OK){
        esp_lcd_panel_io_del(io_handle);
        config->spi_host = NULL;
        return err;
    }

    ESP_LOGI(LOG_TAG, "Touch Mirror Mode: X:%d, Y:%d, swap:%d", config->touch_mirror_x, config->touch_mirror_y, config->touch_swap_xy);
    ESP_LOGI(LOG_TAG, "Touch Initialization Complete");
    return ESP_OK;
}