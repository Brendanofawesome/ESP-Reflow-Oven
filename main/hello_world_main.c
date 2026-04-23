/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "max_31856_driver.h"

// SPI pin definitions
#define SPI_MOSI_PIN 32
#define SPI_MISO_PIN 39
#define SPI_CLK_PIN 25
#define SENSOR_CS_PIN 27
#define CONTROL_PIN 33

static const char* LOG_TAG = "app_main";

void app_main(void)
{
    gpio_config_t control_pin_config = {
        .pin_bit_mask = (1ULL << CONTROL_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&control_pin_config));
    ESP_ERROR_CHECK(gpio_set_level(CONTROL_PIN, 1));

    ESP_LOGI(LOG_TAG, "Initializing SPI bus");
    
    // Initialize the SPI bus for the sensor to use
    spi_bus_config_t bus_config = {
        .mosi_io_num = SPI_MOSI_PIN,
        .miso_io_num = SPI_MISO_PIN,
        .sclk_io_num = SPI_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 320 * 52 + 8  // Default max transfer size
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }
    
    // Initialize the temperature sensor
    max_31856_t temperature_sensor;
    ESP_LOGI(LOG_TAG, "Initializing MAX31856 temperature sensor");
    
    ret = max_31856_init(SPI2_HOST, SENSOR_CS_PIN, &temperature_sensor, MAX31856_AUTO, 500);
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to initialize MAX31856: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return;
    }
    
    ESP_LOGI(LOG_TAG, "Sensor initialized successfully. Starting monitoring at 60Hz");
    
    // Main loop: read and print temperature and faults
    const TickType_t delay_ticks = pdMS_TO_TICKS(1000);  // 16.67ms for 60Hz
    
    while (true) {
        uint8_t flags = 0;
        float temperature = max_31856_get_temperature_c(&temperature_sensor, &flags);
        
        // Print current temperature, last read time (flags), and fault status
        printf("Temp: %.2f°C | Faults: 0x%01X\n", temperature, flags);
        
        vTaskDelay(delay_ticks);
    }
}
