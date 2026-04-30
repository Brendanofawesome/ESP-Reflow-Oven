//defines the list of default temperature profiles
#include "profile.h"

static key_point_t lead_free_points[] = {
    {150 * 128, POINT_RATE, 2.5 * 1000},   // Pre-heat
    {180 * 128, POINT_RATE, 7.5 * 1000},    // Soak
    {245 * 128, POINT_FAST, 0},             // Reflow Ramp-up
    {245 * 128, POINT_TIME, 30 * 1000},     // Reflow Dwell
    {100 * 128, POINT_RATE, -3 * 1000}      // Reflow Ramp-down
};

static key_point_t leaded_solder_points[] = {
    {125 * 128, POINT_RATE, 2.5 * 1000},   // Pre-heat
    {180 * 128, POINT_RATE, 7.5 * 1000},    // Pre-flow
    {230 * 128, POINT_FAST, 0},            // Reflow Ramp-up
    {230 * 128, POINT_TIME, 20 * 1000},    // Reflow Dwell
    {100 * 128, POINT_RATE, -3 * 1000}     // Reflow Ramp-down
};

const profile_t lead_free_profile = {
    .name = "leadfree (SAC305)",
    .points = lead_free_points,
    .num_points = sizeof(lead_free_points) / sizeof(lead_free_points[0])
};

const profile_t leaded_solder_profile = {
    .name = "leaded (Sn63Pb37)",
    .points = leaded_solder_points,
    .num_points = sizeof(leaded_solder_points) / sizeof(leaded_solder_points[0])
};