#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "float_sensor.h"

#define AHT30_SENSOR_TIMEOUT   1000

#define DEFAULT_FLOAT_SENSOR_AHT30_CONFIG { .device_address = 0x38, .scl_speed_hz = 100 * 1000 }

typedef struct float_sensor_aht30_config_s {
    uint16_t device_address;
    uint32_t scl_speed_hz;
} float_sensor_aht30_config_t;


typedef struct float_sensor_aht30_s float_sensor_aht30_t;
typedef float_sensor_aht30_t *float_sensor_aht30_handle_t;

struct float_sensor_aht30_s {
    float_sensor_t base;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    float_sensor_aht30_config_t config;
    uint8_t cached_data[6];
    int64_t cached_at_us;
    bool cache_valid;
};

esp_err_t float_sensor_new_aht30( i2c_master_bus_handle_t i2c_bus, float_sensor_aht30_config_t *config, float_sensor_handle_t *handle);
