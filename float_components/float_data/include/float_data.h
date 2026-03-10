#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef enum float_sensor_type_e {
    FLOAT_SENSOR_TYPE_TEMPERATURE = 0,
    FLOAT_SENSOR_TYPE_HUMIDITY,
    FLOAT_SENSOR_TYPE_PRESSURE,
    FLOAT_SENSOR_TYPE_MAX
} float_sensor_type_t;

typedef float float_sensor_datatype_t;

typedef struct __attribute__((packed)) float_datapoint_s {
    uint8_t reading_type;
    float_sensor_datatype_t value;
} float_datapoint_t;

typedef struct __attribute__((packed)) float_datapoints_s {
    uint8_t num_datapoints;
    float_datapoint_t datapoints[];
} float_datapoints_t;

typedef float_datapoints_t *float_datapoints_handle_t;

esp_err_t float_datapoints_new( float_datapoints_handle_t *datapoints_handle, uint8_t num_datapoints );
int float_datapoints_calculate_size( uint8_t num_datapoints );
