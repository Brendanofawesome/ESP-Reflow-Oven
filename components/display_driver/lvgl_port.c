#include "lvgl_port.h"

#include <string.h>

#include "esp_log.h"

static const char *LOG_TAG = "lvgl_port";
static bool lvgl_task_started = false;

static esp_err_t init_lv_adapter(const lvgl_port_config_t* port_config){
	//initialize the LVGL adapter runtime
	if(esp_lv_adapter_is_initialized()) return ESP_OK;

	const lvgl_port_config_t default_port_config = LVGL_PORT_DEFAULT_CONFIG();
	const lvgl_port_config_t* config = (port_config != NULL) ? port_config : &default_port_config;

	const esp_lv_adapter_config_t lvgl_config = {
		.task_stack_size = config->task_stack_size,
		.task_priority = config->task_priority,
		.task_core_id = config->task_core_id,
		.tick_period_ms = config->tick_period_ms,
		.task_min_delay_ms = config->task_min_delay_ms,
		.task_max_delay_ms = config->task_max_delay_ms,
		.stack_in_psram = config->stack_in_psram,
	};

	esp_err_t err = esp_lv_adapter_init(&lvgl_config);
	if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

	err = esp_lv_adapter_start();
	if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

	lvgl_task_started = true;
	return ESP_OK;
}

esp_err_t lvgl_port_init(spi_host_device_t* host_id,
						 display_controller_config_t* display_config,
						 touch_controller_config_t* touch_config,
						 const lvgl_port_config_t* port_config,
						 lvgl_port_handles_t* out_handles){
	ESP_LOGI(LOG_TAG, "Starting LVGL Port Bringup");

	if(display_config == NULL || touch_config == NULL || out_handles == NULL) return ESP_ERR_INVALID_ARG;

	//clear returned handles before setup begins
	memset(out_handles, 0, sizeof(*out_handles));

	spi_host_device_t selected_host = (host_id != NULL) ? *host_id : SPI2_HOST;

	//initialize the LCD and touch hardware
	esp_err_t err = display_init(&selected_host, display_config);
	if(err != ESP_OK) return err;

	err = touch_init(&selected_host, touch_config);
	if(err != ESP_OK) return err;

	err = init_lv_adapter(port_config);
	if(err != ESP_OK) return err;

	//build the LVGL display registration using the hardware handles
	const lvgl_port_config_t default_port_config = LVGL_PORT_DEFAULT_CONFIG();
	const lvgl_port_config_t* config = (port_config != NULL) ? port_config : &default_port_config;

	esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
		display_config->device_handle,
		display_config->io_handle,
		display_config->hres,
		display_config->vres,
		ESP_LV_ADAPTER_ROTATE_0
	);
	display_cfg.profile.buffer_height = config->buffer_height;
	display_cfg.profile.use_psram = config->use_psram_draw_buffer;
	display_cfg.profile.require_double_buffer = config->require_double_buffer;

	out_handles->display = esp_lv_adapter_register_display(&display_cfg);
	if(out_handles->display == NULL){
		ESP_LOGE(LOG_TAG, "Failed to register LVGL display");
		return ESP_FAIL;
	}

	//register the touch device against the LVGL display
	const esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(
		out_handles->display,
		touch_config->spi_host
	);
	out_handles->touch = esp_lv_adapter_register_touch(&touch_cfg);
	if(out_handles->touch == NULL){
		ESP_LOGE(LOG_TAG, "Failed to register LVGL touch input");
		esp_lv_adapter_unregister_display(out_handles->display);
		out_handles->display = NULL;
		return ESP_FAIL;
	}

	ESP_LOGI(LOG_TAG, "LVGL Port Bringup Complete");
	return ESP_OK;
}

esp_err_t lvgl_port_deinit(const lvgl_port_handles_t* handles){
	//tear down LVGL objects in reverse order of initialization
	if(handles == NULL) return ESP_ERR_INVALID_ARG;

	esp_err_t err = ESP_OK;

	if(handles->touch != NULL){
		esp_err_t touch_err = esp_lv_adapter_unregister_touch(handles->touch);
		if(touch_err != ESP_OK) err = touch_err;
	}

	if(handles->display != NULL){
		esp_err_t display_err = esp_lv_adapter_unregister_display(handles->display);
		if(display_err != ESP_OK && err == ESP_OK) err = display_err;
	}

	if(lvgl_task_started || esp_lv_adapter_is_initialized()){
		esp_err_t deinit_err = esp_lv_adapter_deinit();
		if(deinit_err != ESP_OK && err == ESP_OK) err = deinit_err;
		lvgl_task_started = false;
	}

	return err;
}

esp_err_t lvgl_port_lock(int32_t timeout_ms){
	return esp_lv_adapter_lock(timeout_ms);
}

void lvgl_port_unlock(void){
	esp_lv_adapter_unlock();
}
