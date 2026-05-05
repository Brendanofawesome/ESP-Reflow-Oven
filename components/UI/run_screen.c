#include "UI.h"
<<<<<<< Updated upstream
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

=======
#include <stdio.h>
#include "fonts/fontawesome_solid.c"

static void run_button_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t* btn = lv_event_get_target(e);
    ui_t* ui_data = (ui_t*)lv_event_get_user_data(e);
    if (ui_data == NULL) return;

    // toggle active state
    ui_data->active = !ui_data->active;

    // update icon and text labels (first two children of button)
    lv_obj_t* icon_label = lv_obj_get_child(btn, 0);
    lv_obj_t* text_label = lv_obj_get_child(btn, 1);
    
    if (icon_label != NULL) {
        if (ui_data->active) {
            lv_label_set_text_static(icon_label, "\uf04d"); // stop icon
        } else {
            lv_label_set_text_static(icon_label, "\uf04b"); // play/start icon
        }
    }
    
    if (text_label != NULL) {
        if (ui_data->active) {
            lv_label_set_text_static(text_label, "Stop");
        } else {
            lv_label_set_text_static(text_label, "Start");
        }
    }
}

void initialize_run_screen(lv_obj_t* base, ui_t* ui_data){
    if (base == NULL || ui_data == NULL) return;

    lv_obj_set_layout(base, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(base, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(base, LV_DIR_NONE);
>>>>>>> Stashed changes


<<<<<<< Updated upstream
=======
    // Profile summary
    lv_obj_t* summary = lv_label_create(base);
    lv_obj_set_style_text_color(summary, lv_color_hex(0x9FB3C8), 0);
    if (ui_data->selected_profile != NULL) {
        lv_label_set_text_fmt(summary, "Profile:\n%s", ui_data->selected_profile->name);
    } else {
        lv_label_set_text_static(summary, "No profile selected");
    }
    lv_obj_set_style_text_align(summary, LV_TEXT_ALIGN_CENTER, 0);

    // Large start/stop button
    lv_obj_t* run_btn = lv_btn_create(base);
    lv_obj_set_width(run_btn, LV_PCT(80));
    lv_obj_set_height(run_btn, 80);
    lv_obj_set_style_radius(run_btn, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(run_btn, lv_color_hex(0x2F7BF6), LV_PART_MAIN);
    lv_obj_set_style_text_color(run_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    // Set button to flex layout for icon + text
    lv_obj_set_layout(run_btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(run_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(run_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icon label (fontawesome)
    lv_obj_t* icon_label = lv_label_create(run_btn);
    lv_obj_set_style_text_font(icon_label, &fontawesome_solid, 0);
    if (ui_data->active) {
        lv_label_set_text_static(icon_label, "\uf04d"); // stop icon
    } else {
        lv_label_set_text_static(icon_label, "\uf04b"); // play/start icon
    }

    // Text label
    lv_obj_t* text_label = lv_label_create(run_btn);
    if (ui_data->active) {
        lv_label_set_text_static(text_label, "Stop");
    } else {
        lv_label_set_text_static(text_label, "Start");
    }

    lv_obj_add_event_cb(run_btn, run_button_event_cb, LV_EVENT_CLICKED, ui_data);

    // ensure ui_data active state is initialized
    ui_data->active = false;
}
>>>>>>> Stashed changes
