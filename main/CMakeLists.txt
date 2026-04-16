idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ""
                       REQUIRES max_31856_driver display_driver)
