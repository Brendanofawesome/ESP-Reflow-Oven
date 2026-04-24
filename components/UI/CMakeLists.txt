idf_component_register(SRCS "fonts/consolas_bold.c" "fonts/fontawesome_solid.c" "UI_init.c" "default_profiles.c" "profile_screen.c" "run_screen.c" "settings_screen.c"
                    REQUIRES lvgl esp_common 
                    INCLUDE_DIRS "include")
