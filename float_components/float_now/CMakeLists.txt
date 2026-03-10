idf_component_register(SRCS "float_now.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash float_data )
