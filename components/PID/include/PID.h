//Declares the functions that our PID loop uses
#include <stdint.h>

//use feedforward PID algorithm 
uint8_t get_PID_setpoint(uint32_t requested_hotside_temperature, uint32_t current_hotside_temperature, uint32_t current_coldside_temperature, uint32_t previous_time, uint8_t previous_setpoint);

// Called from the PCNT event callback to compute a new PID output and store
// it into the PCNT user-context provided when registering callbacks.
// - `user_ctx` is an opaque pointer supplied to `pcnt_unit_register_event_callbacks` (must point to a struct containing a writable `uint8_t heater_output` field)
// - `requested_temp_c` and `current_temp_c` are temperatures in degrees Celsius (float)
void PID_on_pcnt_event(void* user_ctx, float requested_temp_c, float current_temp_c);