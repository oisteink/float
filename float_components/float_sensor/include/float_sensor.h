#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "float_data.h"

typedef struct float_sensor_s float_sensor_t;
typedef float_sensor_t *float_sensor_handle_t;

struct float_sensor_s {
    esp_err_t ( *initialize )( float_sensor_handle_t sensor );
    esp_err_t ( *read_temperature )( float_sensor_handle_t sensor, float_sensor_datatype_t *out_temp );
    esp_err_t ( *read_humidity )( float_sensor_handle_t sensor, float_sensor_datatype_t *out_humidity );
    esp_err_t ( *read_pressure )( float_sensor_handle_t sensor, float_sensor_datatype_t *out_pressure );
};

esp_err_t float_sensor_init( float_sensor_handle_t sensor );
esp_err_t float_sensor_read_data( float_sensor_handle_t sensor, float_datapoints_handle_t *datapoints );
esp_err_t float_sensor_read_temperature( float_sensor_handle_t sensor, float_sensor_datatype_t *out_temp );

esp_err_t float_sensor_read_humidity( float_sensor_handle_t sensor, float_sensor_datatype_t *out_humidity );
esp_err_t float_sensor_read_pressure( float_sensor_handle_t sensor, float_sensor_datatype_t *out_pressure );
