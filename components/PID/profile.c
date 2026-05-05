// Implements profile helpers for temperature target generation
#include "include/profile.h"
#include <stdint.h>
#include <stdlib.h>

// tolerance in X128 units for considering temperature "reached" (0.5 degC)
static const int32_t TEMP_TOLERANCE_X128 = (int32_t)(0.5 * 128);

// get_target_temperature
// - `elapsed_ms` is the elapsed milliseconds since the start of this key-point
// - `start_temp` is the starting temperature expressed in X128 fixed-point
// - returns the desired temperature (X128 fixed-point) for the given elapsed time
uint32_t get_target_temperature(uint32_t elapsed_ms, uint32_t start_temp, key_point_t* next_point){
    if (next_point == NULL) {
        return start_temp;
    }

    int64_t start = (int64_t)start_temp;
    int64_t target = (int64_t)next_point->temperature_X128;

    switch (next_point->type) {
        case POINT_RATE: {
            // rate_or_time is in m°C per second (signed)
            int64_t rate_mC_per_s = (int64_t)next_point->rate_or_time; // m°C/s
            // delta X128 = rate_mC_per_s * elapsed_ms * 128 / 1000
            int64_t delta_x128 = (rate_mC_per_s * (int64_t)elapsed_ms * 128) / 1000;
            int64_t desired = start + delta_x128;
            // clamp to not pass the target
            if (start < target) {
                if (desired > target) desired = target;
            } else {
                if (desired < target) desired = target;
            }
            if (desired < 0) desired = 0;
            return (uint32_t)desired;
        }
        case POINT_TIME: {
            // rate_or_time is time in ms to reach target (or dwell time if start==target)
            int64_t duration_ms = llabs((long long)next_point->rate_or_time);
            if (duration_ms == 0) {
                // immediate
                return (uint32_t)target;
            }
            // fraction (capped to 1.0)
            double frac = (double)elapsed_ms / (double)duration_ms;
            if (frac < 0.0) frac = 0.0;
            if (frac > 1.0) frac = 1.0;
            int64_t desired = start + (int64_t)((target - start) * frac);
            if (desired < 0) desired = 0;
            return (uint32_t)desired;
        }
        case POINT_FAST:
        default:
            // jump directly to target (controller will handle approach)
            return (uint32_t)target;
    }
}

// next_point_if_satisfied
// - `elapsed_ms` is the elapsed milliseconds since the current point was started
// - `current_temperature` is expressed in X128 fixed-point
// - returns pointer to the point that should be used for the next iteration (same point if not yet satisfied)
key_point_t* next_point_if_satisfied(uint32_t elapsed_ms, uint32_t current_temperature, profile_t* current_profile, uint16_t chased_point_index){
    if (current_profile == NULL || current_profile->points == NULL) {
        return NULL;
    }

    if (chased_point_index >= current_profile->num_points) {
        return NULL;
    }

    key_point_t* point = &current_profile->points[chased_point_index];
    int64_t cur = (int64_t)current_temperature;
    int64_t target = (int64_t)point->temperature_X128;

    switch (point->type) {
        case POINT_RATE: {
            // satisfied when we've reached or passed the target in the requested direction
            if (cur >= target - TEMP_TOLERANCE_X128 && cur <= target + TEMP_TOLERANCE_X128) {
                // reached target
                if ((size_t)(chased_point_index + 1) < current_profile->num_points) {
                    return &current_profile->points[chased_point_index + 1];
                }
                return NULL;
            }
            // not yet reached
            return point;
        }
        case POINT_TIME: {
            // For time points we treat them as a dwell at the target temperature.
            // Only consider the point satisfied if the current temperature is within tolerance
            // of the target and the elapsed time exceeds the requested duration.
            int64_t duration_ms = llabs((long long)point->rate_or_time);
            if (cur >= target - TEMP_TOLERANCE_X128 && cur <= target + TEMP_TOLERANCE_X128) {
                if (duration_ms == 0 || elapsed_ms >= (uint32_t)duration_ms) {
                    if ((size_t)(chased_point_index + 1) < current_profile->num_points) {
                        return &current_profile->points[chased_point_index + 1];
                    }
                    return NULL;
                }
            }
            return point;
        }
        case POINT_FAST:
        default: {
            // satisfied when we reach the target (within tolerance)
            if (cur >= target - TEMP_TOLERANCE_X128 && cur <= target + TEMP_TOLERANCE_X128) {
                if ((size_t)(chased_point_index + 1) < current_profile->num_points) {
                    return &current_profile->points[chased_point_index + 1];
                }
                return NULL;
            }
            return point;
        }
    }
}
