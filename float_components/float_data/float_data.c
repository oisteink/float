#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_check.h"
#include "string.h"

#include "float_data.h"

static const char *TAG = "float_data";

int float_datapoints_calculate_size( uint8_t num_datapoints ) {
    int size = sizeof( float_datapoints_t ) + sizeof( float_datapoint_t ) * num_datapoints;
    ESP_LOGD(TAG, "Datapoints size: %d", size);
    return size;
}

esp_err_t float_datapoints_new( float_datapoints_handle_t *datapoints_handle, uint8_t num_datapoints ) {
    ESP_RETURN_ON_FALSE(
        datapoints_handle,
        ESP_ERR_INVALID_ARG,
        TAG, "datapoints_handle is NULL"
    );

    float_datapoints_handle_t new_datapoints = (float_datapoints_handle_t) calloc(1, float_datapoints_calculate_size( num_datapoints ) );
    ESP_RETURN_ON_FALSE(
        new_datapoints,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for datapoints"
    );

    new_datapoints->num_datapoints = num_datapoints;
    *datapoints_handle = new_datapoints;

    return ESP_OK;
}
