#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include "esp_err.h"
#include "esp_check.h"
#include "string.h"
#include "float_sensor.h"

static const char *TAG = "float_sensor";

esp_err_t float_sensor_init( float_sensor_handle_t sensor ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to init"
    );
    if ( sensor->initialize )
        ret = sensor->initialize( sensor );
    return ret;
}

esp_err_t float_sensor_read_data( float_sensor_handle_t sensor, float_datapoints_t **datapoints ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && datapoints,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to float_sensor_read_data"
    );

    uint8_t count = 0;
    if ( sensor->read_humidity ) count++;
    if ( sensor->read_pressure ) count++;
    if ( sensor->read_temperature ) count++;

    float_datapoints_t *data = NULL;
    float_datapoints_new( &data, count );
    ESP_RETURN_ON_FALSE(
        data,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for datapoints"
    );

    uint8_t current_datapoint = 0;
    if ( sensor->read_humidity )
    {
        data->datapoints[current_datapoint].reading_type = FLOAT_SENSOR_TYPE_HUMIDITY;
        float_sensor_datatype_t humidity;
        ret = sensor->read_humidity( sensor, &humidity );
        if ( ret == ESP_OK ) {
            data->datapoints[current_datapoint].value = humidity;
            current_datapoint++;
        }
    }

    if ( sensor->read_pressure )
    {
        data->datapoints[current_datapoint].reading_type = FLOAT_SENSOR_TYPE_PRESSURE;
        float_sensor_datatype_t pressure;
        ret = sensor->read_pressure( sensor, &pressure );
        if ( ret == ESP_OK ) {
            data->datapoints[current_datapoint].value = pressure;
            current_datapoint++;
        }
    }

    if ( sensor->read_temperature )
    {
        data->datapoints[current_datapoint].reading_type = FLOAT_SENSOR_TYPE_TEMPERATURE;
        float_sensor_datatype_t temperature;
        ret = sensor->read_temperature( sensor, &temperature );
        if ( ret == ESP_OK ) {
            data->datapoints[current_datapoint].value = temperature;
            current_datapoint++;
        }
    }

    *datapoints = data;
    return ret;
}

esp_err_t float_sensor_read_temperature( float_sensor_handle_t sensor, float_sensor_datatype_t *out_temp ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && out_temp,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to read_temperature"
    );
    if ( sensor->read_temperature )
        ret = sensor->read_temperature( sensor, out_temp );
    return ret;
}

esp_err_t float_sensor_read_humidity( float_sensor_handle_t sensor, float_sensor_datatype_t *out_humidity ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && out_humidity,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to read humidity"
    );

    if ( sensor->read_humidity )
        ret = sensor->read_humidity (sensor, out_humidity );
    return ret;
}

esp_err_t float_sensor_read_pressure( float_sensor_handle_t sensor, float_sensor_datatype_t *out_pressure ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && out_pressure,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to pressure"
    );
    if ( sensor->read_pressure )
        ret = sensor->read_pressure( sensor, out_pressure );
    return ret;
}
