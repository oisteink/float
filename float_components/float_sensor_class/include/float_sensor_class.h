#pragma once

#include <stdint.h>

typedef enum float_sensor_class_e {
    FLOAT_SENSOR_CLASS_AIR_TEMPERATURE = 0,
    FLOAT_SENSOR_CLASS_WATER_TEMPERATURE,
    FLOAT_SENSOR_CLASS_AIR_HUMIDITY,
    FLOAT_SENSOR_CLASS_AIR_PRESSURE,
    FLOAT_SENSOR_CLASS_MAX
} float_sensor_class_t;

#define FLOAT_MAX_SENSOR_CLASSES 8

typedef struct float_sensor_class_info_s {
    float_sensor_class_t sensor_class;
    const char *label;
    const char *unit;
    const char *format;
    float range_min;
    float range_max;
} float_sensor_class_info_t;

const float_sensor_class_info_t *float_sensor_class_get_info( float_sensor_class_t sensor_class );
