#pragma once
#include <stdatomic.h>          //to ensure that temperature readings are atomically stored
#include "driver/spi_master.h"
#include "esp_err.h"


//defines the structure that stores information about the sensor
typedef enum : bool {MANUAL, AUTO} max_31856_async_type;
typedef struct max_31856_t {
    //data definitions
    spi_device_handle_t device_SPI_handle;
    atomic_int_least32_t last_temp_reading;
    atomic_uint_least8_t last_flags_reading;
    _Atomic esp_err_t last_SPI_error;

    //asynchronous definitions
    max_31856_async_type async_type;
    spi_transaction_t static_temp_retrieve_tx;
    TaskHandle_t receiver_task;
    atomic_bool shutdown_requested;

    //queue synchronization
    SemaphoreHandle_t in_flight;

    //Auto Async
    uint32_t delay_ticks;
} max_31856_t;

//initialize a max_31856 struct and verify connection to the SPI device
esp_err_t max_31856_init(spi_host_device_t bus, const int cs_pin, max_31856_t* max_31856, max_31856_async_type async_type);

//release resources and detach the sensor from SPI bus
esp_err_t max_31856_deinit(max_31856_t* max_31856);

//read the temperature from a max_31856 device in a blocking manner
esp_err_t max_31856_update_temp_blocking(max_31856_t* max_31856);

//enqueue an asynchronous read request; driver task will update last reading automatically
esp_err_t max_31856_update_temp_async(max_31856_t* max_31856, uint32_t timeout_ms);

//get the current flags
uint8_t max_31856_get_status(max_31856_t* max_31856);

//get latest temperature
float max_31856_get_temperature_c(max_31856_t* max_31856, uint8_t* flags);