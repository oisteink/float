#include <stdlib.h>
#include "esp_err.h"
#include "float_sensor_mock.h"

static esp_err_t mock_initialize(float_sensor_handle_t sensor) {
    return ESP_OK;
}

static esp_err_t mock_read_temperature(float_sensor_handle_t sensor, float_sensor_datatype_t *out) {
    *out = 20.0f;
    return ESP_OK;
}

static esp_err_t mock_read_humidity(float_sensor_handle_t sensor, float_sensor_datatype_t *out) {
    *out = 50.0f;
    return ESP_OK;
}

static esp_err_t mock_read_pressure(float_sensor_handle_t sensor, float_sensor_datatype_t *out) {
    *out = 101325.0f;
    return ESP_OK;
}

esp_err_t float_sensor_new_mock(float_sensor_handle_t *handle) {
    float_sensor_mock_t *sensor = calloc(1, sizeof(float_sensor_mock_t));
    if (!sensor) return ESP_ERR_NO_MEM;
    sensor->base.initialize        = mock_initialize;
    sensor->base.read_temperature  = mock_read_temperature;
    sensor->base.read_humidity     = mock_read_humidity;
    sensor->base.read_pressure     = mock_read_pressure;
    *handle = (float_sensor_handle_t)sensor;
    return ESP_OK;
}
