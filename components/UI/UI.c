#include "UI.h"
#include "esp_log.h"
#include "fontawesome_solid.c"
#include "consolas_bold.c"

const char* LOG_TAG = "UI_Controller";

esp_err_t ui_init(ui_t* ui_data){
    if(ui_data == NULL){
        ESP_LOGE(LOG_TAG, "UI init called with null data handle");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(LOG_TAG, "Started LVGL Screen Initialization");

    lv_obj_t* screen = lv_screen_active(); //get the default screen
    if(screen == NULL){
        screen = lv_obj_create(NULL);
    }

    LV_FONT_DECLARE(consolas_bold);
    LV_FONT_DECLARE(fontawesome_solid);
    
    ESP_LOGI(LOG_TAG, "Creating UI Containers");

    //main screen format settings
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100)); //make the screen fill the display
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_text_font(screen, &consolas_bold, 0);

    //create the content container
    lv_obj_t* tabview = lv_tabview_create(screen);
    lv_obj_set_width(tabview, LV_PCT(100)); //fill width
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP); //navigation using top bar
    lv_tabview_set_tab_bar_size(tabview, TOPBAR_HEIGHT); //top bar size
    lv_obj_set_flex_grow(tabview, 1); //container fills screen

    //Create the pages
    lv_tabview_add_tab(tabview, "Profiles");
    lv_tabview_add_tab(tabview, "Run");
    lv_tabview_add_tab(tabview, "Settings");

    //create the bottom bar
    lv_obj_t* bottom_bar = lv_obj_create(screen);
    lv_obj_set_width(bottom_bar, LV_PCT(100));
    lv_obj_set_height(bottom_bar, BOTTOMBAR_HEIGHT);

    ESP_LOGI(LOG_TAG, "Furnishing Bottom Bar");
    lv_obj_set_layout(bottom_bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bottom_bar, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(bottom_bar, LV_OBJ_FLAG_SCROLLABLE);

    /*lv_obj_t* state_indicator = lv_label_create(bottom_bar);
    lv_obj_set_style_text_font(state_indicator, &fontawesome_solid, 0);
    lv_label_set_text_static(state_indicator, "");
    lv_obj_set_flex_grow(state_indicator, 1);*/

    //current temperature
    lv_obj_t* thermometer_symbol = lv_label_create(bottom_bar);
    lv_obj_set_style_text_font(thermometer_symbol, &fontawesome_solid, 0);
    lv_label_set_text_static(thermometer_symbol, "");
    lv_obj_align(thermometer_symbol, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* current_temperature_label = lv_label_create(bottom_bar);
    lv_obj_set_flex_grow(current_temperature_label, 1);
    lv_obj_align(current_temperature_label, LV_ALIGN_LEFT_MID, 0, 0);

    //target temperature
    lv_obj_t* target_indicator = lv_label_create(bottom_bar);
    lv_obj_set_style_text_font(target_indicator, &fontawesome_solid, 0);
    lv_label_set_text_static(target_indicator, "");
    lv_obj_align(current_temperature_label, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t* target_temperature_label = lv_label_create(bottom_bar);
    lv_obj_align(current_temperature_label, LV_ALIGN_RIGHT_MID, 0, 0);

    ESP_LOGI(LOG_TAG, "Finished Creating Content");

    ui_data->content = tabview;
    ui_data->target_temperature_label = target_temperature_label;
    ui_data->current_temperature_label = current_temperature_label;

    return ESP_OK;
}

esp_err_t ui_set_current_temperature(ui_t* ui_data, const float temperature_c){
    if(ui_data == NULL){
        ESP_LOGE(LOG_TAG, "Set current temperature called with null data handle");
        return ESP_ERR_INVALID_ARG;
    }

    lv_label_set_text_fmt(ui_data->current_temperature_label, "%3.1f°C", temperature_c);
    return ESP_OK;
}

esp_err_t ui_set_target_temperature(ui_t* ui_data, const float temperature_c){
    if(ui_data == NULL){
        ESP_LOGE(LOG_TAG, "Set target temperature called with null data handle");
        return ESP_ERR_INVALID_ARG;
    }

    lv_label_set_text_fmt(ui_data->target_temperature_label, "%3.1f°C", temperature_c);
    return ESP_OK;
}