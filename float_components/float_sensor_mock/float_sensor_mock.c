#include <stdlib.h>
#include "esp_err.h"
#include "esp_random.h"
#include "float_sensor_mock.h"

#include "sdkconfig.h"

#if !defined(CONFIG_FLOAT_MOCK_AIR_TEMPERATURE) && \
    !defined(CONFIG_FLOAT_MOCK_WATER_TEMPERATURE) && \
    !defined(CONFIG_FLOAT_MOCK_AIR_HUMIDITY) && \
    !defined(CONFIG_FLOAT_MOCK_AIR_PRESSURE)
#define FLOAT_MOCK_DEFAULT_ALL 1
#endif

static esp_err_t mock_initialize( float_sensor_handle_t sensor ) {
    return ESP_OK;
}

static float small_variation( float base, float range )
{
    uint32_t r = esp_random() % 1000;
    float offset = ( (float)r / 999.0f ) * 2 - 1;
    return base + offset * range;
}

static esp_err_t mock_read_air_temperature( float_sensor_handle_t sensor, float_sensor_datatype_t *out ) {
    *out = small_variation( 20.0f, 5.0f );
    return ESP_OK;
}

static esp_err_t mock_read_water_temperature( float_sensor_handle_t sensor, float_sensor_datatype_t *out ) {
    *out = small_variation( 8.0f, 3.0f );
    return ESP_OK;
}

static esp_err_t mock_read_humidity( float_sensor_handle_t sensor, float_sensor_datatype_t *out ) {
    *out = small_variation( 50.0f, 25.0f );
    return ESP_OK;
}

static esp_err_t mock_read_pressure( float_sensor_handle_t sensor, float_sensor_datatype_t *out ) {
    *out = small_variation( 1013.25f, 15.0f );
    return ESP_OK;
}

esp_err_t float_sensor_new_mock( float_sensor_handle_t *handle ) {
    float_sensor_mock_t *sensor = calloc( 1, sizeof( float_sensor_mock_t ) );
    if ( !sensor ) return ESP_ERR_NO_MEM;

    sensor->base.initialize = mock_initialize;

    uint8_t ch = 0;

#if defined(CONFIG_FLOAT_MOCK_AIR_TEMPERATURE) || defined(FLOAT_MOCK_DEFAULT_ALL)
    sensor->base.channels[ch++] = ( float_sensor_channel_t ){
        .sensor_class = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE,
        .read = mock_read_air_temperature
    };
#endif

#if defined(CONFIG_FLOAT_MOCK_WATER_TEMPERATURE)
    sensor->base.channels[ch++] = ( float_sensor_channel_t ){
        .sensor_class = FLOAT_SENSOR_CLASS_WATER_TEMPERATURE,
        .read = mock_read_water_temperature
    };
#endif

#if defined(CONFIG_FLOAT_MOCK_AIR_HUMIDITY) || defined(FLOAT_MOCK_DEFAULT_ALL)
    sensor->base.channels[ch++] = ( float_sensor_channel_t ){
        .sensor_class = FLOAT_SENSOR_CLASS_AIR_HUMIDITY,
        .read = mock_read_humidity
    };
#endif

#if defined(CONFIG_FLOAT_MOCK_AIR_PRESSURE) || defined(FLOAT_MOCK_DEFAULT_ALL)
    sensor->base.channels[ch++] = ( float_sensor_channel_t ){
        .sensor_class = FLOAT_SENSOR_CLASS_AIR_PRESSURE,
        .read = mock_read_pressure
    };
#endif

    sensor->base.num_channels = ch;

    *handle = ( float_sensor_handle_t )sensor;
    return ESP_OK;
}
