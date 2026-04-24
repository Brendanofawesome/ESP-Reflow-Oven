#include "max_31856_driver.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

const static char* LOG_TAG = "max_31856 Driver";

static void store_reading_from_rx(max_31856_t* max_31856) {
    int32_t hotside_reading = ((int32_t)(
        ((uint32_t)max_31856->rx_buffer[2] << (8 * 3)) |
        ((uint32_t)max_31856->rx_buffer[3] << (8 * 2)) |
        ((uint32_t)max_31856->rx_buffer[4] << (8 * 1))
    )) >> (32 - 19);

    int16_t coldsize_reading = *(uint16_t*)(max_31856->rx_buffer) >> 2;

    uint8_t read_flags = max_31856->rx_buffer[5];

    atomic_store_explicit(&max_31856->last_hotside_temp_reading, hotside_reading, memory_order_relaxed);
    atomic_store_explicit(&max_31856->last_coldside_temp_reading, coldsize_reading, memory_order_relaxed);
    atomic_store_explicit(&max_31856->last_flags_reading, read_flags, memory_order_relaxed);
}

//manual background reader task
static void manual_asynchronous_receive(void* arg){
    max_31856_t* max_31856 = (max_31856_t*)arg;

    while(!atomic_load_explicit(&max_31856->shutdown_requested, memory_order_relaxed)) {
        spi_transaction_t* completed_tx = NULL;
        esp_err_t err = spi_device_get_trans_result(
            max_31856->device_SPI_handle,
            &completed_tx,
            pdMS_TO_TICKS(100)
        );

        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }

        if(err == ESP_OK && completed_tx == &max_31856->static_temp_retrieve_tx) {
            store_reading_from_rx(max_31856);
            xSemaphoreGive(max_31856->in_flight);
            continue;
        }

        if (err == ESP_OK) {
            err = ESP_ERR_INVALID_STATE;
        }

        atomic_store_explicit(&max_31856->last_SPI_error, err, memory_order_relaxed);
        xSemaphoreGive(max_31856->in_flight);
    }

    max_31856->receiver_task = NULL;
    vTaskDelete(NULL);
}

static void auto_asynchronous_receive(void* arg){
    max_31856_t* max_31856 = (max_31856_t*)arg;

    while(!atomic_load_explicit(&max_31856->shutdown_requested, memory_order_relaxed)){
        vTaskDelay(max_31856->delay_ticks);

        if (atomic_load_explicit(&max_31856->shutdown_requested, memory_order_relaxed)) {
            break;
        }

        if (xSemaphoreTake(max_31856->in_flight, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        esp_err_t err = spi_device_transmit(max_31856->device_SPI_handle, &max_31856->static_temp_retrieve_tx);

        atomic_store_explicit(&max_31856->last_SPI_error, err, memory_order_relaxed);

        if(err == ESP_OK){
            store_reading_from_rx(max_31856);
        }

        xSemaphoreGive(max_31856->in_flight);
    }

    max_31856->receiver_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t max_31856_init(spi_host_device_t bus, const int cs_pin, max_31856_t* max_31856, max_31856_async_type async_type, uint16_t ms_refresh_rate){
    if (max_31856 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(max_31856, 0, sizeof(*max_31856));
    atomic_init(&max_31856->last_hotside_temp_reading, 0);
    atomic_init(&max_31856->last_coldside_temp_reading, 0);
    atomic_init(&max_31856->last_flags_reading, 0);
    atomic_init(&max_31856->last_SPI_error, ESP_OK);
    atomic_init(&max_31856->shutdown_requested, false);
    max_31856->async_type = async_type;

    max_31856->in_flight = xSemaphoreCreateBinary();
    if (max_31856->in_flight == NULL) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(max_31856->in_flight);
    max_31856->delay_ticks = pdMS_TO_TICKS(ms_refresh_rate);

    memset(&max_31856->static_temp_retrieve_tx, 0, sizeof(max_31856->static_temp_retrieve_tx));
    max_31856->static_temp_retrieve_tx.addr = 0x0A & 0x7F;
    max_31856->static_temp_retrieve_tx.rxlength = 8 * 6; //6-byte read
    max_31856->static_temp_retrieve_tx.rx_buffer = max_31856->rx_buffer;
    
    //create device configuration
    spi_device_interface_config_t device_config = {
        .clock_speed_hz = 5 * 1000 * 1000,   //5MHz
        .mode           = 1,                 //SPI mode 1
        .spics_io_num   = cs_pin,
        .queue_size     = 1,

        .flags          = SPI_DEVICE_HALFDUPLEX,

        .command_bits   = 0,
        .address_bits   = 8,
        .dummy_bits     = 0
    };

    //attatch device to bus
    ESP_LOGI(LOG_TAG, "attatching device to SPI bus");
    esp_err_t err = spi_bus_add_device(bus, &device_config, &max_31856->device_SPI_handle);
    if(err != ESP_OK){
        max_31856->device_SPI_handle = NULL;
        vSemaphoreDelete(max_31856->in_flight);
        max_31856->in_flight = NULL;
        return err;
    }
    
    //temperature limits
    const uint16_t max_tc = 600 * 16; //°C * 16
    const uint16_t min_tc = -10 * 16; //°C * 16
    const uint8_t max_cj = 50; //°C
    const uint8_t min_cj = -10; //°C

    //configuration settings
    const uint8_t conf_tx_data[] = {
        0b10100110,  //configuration register 0: {automatic mode, no one shot, open/short detection enabled, CJ enabled, comparator fault mode, clr faults, 60Hz rejection}
        0b00110011,  //configuration register 1: {8 sample average, K type probe}
        0b00000000,  //enable all fault conditions
        max_cj,      //max cold-side temp 
        min_cj,      //min cold-side temp
        (max_tc >> 8), (max_tc & 0xFF), //max temp
        (min_tc >> 8), (min_tc & 0xFF), //min temp
        0x00         //no cold junction offset
    };

    //write configuration registers
    ESP_LOGI(LOG_TAG, "writting configuration registers");
    spi_transaction_t conf_tx = {
        .addr       = 0x00 | 0x80, //start write at 0x00
        .tx_buffer  = conf_tx_data,
        .length     = 8 * sizeof(conf_tx_data),
        .rx_buffer  = NULL
    };
    err = spi_device_transmit(max_31856->device_SPI_handle, &conf_tx);
    if (err != ESP_OK) {
        max_31856_deinit(max_31856);
        return err;
    }

    //readback configuration registers
    ESP_LOGI(LOG_TAG, "verifying configuration registers");
    uint8_t rx_buf[sizeof(conf_tx_data)/sizeof(conf_tx_data[0])];
    spi_transaction_t conf_rx = {
        .addr       = 0x00 & 0x7F, //start read at 0x00
        .rxlength   = 8 * sizeof(conf_tx_data),
        .rx_buffer  = rx_buf,
    };
    err = spi_device_transmit(max_31856->device_SPI_handle, &conf_rx);
    if (err != ESP_OK) {
        max_31856_deinit(max_31856);
        return err;
    }

    //ensure that the configuration registers were set correctly
    bool config_correct = true;
    
    //mask off one-shot configuration registers
    uint8_t expected_data[sizeof(conf_tx_data)];
    memcpy(expected_data, conf_tx_data, sizeof(conf_tx_data));

    expected_data[0] &= 0b10111101; //one-shot and faultclr bits
    expected_data[1] = (conf_tx_data[1] & 0b01111111) | (rx_buf[1] & 0b10000000); //reserved field
    expected_data[2] = (conf_tx_data[2] & 0b00111111) | (rx_buf[2] & 0b11000000); //reserved field


    for(size_t x = 0; x < sizeof(conf_tx_data)/sizeof(conf_tx_data[0]); x++){
        
        if(expected_data[x] != rx_buf[x]){
            config_correct = false;
            ESP_LOGE(LOG_TAG, "Wrote: 0x%02X at reg %02X -> Expected: 0x%02X, Read: 0x%02X", conf_tx_data[x], x, expected_data[x], rx_buf[x]);
        } else {
            ESP_LOGI(LOG_TAG, "Wrote: 0x%02X at reg %02X -> Expected: 0x%02X, Read: 0x%02X", conf_tx_data[x], x, expected_data[x], rx_buf[x]);
        }
    }

    if(config_correct == false){
        ESP_LOGE(LOG_TAG, "Device did not respond correctly to SPI transmission");
        max_31856_deinit(max_31856);
        return ESP_ERR_INVALID_RESPONSE;
    }

    BaseType_t task_ok;
    if(async_type == MAX31856_MANUAL){
        ESP_LOGI(LOG_TAG, "starting max_31856 driver in manual refresh mode");
        task_ok = xTaskCreate(
            manual_asynchronous_receive,
            "max31856_rx",
            1024,
            max_31856,
            8,
            &max_31856->receiver_task
        );
    } else {
        ESP_LOGI(LOG_TAG, "starting max_31856 driver in automatic refresh mode");
        task_ok = xTaskCreate(
            auto_asynchronous_receive,
            "max31856_rx",
            1024,
            max_31856,
            8,
            &max_31856->receiver_task
        );
    }
    if (task_ok != pdPASS) {
        max_31856_deinit(max_31856);
        return ESP_ERR_NO_MEM;
    }

    //get the latest temperature reading
    ESP_LOGI(LOG_TAG, "initializing temperature reading");
    err = max_31856_update_temp_blocking(max_31856);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "failed to get initial temeprature reading");
        max_31856_deinit(max_31856);
        return err;
    }

    return ESP_OK;
}

esp_err_t max_31856_deinit(max_31856_t* max_31856) {
    if (max_31856 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    //signal the reciever to stop
    atomic_store_explicit(&max_31856->shutdown_requested, true, memory_order_relaxed);

    //can only delete an owned mutex so grab it
    if (max_31856->in_flight != NULL) {
        if (xSemaphoreTake(max_31856->in_flight, pdMS_TO_TICKS(500)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
    }

    //kill the reciever if it is stuck
    if (max_31856->receiver_task != NULL) {
        for (int i = 0; i < 50 && max_31856->receiver_task != NULL; i++) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (max_31856->receiver_task != NULL) {
            vTaskDelete(max_31856->receiver_task);
            max_31856->receiver_task = NULL;
        }
    }

    //remove the device from the spi bus
    esp_err_t err = ESP_OK;
    if (max_31856->device_SPI_handle != NULL) {
        err = spi_bus_remove_device(max_31856->device_SPI_handle);
        max_31856->device_SPI_handle = NULL;
    }

    //finally, delete the mutex
    if (max_31856->in_flight != NULL) {
        vSemaphoreDelete(max_31856->in_flight);
        max_31856->in_flight = NULL;
    }

    return err;
}

esp_err_t max_31856_update_temp_blocking(max_31856_t* max_31856){
    if (max_31856 == NULL || max_31856->device_SPI_handle == NULL || max_31856->in_flight == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    //special logic if auto mode is in use -> immediately read and update
    if (max_31856->async_type == MAX31856_AUTO) {
        if (xSemaphoreTake(max_31856->in_flight, 0) == pdFALSE) { //auto is currently reading -> just wait for it to finish
            if(xSemaphoreTake(max_31856->in_flight, pdMS_TO_TICKS(1000)) == pdFALSE){ // wait for it to finish
                return ESP_ERR_TIMEOUT;
            } else { //acquired the semaphore momentarily
                xSemaphoreGive(max_31856->in_flight);
                return atomic_load_explicit(&max_31856->last_SPI_error, memory_order_relaxed);
            }
        }

        //immediately read
        esp_err_t err = spi_device_transmit(max_31856->device_SPI_handle, &max_31856->static_temp_retrieve_tx);
        atomic_store_explicit(&max_31856->last_SPI_error, err, memory_order_relaxed);
        if (err == ESP_OK) {
            store_reading_from_rx(max_31856);
        }

        xSemaphoreGive(max_31856->in_flight);
        return err;
    }

    //Normal manual mode logic
    //check if an update is already queued
    if(xSemaphoreTake(max_31856->in_flight, 0) == pdFALSE) { //mutex was not acquired -> update already queued
        if (xSemaphoreTake(max_31856->in_flight, portMAX_DELAY) != pdTRUE) { //wait for the queued update to finish
            return ESP_ERR_TIMEOUT;
        }

        esp_err_t last_err = atomic_load_explicit(&max_31856->last_SPI_error, memory_order_relaxed);
        xSemaphoreGive(max_31856->in_flight);
        return last_err;
    }

    //no update queued, so queue one
    esp_err_t err = spi_device_queue_trans(max_31856->device_SPI_handle, &max_31856->static_temp_retrieve_tx, portMAX_DELAY);
    if(err != ESP_OK){
        xSemaphoreGive(max_31856->in_flight);
        return err;
    }

    //wait for the update to finish
    if (xSemaphoreTake(max_31856->in_flight, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    //collect the error response and return
    err = atomic_load_explicit(&max_31856->last_SPI_error, memory_order_relaxed);
    xSemaphoreGive(max_31856->in_flight);
    return err;
}

esp_err_t max_31856_update_temp_async(max_31856_t* max_31856, uint32_t timeout_ms){
    if (max_31856 == NULL || max_31856->device_SPI_handle == NULL || max_31856->in_flight == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    //update is already enqueued if auto is selected
    if (max_31856->async_type == MAX31856_AUTO) {
        return ESP_OK;
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    //check if a transmission is already queued
    if(xSemaphoreTake(max_31856->in_flight, 0) == pdFALSE) { //mutex was not acquired -> update already queued 
        return ESP_OK;
    }

    //queue a new transmission
    esp_err_t err = spi_device_queue_trans(max_31856->device_SPI_handle, &max_31856->static_temp_retrieve_tx, timeout_ticks);
    if(err != ESP_OK){
        xSemaphoreGive(max_31856->in_flight);
        return err;
    }

    return err;
}

float max_31856_get_temperature_c(max_31856_t* max_31856, uint8_t* flags, float* cold_junction_temp){
    if(max_31856 == NULL) return 0;

    if(flags != NULL) *flags = atomic_load_explicit(&max_31856->last_flags_reading, memory_order_relaxed);
    if(cold_junction_temp != NULL) *cold_junction_temp = atomic_load_explicit(&max_31856->last_coldside_temp_reading, memory_order_relaxed) / 64.0;
    return (float)atomic_load_explicit(&max_31856->last_hotside_temp_reading, memory_order_relaxed) / 128.0;
}

uint8_t max_31856_get_status(max_31856_t* max_31856){
    if(max_31856 == NULL) return ~0;

    return atomic_load_explicit(&max_31856->last_flags_reading, memory_order_relaxed);
}