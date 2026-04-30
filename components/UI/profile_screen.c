#include "UI.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const profile_t* const PROFILE_OPTIONS[] = {
    &lead_free_profile,
    &leaded_solder_profile,
};

typedef struct profile_screen_state_t {
    ui_t* ui_data;
    lv_obj_t* summary_label;
    lv_obj_t* cards[2];
    lv_obj_t* detail_labels[2];
    size_t selected_index;
} profile_screen_state_t;

static profile_screen_state_t profile_screen_state = {
    .ui_data = NULL,
    .summary_label = NULL,
    .cards = {NULL, NULL},
    .detail_labels = {NULL, NULL},
    .selected_index = 0,
};

static float profile_max_temperature_c(const profile_t* profile) {
    float max_temperature_c = 0.0f;

    if (profile == NULL || profile->points == NULL || profile->num_points == 0) {
        return max_temperature_c;
    }

    max_temperature_c = (float)profile->points[0].temperature_X128 / 128.0f;
    for (uint16_t i = 1; i < profile->num_points; ++i) {
        float temperature_c = (float)profile->points[i].temperature_X128 / 128.0f;
        if (temperature_c > max_temperature_c) {
            max_temperature_c = temperature_c;
        }
    }

    return max_temperature_c;
}

static int32_t profile_nominal_rate_mC_per_s(const profile_t* profile) {
    int64_t rate_sum = 0;
    uint16_t rate_count = 0;

    if (profile == NULL || profile->points == NULL) {
        return 5000;
    }

    for (uint16_t i = 0; i < profile->num_points; ++i) {
        if (profile->points[i].type == POINT_RATE && profile->points[i].rate_or_time > 0) {
            rate_sum += profile->points[i].rate_or_time;
            rate_count++;
        }
    }

    if (rate_count == 0) {
        return 5000;
    }

    return (int32_t)(rate_sum / rate_count);
}

static uint32_t profile_estimated_time_ms(const profile_t* profile) {
    const int32_t ambient_temperature_mC = 25 * 1000;
    int32_t previous_temperature_mC = ambient_temperature_mC;
    uint64_t total_time_ms = 0;
    int32_t nominal_rate_mC_per_s = profile_nominal_rate_mC_per_s(profile);

    if (profile == NULL || profile->points == NULL) {
        return 0;
    }

    for (uint16_t i = 0; i < profile->num_points; ++i) {
        const key_point_t* point = &profile->points[i];
        int32_t target_temperature_mC = (int32_t)(point->temperature_X128 * 1000 / 128);
        int32_t delta_temperature_mC = target_temperature_mC - previous_temperature_mC;

        switch (point->type) {
            case POINT_TIME:
                total_time_ms += (uint32_t)llabs((long long)point->rate_or_time);
                break;
            case POINT_RATE:
                if (point->rate_or_time != 0) {
                    total_time_ms += (uint32_t)((llabs((long long)delta_temperature_mC) * 1000LL) / llabs((long long)point->rate_or_time));
                }
                break;
            case POINT_FAST:
                if (nominal_rate_mC_per_s > 0) {
                    total_time_ms += (uint32_t)((llabs((long long)delta_temperature_mC) * 1000LL) / nominal_rate_mC_per_s);
                }
                break;
        }

        previous_temperature_mC = target_temperature_mC;
    }

    return (uint32_t)total_time_ms;
}

static void format_duration(uint32_t duration_ms, char* buffer, size_t buffer_size) {
    uint32_t total_seconds = duration_ms / 1000;
    uint32_t minutes = total_seconds / 60;
    uint32_t seconds = total_seconds % 60;
    snprintf(buffer, buffer_size, "%02u:%02u", (unsigned)minutes, (unsigned)seconds);
}

static void update_profile_selection(size_t selected_index) {
    profile_screen_state.selected_index = selected_index;

    if (profile_screen_state.ui_data != NULL) {
        profile_screen_state.ui_data->selected_profile = PROFILE_OPTIONS[selected_index];
    }

    lv_label_set_text_fmt(profile_screen_state.summary_label, "Selected:\n %s", PROFILE_OPTIONS[selected_index]->name);

    for (size_t i = 0; i < 2; ++i) {
        if (i == selected_index) {
            lv_obj_add_state(profile_screen_state.cards[i], LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(profile_screen_state.cards[i], LV_STATE_CHECKED);
        }
    }
}

static void profile_card_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    size_t selected_index = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (selected_index >= 2) {
        return;
    }

    update_profile_selection(selected_index);
}

static lv_obj_t* create_profile_card(lv_obj_t* base, const profile_t* profile, size_t index) {
    char details_buffer[64];
    char duration_buffer[16];

    format_duration(profile_estimated_time_ms(profile), duration_buffer, sizeof(duration_buffer));
    snprintf(details_buffer, sizeof(details_buffer), "Max: %.1f Est: %s", profile_max_temperature_c(profile), duration_buffer);

    lv_obj_t* card = lv_btn_create(base);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1C2330), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0x3A465C), LV_PART_MAIN);
    lv_obj_set_style_text_color(card, lv_color_hex(0xF3F7FF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2F7BF6), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(card, lv_color_hex(0xA8C5FF), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_event_cb(card, profile_card_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)index);

    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title_label = lv_label_create(card);
    lv_label_set_text(title_label, profile->name);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_LEFT, 0);

    lv_obj_t* detail_label = lv_label_create(card);
    lv_label_set_text(detail_label, details_buffer);
    lv_obj_set_style_text_color(detail_label, lv_color_hex(0xD6DEE9), 0);

    profile_screen_state.detail_labels[index] = detail_label;
    return card;
}

void initialize_profiles_screen(lv_obj_t* base, ui_t* ui_data){
    profile_screen_state.ui_data = ui_data;
    profile_screen_state.selected_index = 0;
    profile_screen_state.summary_label = NULL;
    profile_screen_state.cards[0] = NULL;
    profile_screen_state.cards[1] = NULL;
    profile_screen_state.detail_labels[0] = NULL;
    profile_screen_state.detail_labels[1] = NULL;

    lv_obj_set_layout(base, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(base, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(base, LV_DIR_VER);
    lv_obj_set_style_pad_all(base, 12, 0);
    lv_obj_set_style_pad_row(base, 10, 0);
    lv_obj_set_style_bg_color(base, lv_color_hex(0x10151F), 0);

    profile_screen_state.summary_label = lv_label_create(base);
    lv_obj_set_style_text_color(profile_screen_state.summary_label, lv_color_hex(0x9FB3C8), 0);

    profile_screen_state.cards[0] = create_profile_card(base, PROFILE_OPTIONS[0], 0);
    profile_screen_state.cards[1] = create_profile_card(base, PROFILE_OPTIONS[1], 1);

    update_profile_selection(0);
}