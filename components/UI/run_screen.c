#include "UI.h"

void initialize_run_screen(lv_obj_t* base, ui_t* ui_data){
    lv_obj_set_layout(base, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(base, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(base, LV_DIR_NONE);

    lv_obj_set_style_pad_all(base, 12, 0);
    lv_obj_set_style_pad_row(base, 10, 0);
    lv_obj_set_style_bg_color(base, lv_color_hex(0x10151F), 0);

    ui_data->active = false;
}