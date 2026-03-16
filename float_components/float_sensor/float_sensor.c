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

    ESP_RETURN_ON_FALSE(
        sensor->num_channels > 0,
        ESP_ERR_INVALID_STATE,
        TAG, "Sensor has no channels"
    );

    float_datapoints_t *data = NULL;
    float_datapoints_new( &data, sensor->num_channels );
    ESP_RETURN_ON_FALSE(
        data,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for datapoints"
    );

    uint8_t current_datapoint = 0;
    for ( uint8_t i = 0; i < sensor->num_channels; i++ ) {
        float_sensor_channel_t *ch = &sensor->channels[i];
        if ( !ch->read )
            continue;

        float_sensor_datatype_t value;
        ret = ch->read( sensor, &value );
        if ( ret == ESP_OK ) {
            data->datapoints[current_datapoint].reading_type = ch->sensor_class;
            data->datapoints[current_datapoint].value = value;
            current_datapoint++;
        }
    }

    data->num_datapoints = current_datapoint;
    *datapoints = data;
    return ret;
}

esp_err_t float_sensor_get_classes( float_sensor_handle_t sensor, float_sensor_class_t *out_classes, uint8_t *out_count ) {
    ESP_RETURN_ON_FALSE(
        sensor && out_classes && out_count,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to get_classes"
    );

    uint8_t count = 0;
    for ( uint8_t i = 0; i < sensor->num_channels; i++ ) {
        out_classes[count++] = sensor->channels[i].sensor_class;
    }
    *out_count = count;
    return ESP_OK;
}
