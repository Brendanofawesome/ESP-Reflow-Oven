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

esp_err_t display_init(spi_host_device_t host_id, display_controller_config_t* config){
    ESP_LOGI(LOG_TAG, "Starting Display Initialization");

    if(config == NULL) return ESP_ERR_INVALID_ARG;

    //initialize the SPI peripheral
    ESP_LOGD(LOG_TAG, "Initializng Display Controller SPI Bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = config->pin_num_sclk,
        .miso_io_num = config->pin_num_miso,
        .mosi_io_num = config->pin_num_mosi,
        .quadhd_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .max_transfer_sz = config->max_transfer_sz
    };
    esp_err_t err = spi_bus_initialize(host_id, &buscfg, SPI_DMA_CH_AUTO);
    if(err == ESP_ERR_INVALID_STATE){
        ESP_LOGW(LOG_TAG, "LCD SPI bus already initialized. Ensure buffer is large enough");
    } else if (err != ESP_OK) return err;

    //initialize the IO driver
    ESP_LOGD(LOG_TAG, "Initializing Panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = config->pin_num_lcd_cs,
        .dc_gpio_num = config->pin_num_lcd_dc,
        .spi_mode = 0,
        .pclk_hz = config->lcd_pixel_clock_hz,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = config->lcd_cmd_bits,
        .lcd_param_bits = config->lcd_param_bits,
        .flags = {
            .dc_low_on_data = false,
            .octal_mode = false,
            .quad_mode = false,
            .sio_mode = false,
            .lsb_first = false,
            .cs_high_active = false,
        }
    };
    err = esp_lcd_new_panel_io_spi(host_id, &io_config, &config->io_handle);
    if(err != ESP_OK){
        spi_bus_free(host_id);
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
        spi_bus_free(host_id);
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
        spi_bus_free(host_id);
        config->io_handle = NULL;
        config->device_handle = NULL;
        return err;
    }

    ESP_LOGD(LOG_TAG, "Initializing Display");
    err = esp_lcd_panel_reset(config->device_handle);
    if(err != ESP_OK){
        esp_lcd_panel_del(config->device_handle);
        esp_lcd_panel_io_del(config->io_handle);
        spi_bus_free(host_id);
        config->io_handle = NULL;
        config->device_handle = NULL;
        return err;
    }
    err = esp_lcd_panel_init(config->device_handle);
    if(err != ESP_OK){
        esp_lcd_panel_del(config->device_handle);
        esp_lcd_panel_io_del(config->io_handle);
        spi_bus_free(host_id);
        config->io_handle = NULL;
        config->device_handle = NULL;
        return err;
    }

    ESP_LOGI(LOG_TAG, "Display Mirror Mode: X:%d, Y:%d, swap:%d", config->lcd_mirror_x, config->lcd_mirror_y, config->lcd_swap_xy);
    esp_lcd_panel_mirror(config->device_handle, config->lcd_mirror_x, config->lcd_mirror_y);
    esp_lcd_panel_swap_xy(config->device_handle, config->lcd_swap_xy);

    ESP_LOGI(LOG_TAG, "Sending ON command to display");
    err = esp_lcd_panel_disp_on_off(config->device_handle, true);
    if(err != ESP_OK){
        esp_lcd_panel_del(config->device_handle);
        esp_lcd_panel_io_del(config->io_handle);
        spi_bus_free(host_id);
        config->io_handle = NULL;
        config->device_handle = NULL;
        return err;
    }

    ESP_LOGD(LOG_TAG, "Turning on LCD backlight");
    gpio_set_level(config->pin_num_bkl, config->bkl_state_on);

    ESP_LOGI(LOG_TAG, "Display Initialization Complete");
    return ESP_OK;
}


//used to scale recieved touch coordinates from the measured min and max values to the actual coordinates on the display
static uint16_t map_coordinate_clamped(const uint16_t value, const uint16_t in_actual_min, const uint16_t out_expected_max, const uint32_t scaling_factor){
    uint32_t mapped_value = value;

    //shift value to bottom of range
    mapped_value = (mapped_value <= in_actual_min) ? 0 : mapped_value - in_actual_min;

    //multiply value by the scaling factor
    mapped_value = mapped_value * scaling_factor;

    //shift fractional bits away
    mapped_value = mapped_value >> 8;

    //limit value to top of range
    if(mapped_value > out_expected_max) mapped_value = out_expected_max;

    return mapped_value;
}

//applies calibration to touch coordinates before they are handled by LVGL
static void touch_process_coordinates(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{

    if (tp == NULL || x == NULL || y == NULL || point_num == NULL || *point_num == 0) {
        return;
    }

    const touch_controller_config_t *cfg = (const touch_controller_config_t *)tp->config.driver_data;
    if (cfg == NULL || cfg->touch_hres == 0 || cfg->touch_vres == 0) {
        return;
    }

    const uint8_t count = (*point_num < max_point_num) ? *point_num : max_point_num;

    for (uint8_t i = 0; i < count; i++) {
        x[i] = map_coordinate_clamped(x[i], cfg->x_min, cfg->touch_hres - 1, cfg->_x_scaling_factor_frac8);
        y[i] = map_coordinate_clamped(y[i], cfg->y_min, cfg->touch_vres - 1, cfg->_y_scaling_factor_frac8);
    }
}

esp_err_t touch_init(spi_host_device_t host_id, touch_controller_config_t* config){
    ESP_LOGI(LOG_TAG, "Starting Touch Initialization");

    if(config == NULL) return ESP_ERR_INVALID_ARG;

    //initialize the SPI peripheral
    ESP_LOGD(LOG_TAG, "Initializng Touch Controller SPI Bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = config->pin_num_sclk,
        .miso_io_num = config->pin_num_miso,
        .mosi_io_num = config->pin_num_mosi,
        .quadhd_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 0,
        .isr_cpu_id = 1
    };
    esp_err_t err = spi_bus_initialize(host_id, &buscfg, SPI_DMA_CH_AUTO);
    if(err == ESP_ERR_INVALID_STATE){
        ESP_LOGD(LOG_TAG, "Touch Controller SPI bus already initialized.");
    } else if (err != ESP_OK) return err;

    //initialize the IO driver
    ESP_LOGD(LOG_TAG, "Initializing Touch IO Driver");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(config->pin_num_touch_cs);
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 5;
    err = esp_lcd_new_panel_io_spi(host_id, &io_config, &io_handle);
    if(err != ESP_OK) return err;

    //calculate scaling factors
    if(config->x_max == config->x_min || config->y_max == config->y_min){
        ESP_LOGE(LOG_TAG, "Touchscreen calibration range is not set");
        return ESP_ERR_INVALID_ARG;
    }
    config->_x_scaling_factor_frac8 = ((uint32_t)(config->touch_hres - 1) << 8) / (config->x_max - config->x_min);
    config->_y_scaling_factor_frac8 = ((uint32_t)(config->touch_vres - 1) << 8) / (config->y_max - config->y_min);

    //initialize specific touch interface
    ESP_LOGI(LOG_TAG, "Installing Touch Controller XPT2046");
    esp_lcd_touch_config_t touch_config = {
        .x_max = (uint16_t)(config->touch_hres - 1),
        .y_max = (uint16_t)(config->touch_vres - 1),
        .rst_gpio_num = config->pin_num_touch_rst,
        .int_gpio_num = config->pin_num_touch_int,
        .process_coordinates = touch_process_coordinates,
        .driver_data = config,
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
    ESP_LOGI(LOG_TAG, "Touch calibration: x[%u..%u] -> [0..%u], y[%u..%u] -> [0..%u]",
             config->x_min, config->x_max, (unsigned)(config->touch_hres - 1),
             config->y_min, config->y_max, (unsigned)(config->touch_vres - 1));
    ESP_LOGI(LOG_TAG, "Touch Initialization Complete");
    return ESP_OK;
}