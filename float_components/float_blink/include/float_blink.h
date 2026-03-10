#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

typedef enum {
    BLINK_DATA_SEND = 0,
    BLINK_DATA_RECEIVE,
    BLINK_DATA_UNKNOWN_PEER,
    BLINK_PAIRING,
    BLINK_PAIRING_COMPLETE,
    BLINK_MAX
} float_blink_type_t;

typedef struct float_blink_config_s {
    gpio_num_t gpio_pin;
} float_blink_config_t;

typedef struct float_blink_s *float_blink_handle_t;

esp_err_t float_blink_new(const float_blink_config_t *config, float_blink_handle_t *out_handle);
esp_err_t float_blink_delete(float_blink_handle_t handle);
esp_err_t float_blink_start(float_blink_handle_t handle, float_blink_type_t blink_type);
esp_err_t float_blink_stop(float_blink_handle_t handle, float_blink_type_t blink_type);
