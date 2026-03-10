idf_component_register(SRCS "float_sensor_bmp280.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "driver" "float_sensor"
                    PRIV_REQUIRES "esp_timer")
