#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lv_adapter.h"
#include "lvgl.h"

#include "display_driver.h"

/**
 * @brief LVGL runtime handles returned by lvgl_port_init().
 *
 * Use this structure to keep track of the LVGL display object and the touch
 * input object created during bringup. Store it in application state so you
 * can later create widgets, lock LVGL for thread-safe access, or cleanly tear
 * everything down with lvgl_port_deinit().
 */
typedef struct {
	lv_display_t* display;
	lv_indev_t* touch;
} lvgl_port_handles_t;

/**
 * @brief Configuration for the LVGL adapter worker and draw buffers.
 *
 * Fill this in when you want to override the default LVGL task settings or
 * buffer sizing. If you do not need custom behavior, use LVGL_PORT_DEFAULT_CONFIG().
 * The defaults are intended to be safe for this project and should work for
 * the first working integration.
 */
typedef struct {
	uint32_t task_stack_size;
	uint32_t task_priority;
	int task_core_id;
	uint32_t tick_period_ms;
	uint32_t task_min_delay_ms;
	uint32_t task_max_delay_ms;
	bool stack_in_psram;

	uint16_t buffer_height;
	bool use_psram_draw_buffer;
	bool require_double_buffer;
} lvgl_port_config_t;

/**
 * @brief Default LVGL port configuration.
 *
 * Use this macro as a starting point when bringing up the UI. It enables the
 * adapter with sane task settings and a modest draw buffer height. Override
 * individual fields if you need a larger buffer, different task priority, or
 * PSRAM-backed buffers.
 */
#define LVGL_PORT_DEFAULT_CONFIG() {                              \
	.task_stack_size = ESP_LV_ADAPTER_DEFAULT_STACK_SIZE,         \
	.task_priority = ESP_LV_ADAPTER_DEFAULT_TASK_PRIORITY,        \
	.task_core_id = ESP_LV_ADAPTER_DEFAULT_TASK_CORE_ID,          \
	.tick_period_ms = ESP_LV_ADAPTER_DEFAULT_TICK_PERIOD_MS,      \
	.task_min_delay_ms = ESP_LV_ADAPTER_DEFAULT_TASK_MIN_DELAY_MS,\
	.task_max_delay_ms = ESP_LV_ADAPTER_DEFAULT_TASK_MAX_DELAY_MS,\
	.stack_in_psram = false,                                      \
	.buffer_height = 40,                                          \
	.use_psram_draw_buffer = false,                               \
	.require_double_buffer = false,                               \
}

/**
 * @brief Bring up the display, touch controller, and LVGL runtime.
 *
 * Call this once during application startup after defining the panel and touch
 * pin mappings in display_config and touch_config. This function initializes
 * the hardware, starts the LVGL adapter task, registers the LVGL display, and
 * registers the touch input device so the UI can begin drawing and accepting
 * input.
 *
 * Pass a valid out_handles pointer and keep the returned handles for later UI
 * work and shutdown. If host_id is NULL, SPI2_HOST is used.
 *
 * @param host_id SPI host to use for both display and touch. Pass NULL to use
 *                SPI2_HOST.
 * @param display_config Hardware settings for the LCD panel and backlight.
 * @param touch_config Hardware settings for the XPT2046 touch controller.
 * @param port_config Optional LVGL task/buffer settings. Pass NULL to use
 *                    LVGL_PORT_DEFAULT_CONFIG().
 * @param out_handles Receives the LVGL display and touch handles on success.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if required arguments are NULL
 *      - Any hardware or LVGL adapter error returned by the underlying APIs
 */
esp_err_t lvgl_port_init(spi_host_device_t* host_id,
						 display_controller_config_t* display_config,
						 touch_controller_config_t* touch_config,
						 const lvgl_port_config_t* port_config,
						 lvgl_port_handles_t* out_handles);

/**
 * @brief Tear down the LVGL runtime and unregister input/display objects.
 *
 * Call this during shutdown before deleting the LCD or touch hardware handles.
 * It removes the LVGL input device, unregisters the display, and shuts down
 * the adapter worker if it was started by lvgl_port_init().
 *
 * @param handles The handles previously returned by lvgl_port_init().
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if handles is NULL
 *      - Any error returned by the underlying unregister/deinit calls
 */
esp_err_t lvgl_port_deinit(const lvgl_port_handles_t* handles);

/**
 * @brief Lock LVGL for thread-safe access.
 *
 * Use this before creating widgets, updating labels, or touching LVGL objects
 * from application tasks. This prevents concurrent access while the LVGL
 * worker task is running.
 *
 * Pass -1 to wait forever, or a timeout in milliseconds if you want bounded
 * waiting.
 *
 * @param timeout_ms Lock wait timeout in milliseconds, or -1 for infinite wait.
 *
 * @return
 *      - ESP_OK if the lock was acquired
 *      - ESP_ERR_TIMEOUT if the lock could not be acquired in time
 *      - Any error returned by the LVGL adapter
 */
esp_err_t lvgl_port_lock(int32_t timeout_ms);

/**
 * @brief Release the LVGL lock acquired by lvgl_port_lock().
 *
 * Call this after you finish modifying LVGL objects from your task.
 */
void lvgl_port_unlock(void);
