#pragma once

#include "esp_err.h"
#include "float_sensor.h"

typedef struct float_sensor_mock_s float_sensor_mock_t;
typedef float_sensor_mock_t *float_sensor_mock_handle_t;

struct float_sensor_mock_s {
    float_sensor_t base;
};

esp_err_t float_sensor_new_mock(float_sensor_handle_t *handle);
