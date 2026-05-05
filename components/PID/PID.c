#include <stdio.h>
#include "PID.h"

#include <stdint.h>
#include "esp_timer.h"

uint8_t get_PID_setpoint(uint32_t requested_hotside_temperature, uint32_t current_hotside_temperature, uint32_t current_coldside_temperature, uint32_t previous_time, uint8_t previous_setpoint){
	// Simple feedforward + PID controller producing an 8-bit heater setpoint (0-255)
	// Assumptions:
	// - `previous_time` is the elapsed time in milliseconds since the last call (delta t)
	// - Setpoint output range is 0..255 mapped roughly to 0..300 degC
	// - This implementation keeps internal integral and previous error state between calls

	// Tunable gains (adjust to your oven)
	const double Kp = 2.0;      // proportional gain (per degC)
	const double Ki = 0.05;     // integral gain (per degC-second)
	const double Kd = 1.0;      // derivative gain (per degC/second)
    
	const double Kff = 0.6;     // feedforward gain (multiplies requested temperature contribution)

	// Safety / scaling
	const double TEMP_MAX = 300.0; // maximum expected temperature for scaling
	const double OUT_MAX = 255.0;  // max output value (uint8_t)
	const double SCALE = OUT_MAX / TEMP_MAX;

	// static controller state preserved across calls
	static double integral = 0.0;
	static double prev_error = 0.0;

	// convert inputs to double
	double req = (double)requested_hotside_temperature;
	double cur = (double)current_hotside_temperature;

	// compute time step in seconds
	double dt = (previous_time > 0) ? ((double)previous_time / 1000.0) : 0.1;
	if (dt <= 0.0) dt = 0.1;

	// error (degC)
	double error = req - cur;

	// proportional term (converted to output units)
	double P_out = Kp * error * SCALE;

	// integral term (accumulate error over time) with anti-windup clamping
	integral += error * dt;
	const double INTEGRAL_MAX = 1000.0; // clamp integral (in degC*sec)
	if (integral > INTEGRAL_MAX) integral = INTEGRAL_MAX;
	if (integral < -INTEGRAL_MAX) integral = -INTEGRAL_MAX;
	double I_out = Ki * integral * SCALE;

	// derivative term (rate of change of error)
	double derivative = (error - prev_error) / dt;
	double D_out = Kd * derivative * SCALE;

	// simple feedforward: base heater drive proportional to requested temperature
	double FF_out = Kff * req * SCALE;

	// combine terms
	double desired = P_out + I_out + D_out + FF_out;

	// keep desired in 0..OUT_MAX
	if (desired < 0.0) desired = 0.0;
	if (desired > OUT_MAX) desired = OUT_MAX;

	// rate-limit changes relative to previous_setpoint to avoid sudden jumps
	const double MAX_DELTA_PER_SEC = 200.0; // units per second
	double max_delta = MAX_DELTA_PER_SEC * dt;
	double prev = (double)previous_setpoint;
	double delta = desired - prev;
	if (delta > max_delta) desired = prev + max_delta;
	if (delta < -max_delta) desired = prev - max_delta;

	// update previous error for next derivative calculation
	prev_error = error;

	// final output as uint8_t
	return (uint8_t)(desired + 0.5);
}

// Handler invoked from PCNT event context (or a wrapper called from it).
// user_ctx is expected to point to a struct with a writable `uint8_t heater_output` field.
void PID_on_pcnt_event(void* user_ctx, float requested_temp_c, float current_temp_c){
	static int64_t last_time_us = 0;
	static uint8_t prev_output = 0;

	int64_t now = esp_timer_get_time(); // microseconds
	uint32_t dt_ms = 100; // default if first call
	if (last_time_us != 0) {
		int64_t delta_us = now - last_time_us;
		if (delta_us < 0) delta_us = 0;
		dt_ms = (uint32_t)(delta_us / 1000);
		if (dt_ms == 0) dt_ms = 1;
	}
	last_time_us = now;

	// Convert floats to integer degrees for existing get_PID_setpoint API
	uint32_t req = (uint32_t)requested_temp_c;
	uint32_t cur = (uint32_t)current_temp_c;

	uint8_t out = get_PID_setpoint(req, cur, 0, dt_ms, prev_output);

	// write the output into user context if possible
	if (user_ctx != NULL) {
		// common layout: first byte is heater_output
		uint8_t* p = (uint8_t*)user_ctx;
		*p = out;
	}

	prev_output = out;
}