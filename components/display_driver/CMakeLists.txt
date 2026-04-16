idf_component_register(SRCS "display_driver.c" "lvgl_port.c"
                    REQUIRES esp_driver_spi lvgl esp_lvgl_adapter esp_lcd_ili9341 esp_lcd_touch esp_lcd_touch_xpt2046
                    INCLUDE_DIRS "include")
