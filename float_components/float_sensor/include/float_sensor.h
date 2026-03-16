#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "float_data.h"

typedef struct float_sensor_s float_sensor_t;
typedef float_sensor_t *float_sensor_handle_t;

typedef struct float_sensor_channel_s {
    float_sensor_class_t sensor_class;
    esp_err_t ( *read )( float_sensor_handle_t sensor, float_sensor_datatype_t *out );
} float_sensor_channel_t;

#define FLOAT_SENSOR_MAX_CHANNELS 8

struct float_sensor_s {
    esp_err_t ( *initialize )( float_sensor_handle_t sensor );
    uint8_t num_channels;
    float_sensor_channel_t channels[FLOAT_SENSOR_MAX_CHANNELS];
};

esp_err_t float_sensor_init( float_sensor_handle_t sensor );
esp_err_t float_sensor_read_data( float_sensor_handle_t sensor, float_datapoints_handle_t *datapoints );
esp_err_t float_sensor_get_classes( float_sensor_handle_t sensor, float_sensor_class_t *out_classes, uint8_t *out_count );
