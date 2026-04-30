#pragma once
#include "esp_err.h"
#include "lvgl.h"
#include <stdint.h>
//defines and runs the UI

//ensure that application data is static for the lifetime of the UI
typedef struct ui_t {
    //internal state
    lv_timer_t* ui_updater;
    lv_obj_t* current_temperature_label;
    lv_obj_t* target_temperature_label;
    lv_obj_t* content;
} ui_t;

#define TOPBAR_HEIGHT 50
#define BOTTOMBAR_HEIGHT 50

//initialize UI elements. Assumes lv_lock is owned.
esp_err_t ui_init(ui_t* ui_data);

//set the latest current hotside temperature used by the label updater.
esp_err_t ui_set_current_temperature(ui_t* ui_data, const float temperature_c);

//set the latest target hotside temperature used by the label updater.
esp_err_t ui_set_target_temperature(ui_t* ui_data, const float temperature_c);

/* symbols
pause: f04c 
thermometer: f769 
play: f04b 
stop: f04d 
target: f140 
folder: f07b 
folder_open: f07c 
gear: f013 
*/