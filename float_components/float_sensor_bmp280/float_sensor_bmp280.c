#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "string.h"

#include "float_sensor_bmp280.h"

static const char *TAG = "float_sensor_bmp280";

#define BMP280_CACHE_LIFETIME_US (1000000)

static esp_err_t _read_register( float_sensor_bmp280_t *bmp280, uint8_t reg, uint8_t *data, size_t datasize ) {
    ESP_RETURN_ON_FALSE(
        bmp280,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL passed to _read_register"
    );

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive( bmp280->dev_handle, &reg, 1, data, datasize, portMAX_DELAY ),
        TAG, "Error reading BMP280 register 0x%x", reg
    );
    return ESP_OK;
}

static esp_err_t _write_register( float_sensor_bmp280_t *bmp280, uint8_t reg, uint8_t value ) {
    ESP_RETURN_ON_FALSE(
        bmp280,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL passed to _write_register"
    );

    uint8_t buf[2] = { reg, value };
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit( bmp280->dev_handle, buf, sizeof( buf ), portMAX_DELAY ),
        TAG, "Error writing BMP280 register 0x%x", reg
    );
    return ESP_OK;
}

static void _unpack_calibration_data( float_sensor_bmp280_t *bmp280, const uint8_t *raw ) {
    bmp280->calib.dig_T1 = (uint16_t)( raw[1] << 8 | raw[0] );
    bmp280->calib.dig_T2 = (int16_t)( raw[3] << 8 | raw[2] );
    bmp280->calib.dig_T3 = (int16_t)( raw[5] << 8 | raw[4] );
    bmp280->calib.dig_P1 = (uint16_t)( raw[7] << 8 | raw[6] );
    bmp280->calib.dig_P2 = (int16_t)( raw[9] << 8 | raw[8] );
    bmp280->calib.dig_P3 = (int16_t)( raw[11] << 8 | raw[10] );
    bmp280->calib.dig_P4 = (int16_t)( raw[13] << 8 | raw[12] );
    bmp280->calib.dig_P5 = (int16_t)( raw[15] << 8 | raw[14] );
    bmp280->calib.dig_P6 = (int16_t)( raw[17] << 8 | raw[16] );
    bmp280->calib.dig_P7 = (int16_t)( raw[19] << 8 | raw[18] );
    bmp280->calib.dig_P8 = (int16_t)( raw[21] << 8 | raw[20] );
    bmp280->calib.dig_P9 = (int16_t)( raw[23] << 8 | raw[22] );
}

static esp_err_t _read_calibration_data( float_sensor_bmp280_t *bmp280 ) {
    uint8_t raw[24];
    ESP_RETURN_ON_ERROR(
        _read_register( bmp280, BMP280_REGISTER_DIG_T1, raw, sizeof( raw ) ),
        TAG, "Error reading calibration data"
    );
    _unpack_calibration_data( bmp280, raw );
    return ESP_OK;
}

static esp_err_t _read_raw_data( float_sensor_bmp280_t *bmp280, uint8_t *data ) {
    int64_t now = esp_timer_get_time();
    if ( bmp280->cache_valid && ( now - bmp280->cached_at_us ) < BMP280_CACHE_LIFETIME_US ) {
        memcpy( data, bmp280->cached_data, 6 );
        return ESP_OK;
    }

    uint8_t ctrl_meas;
    memcpy( &ctrl_meas, &bmp280->bmp280_ctrl_meas, sizeof( uint8_t ) );
    ESP_RETURN_ON_ERROR(
        _write_register( bmp280, BMP280_REGISTER_CONTROL, ctrl_meas ),
        TAG, "Error triggering forced-mode measurement"
    );

    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    ESP_RETURN_ON_ERROR(
        _read_register( bmp280, BMP280_REGISTER_PRESSUREDATA, data, 6 ),
        TAG, "Error reading raw measurement data"
    );

    memcpy( bmp280->cached_data, data, 6 );
    bmp280->cached_at_us = esp_timer_get_time();
    bmp280->cache_valid = true;
    return ESP_OK;
}

static int32_t _compensate_temperature( float_sensor_bmp280_t *bmp280, const uint8_t *data ) {
    int32_t adc_T = ( (int32_t)data[3] << 12 ) | ( (int32_t)data[4] << 4 ) | ( data[5] >> 4 );

    int32_t var1 = ( ( ( ( adc_T >> 3 ) - ( (int32_t)bmp280->calib.dig_T1 << 1 ) ) ) * ( (int32_t)bmp280->calib.dig_T2 ) ) >> 11;
    int32_t var2 = ( ( ( ( ( adc_T >> 4 ) - ( (int32_t)bmp280->calib.dig_T1 ) ) * ( ( adc_T >> 4 ) - ( (int32_t)bmp280->calib.dig_T1 ) ) ) >> 12 ) * ( (int32_t)bmp280->calib.dig_T3 ) ) >> 14;

    bmp280->t_fine = var1 + var2;
    return ( bmp280->t_fine * 5 + 128 ) >> 8;
}

static uint32_t _compensate_pressure( float_sensor_bmp280_t *bmp280, const uint8_t *data ) {
    int32_t adc_P = ( (int32_t)data[0] << 12 ) | ( (int32_t)data[1] << 4 ) | ( data[2] >> 4 );

    int64_t var1 = (int64_t)bmp280->t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)bmp280->calib.dig_P6;
    var2 = var2 + ( ( var1 * (int64_t)bmp280->calib.dig_P5 ) << 17 );
    var2 = var2 + ( (int64_t)bmp280->calib.dig_P4 << 35 );
    var1 = ( ( var1 * var1 * (int64_t)bmp280->calib.dig_P3 ) >> 8 ) + ( ( var1 * (int64_t)bmp280->calib.dig_P2 ) << 12 );
    var1 = ( ( ( (int64_t)1 << 47 ) + var1 ) ) * (int64_t)bmp280->calib.dig_P1 >> 33;

    if ( var1 == 0 ) {
        return 0;
    }

    int64_t p = 1048576 - adc_P;
    p = ( ( ( p << 31 ) - var2 ) * 3125 ) / var1;
    var1 = ( (int64_t)bmp280->calib.dig_P9 * ( p >> 13 ) * ( p >> 13 ) ) >> 25;
    var2 = ( (int64_t)bmp280->calib.dig_P8 * p ) >> 19;
    p = ( ( p + var1 + var2 ) >> 8 ) + ( (int64_t)bmp280->calib.dig_P7 << 4 );

    return (uint32_t)p;
}

esp_err_t float_sensor_bmp280_read_temperature( float_sensor_t *sensor, float_sensor_datatype_t *out_temp ) {
    float_sensor_bmp280_t *bmp280 = __containerof( sensor, float_sensor_bmp280_t, base );

    uint8_t data[6];
    ESP_RETURN_ON_ERROR(
        _read_raw_data( bmp280, data ),
        TAG, "Failed to read raw data for temperature"
    );

    int32_t temp_raw = _compensate_temperature( bmp280, data );
    *out_temp = temp_raw / 100.0f;
    return ESP_OK;
}

esp_err_t float_sensor_bmp280_read_pressure( float_sensor_t *sensor, float_sensor_datatype_t *out_pressure ) {
    float_sensor_bmp280_t *bmp280 = __containerof( sensor, float_sensor_bmp280_t, base );

    uint8_t data[6];
    ESP_RETURN_ON_ERROR(
        _read_raw_data( bmp280, data ),
        TAG, "Failed to read raw data for pressure"
    );

    _compensate_temperature( bmp280, data );
    uint32_t press_raw = _compensate_pressure( bmp280, data );
    *out_pressure = press_raw / 25600.0f;
    return ESP_OK;
}

esp_err_t float_sensor_bmp280_initialize( float_sensor_t *sensor ) {
    esp_err_t ret = ESP_OK;

    float_sensor_bmp280_t *bmp280 = __containerof( sensor, float_sensor_bmp280_t, base );
    ESP_RETURN_ON_FALSE(
        bmp280,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid sensor handle"
    );

    ESP_RETURN_ON_ERROR(
        _read_calibration_data( bmp280 ),
        TAG, "Error initializing sensor"
    );

    ESP_LOGI( TAG, "bmp280 Sensor initialized" );
    return ret;
}

esp_err_t float_sensor_new_bmp280( i2c_master_bus_handle_t i2c_bus, float_sensor_bmp280_config_t *config, float_sensor_handle_t *handle ) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        config && handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to new_bmp280"
    );

    float_sensor_bmp280_handle_t bmp280 = calloc( 1, sizeof( float_sensor_bmp280_t ) );
    ESP_RETURN_ON_FALSE(
        bmp280,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating memory for sensor"
    );

    bmp280->bus_handle = i2c_bus;
    memcpy( &bmp280->config, config, sizeof( float_sensor_bmp280_config_t ) );
    bmp280->bmp280_ctrl_meas = bmp280->config.control_measure;
    bmp280->bmp280_config = bmp280->config.config;

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->device_address,
        .scl_speed_hz = config->scl_speed_hz,
    };
    ESP_GOTO_ON_ERROR(
        i2c_master_bus_add_device( bmp280->bus_handle, &device_config, &bmp280->dev_handle ),
        err,
        TAG, "Error adding i2c device to bus"
    );

    bmp280->base.initialize = float_sensor_bmp280_initialize;
    bmp280->base.num_channels = 2;
    bmp280->base.channels[0] = ( float_sensor_channel_t ){
        .sensor_class = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE,
        .read = float_sensor_bmp280_read_temperature
    };
    bmp280->base.channels[1] = ( float_sensor_channel_t ){
        .sensor_class = FLOAT_SENSOR_CLASS_AIR_PRESSURE,
        .read = float_sensor_bmp280_read_pressure
    };

    *handle = &( bmp280->base );
    return ESP_OK;
err:
    if ( bmp280 )
        free( bmp280 );

    return ret;
}
