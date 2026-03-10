#include <stdlib.h>
#include "esp_err.h"
#include "esp_random.h"
#include "float_sensor_mock.h"

static esp_err_t mock_initialize(float_sensor_handle_t sensor) {
    return ESP_OK;
}

static float small_variation(float base, float range)
{
    // range = max deviation (+/-)
    uint32_t r = esp_random() % 1000;           // 0..999
    float offset = ((float)r / 999.0f) * 2 - 1; // -1..1
    return base + offset * range;
}

static esp_err_t mock_read_temperature(float_sensor_handle_t sensor, float_sensor_datatype_t *out) {
    *out = small_variation(20.0f, 5.0f);
    return ESP_OK;
}

static esp_err_t mock_read_humidity(float_sensor_handle_t sensor, float_sensor_datatype_t *out) {
    *out = small_variation(50.0f, 25.0f);
    return ESP_OK;
}

static esp_err_t mock_read_pressure(float_sensor_handle_t sensor, float_sensor_datatype_t *out) {
    *out = small_variation(1013.25f, 15.0f);
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
