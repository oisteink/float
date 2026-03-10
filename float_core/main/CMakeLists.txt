idf_component_register(SRCS "float_core.c" "cmd_system_common.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_event float_now float_registry float_blink float_node_list console msp3520
                    PRIV_REQUIRES spi_flash)
