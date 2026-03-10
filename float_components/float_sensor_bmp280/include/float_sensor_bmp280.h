#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "float_sensor.h"

// Registers
// Compensation parameters are stored in two consecutive registers, LSB -> MSB: Table 17
#define BMP280_REGISTER_DIG_T1              0x88
#define BMP280_REGISTER_DIG_T2              0x8A
#define BMP280_REGISTER_DIG_T3              0x8C

#define BMP280_REGISTER_DIG_P1              0x8E
#define BMP280_REGISTER_DIG_P2              0x90
#define BMP280_REGISTER_DIG_P3              0x92
#define BMP280_REGISTER_DIG_P4              0x94
#define BMP280_REGISTER_DIG_P5              0x96
#define BMP280_REGISTER_DIG_P6              0x98
#define BMP280_REGISTER_DIG_P7              0x9A
#define BMP280_REGISTER_DIG_P8              0x9C
#define BMP280_REGISTER_DIG_P9              0x9E

// Table 18
#define BMP280_REGISTER_CHIPID              0xD0
#define BMP280_REGISTER_VERSION             0xD1
#define BMP280_REGISTER_SOFTRESET           0xE0
#define BMP280_REGISTER_STATUS              0xF3

#define BMP280_REGISTER_CONTROL             0xF4
#define BMP280_REGISTER_CONFIG              0xF5

// Pressure and temperature data is stored in top 20 bits of 3 consecutive registers, MSB, LSB, XLSB
#define BMP280_REGISTER_PRESSUREDATA        0xF7
#define BMP280_REGISTER_TEMPDATA            0xFA

// 3.3.1 Temperature: Table 21
// 3.3.2 Pressure : Table 22
typedef enum bmp280_oversampling_e {
    BMP280_MEASUREMENT_SKIP  = 0b000,
    BMP280_OVERSAMPLING_X1   = 0b001,
    BMP280_OVERSAMPLING_X2   = 0b010,
    BMP280_OVERSAMPLING_X4   = 0b011,
    BMP280_OVERSAMPLING_X8   = 0b100,
    BMP280_OVERSAMPLING_X16  = 0b101,
} bmp280_oversampling_t;

// 3.6 Power Modes: Table 10
typedef enum bmp280_power_mode_e {
    BMP280_MODE_SLEEP  = 0b00,
    BMP280_MODE_FORCED = 0b01,
    BMP280_MODE_NORMAL = 0b11,
} bmp280_power_mode_t;

// 3.6.3 Inactive duration in normal mode: Table 11
typedef enum bmp280_standby_e {
    BMP280_STANDBY_MS_0_5  = 0b000,
    BMP280_STANDBY_MS_10   = 0b110,
    BMP280_STANDBY_MS_20   = 0b111,
    BMP280_STANDBY_MS_62_5 = 0b001,
    BMP280_STANDBY_MS_125  = 0b010,
    BMP280_STANDBY_MS_250  = 0b011,
    BMP280_STANDBY_MS_500  = 0b100,
    BMP280_STANDBY_MS_1000 = 0b101,
} bmp280_standby_t;

// 3.3.3 IIR filter: Table 6
typedef enum bmp280_iir_filters_e {
    BMP280_FILTER_OFF = 0b000,
    BMP280_FILTER_X2  = 0b001,
    BMP280_FILTER_X4  = 0b010,
    BMP280_FILTER_X8  = 0b011,
    BMP280_FILTER_X16 = 0b100,
} bmp280_IIR_filter_t;

// spi3w_en: Table 18
typedef enum bmp280_spi3w_en_e {
    BMP280_SPI3W_DISABLED = 0b0,
    BMP280_SPI3W_ENABLED  = 0x1,
} bmp280_spi32_en_t;

typedef struct bmp280_config_s {
    unsigned int t_sb : 3;
    unsigned int filter : 3;
    unsigned int unused : 1;
    unsigned int spi3w_en : 1;
} bmp280_config_t;

typedef struct ctrl_meas_s {
    unsigned int osrs_t : 3;
    unsigned int osrs_p : 3;
    unsigned int mode : 2;
} bmp280_ctrl_meas_t;

typedef struct status_s {
    unsigned int unused : 4;
    unsigned int measuring : 3;
    unsigned int unused2 : 1;
    unsigned int im_update : 1;
} status_t;

typedef struct float_sensor_bmp280_config_s {
    uint16_t device_address;
    uint32_t scl_speed_hz;
    bmp280_config_t config;
    bmp280_ctrl_meas_t control_measure;
} float_sensor_bmp280_config_t;

// Default setup is for Weather monitoring according to table 7
#define DEFAULT_FLOAT_SENSOR_BMP280_CONFIG {            \
    .device_address = 0x76,                             \
    .scl_speed_hz = 100 * 1000,                         \
    .config.filter =  BMP280_FILTER_OFF,                \
    .control_measure.osrs_t = BMP280_OVERSAMPLING_X1,   \
    .control_measure.osrs_p = BMP280_OVERSAMPLING_X1,   \
    .control_measure.mode = BMP280_MODE_FORCED,         \
}

typedef struct bmp280_calib_data_s {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
} bmp280_calib_data_t;

typedef struct float_sensor_bmp280_s float_sensor_bmp280_t;
typedef float_sensor_bmp280_t *float_sensor_bmp280_handle_t;

struct float_sensor_bmp280_s {
    float_sensor_t base;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    bmp280_config_t bmp280_config;
    bmp280_ctrl_meas_t bmp280_ctrl_meas;
    bmp280_calib_data_t calib;
    int32_t t_fine;
    uint8_t cached_data[6];
    int64_t cached_at_us;
    bool cache_valid;
    float_sensor_bmp280_config_t config;
};

esp_err_t float_sensor_new_bmp280( i2c_master_bus_handle_t i2c_bus, float_sensor_bmp280_config_t *config, float_sensor_handle_t *handle);
