#include "UI.h"
#include "stdio.h"
#include "stdlib.h"


typedef struct run_screen_state_t {
    ui_t* ui_data;
    lv_obj_t* status_label;
    lv_obj_t* run_time;
    lv_obj_t* target_temp;
    lv_obj_t* elapsed_time;

    lv_obj_t* stop_button;


 
} run_screen_state_t; 

static run_screen_state_t run_screen_state = {
    .ui_data = NULL,
    .status_label = NULL,
    .profile_label = NULL,
    .current_temp_label = NULL,
    .target_temp_label = NULL,
    .elapsed_time_label = NULL,
    .stop_button = NULL,
    .active_profile_index = 0,
    .is_running = false,
};



