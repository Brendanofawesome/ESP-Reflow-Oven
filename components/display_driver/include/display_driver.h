#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

typedef struct {
	esp_lcd_panel_handle_t device_handle;
    esp_lcd_panel_io_handle_t io_handle;

	const int pin_num_sclk;
	const int pin_num_mosi;
	const int pin_num_miso;
	const int pin_num_lcd_cs;
    
	const int pin_num_lcd_dc;
	const int pin_num_lcd_rst;

    const int pin_num_bkl;
    const bool bkl_state_on;

	const uint16_t hres;
	const uint16_t vres;

	const uint32_t lcd_pixel_clock_hz;
	const size_t lcd_cmd_bits;
	const size_t lcd_param_bits;
	const size_t max_transfer_sz;

	const bool lcd_swap_xy;
	const bool lcd_mirror_x;
	const bool lcd_mirror_y;

	const uint16_t buffersize;
} display_controller_config_t;

esp_err_t display_init(spi_host_device_t* host_id, display_controller_config_t* config);

typedef struct {
	esp_lcd_touch_handle_t spi_host;

	const int pin_num_sclk;
	const int pin_num_mosi;
	const int pin_num_miso;
	const int pin_num_touch_cs;

	const int pin_num_touch_int;
	const int pin_num_touch_rst;

    const uint16_t y_min;
    const uint16_t y_max;
    const uint16_t x_min;
    const uint16_t x_max;

	const bool touch_swap_xy;
	const bool touch_mirror_x;
	const bool touch_mirror_y;
} touch_controller_config_t;

esp_err_t touch_init(spi_host_device_t* host_id, touch_controller_config_t* config);