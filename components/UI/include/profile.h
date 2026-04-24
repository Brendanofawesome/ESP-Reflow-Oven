//data definitions and common functions for manipulating profiles
#pragma once
#include <stdint.h>

//points can either:
//  request a constant rate of change until their requested temperature
//  request a constant elapsed time until their requested temperature
//  request to reach their requested temperature as fast as possible
typedef enum {POINT_RATE, POINT_TIME, POINT_FAST} key_point_type_e;

typedef struct key_point_t {
    uint32_t temperature_X128; //represents the temperature in fixed point format where LSB is 2^-7 (matches temperature sensor)

    key_point_type_e type;
    int32_t rate_or_time; //units are m°C (signed) or ms
} key_point_t;

typedef struct profile_t {
    char* name;

    key_point_t* points;
    uint16_t num_points;
} profile_t;


//returns the target temperature
uint32_t get_target_temperature(uint32_t start_time, uint32_t start_temp, key_point_t* next_point);

//gets the point that should be used for the next iteration
key_point_t* point_satisfied(uint32_t start_time, uint32_t current_temperature, profile_t* current_profile, uint16_t chased_point_index);