if(CONFIG_FLOAT_NODE_SENSOR_BMP280)
    set(SENSOR_COMPONENT "float_sensor_bmp280")
elseif(CONFIG_FLOAT_NODE_SENSOR_AHT30)
    set(SENSOR_COMPONENT "float_sensor_aht30")
else()
    set(SENSOR_COMPONENT "float_sensor_mock")
endif()

idf_component_register(SRCS "float_node.c"
                    INCLUDE_DIRS "."
                    REQUIRES float_data float_sensor ${SENSOR_COMPONENT} float_now float_blink esp_wifi nvs_flash esp_driver_i2c)
