#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "string.h"
#include "float_sensor_aht30.h"

static const char *TAG = "AHT30";

#define AHT30_CACHE_LIFETIME_US  (1000000)

esp_err_t float_sensor_aht30_read_signal( float_sensor_aht30_t *aht30, uint8_t *data) {
    int64_t now = esp_timer_get_time();
    if (aht30->cache_valid && (now - aht30->cached_at_us) < AHT30_CACHE_LIFETIME_US) {
        memcpy(data, aht30->cached_data, 6);
        return ESP_OK;
    }

    uint8_t command[] = {0xAC, 0x33, 0x00};
    uint8_t tries = 0;
    do {
        tries++;
        vTaskDelay( pdMS_TO_TICKS( 10 ) );

        ESP_RETURN_ON_ERROR(
            i2c_master_transmit( aht30->dev_handle, command, sizeof( command ), AHT30_SENSOR_TIMEOUT ),
            TAG, "Error sending command to sensor"
        );

        vTaskDelay( pdMS_TO_TICKS( 80 ) );

        ESP_RETURN_ON_ERROR(
            i2c_master_receive( aht30->dev_handle, data, 6, AHT30_SENSOR_TIMEOUT ),
            TAG, "Error receiving data from sensor"
        );

        ESP_RETURN_ON_FALSE(
            tries < 5,
            ESP_ERR_INVALID_RESPONSE,
            TAG, "Sensor busy for > 500 ms"
        );
    } while ( data[0] & 0x80 );

    memcpy(aht30->cached_data, data, 6);
    aht30->cached_at_us = esp_timer_get_time();
    aht30->cache_valid = true;

    return ESP_OK;
}

esp_err_t float_sensor_aht30_read_temperature( float_sensor_t *sensor, float_sensor_datatype_t *out_temp )
{
    float_sensor_aht30_t *aht30 = __containerof(sensor, float_sensor_aht30_t, base);

    uint8_t data[6];
    ESP_RETURN_ON_ERROR(
        float_sensor_aht30_read_signal(aht30, data),
        TAG, "Failed to get signal from sensor"
    );
    float signal = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
    *out_temp =   signal / 1048576.0f * 200.0f - 50.0f;

    return ESP_OK;
}

esp_err_t float_sensor_aht30_read_humidity( float_sensor_t *sensor, float_sensor_datatype_t *out_humidity )
{
    float_sensor_aht30_t *aht30 = __containerof(sensor, float_sensor_aht30_t, base);

    uint8_t data[6];
    ESP_RETURN_ON_ERROR(
        float_sensor_aht30_read_signal(aht30, data),
        TAG, "Failed to get signal from sensor"
    );
    float signal = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4);
    *out_humidity =  signal / 1048576.0f * 100.0f;

    return ESP_OK;
}


esp_err_t float_sensor_aht30_initialize( float_sensor_t *sensor )
{
    esp_err_t ret = ESP_OK;
    float_sensor_aht30_t *aht30 = __containerof(sensor, float_sensor_aht30_t, base);
    ESP_RETURN_ON_FALSE(
        aht30,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid sensor handle"
    );

    vTaskDelay( pdMS_TO_TICKS( 100 ) );

    return ret;
}

esp_err_t float_sensor_new_aht30( i2c_master_bus_handle_t i2c_bus, float_sensor_aht30_config_t *config, float_sensor_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        config && handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to new_aht30"
    );

    float_sensor_aht30_handle_t aht30 = NULL;
    aht30 = calloc( 1, sizeof( float_sensor_aht30_t ) );
    ESP_RETURN_ON_FALSE(
        aht30,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating memory for driver"
    );

    aht30->bus_handle = i2c_bus;
    memcpy( &aht30->config, config, sizeof( float_sensor_aht30_config_t ) );

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = aht30->config.device_address,
        .scl_speed_hz = aht30->config.scl_speed_hz,
    };
    ESP_GOTO_ON_ERROR(
        i2c_master_bus_add_device(aht30->bus_handle, &dev_config, &aht30->dev_handle),
        err,
        TAG, "Error adding i2c device to bus"
    );

    aht30->base.initialize = float_sensor_aht30_initialize;
    aht30->base.num_channels = 2;
    aht30->base.channels[0] = ( float_sensor_channel_t ){
        .sensor_class = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE,
        .read = float_sensor_aht30_read_temperature
    };
    aht30->base.channels[1] = ( float_sensor_channel_t ){
        .sensor_class = FLOAT_SENSOR_CLASS_AIR_HUMIDITY,
        .read = float_sensor_aht30_read_humidity
    };

    *handle = &(aht30->base);
    return ESP_OK;

err:
    if ( aht30 )
        free ( aht30 );
    return ret;
}
