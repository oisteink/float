#include <stdlib.h>
#include "esp_err.h"
#include "esp_check.h"
#include "led_indicator.h"
#include "led_indicator_strips.h"

#include "float_blink.h"
#include "float_blink_private.h"

static const char *TAG = "float_blink";

struct float_blink_s {
    led_indicator_handle_t led_indicator;
};

esp_err_t float_blink_new(const float_blink_config_t *config, float_blink_handle_t *out_handle) {
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "invalid args");

    float_blink_handle_t handle = calloc(1, sizeof(struct float_blink_s));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "alloc failed");

    led_strip_config_t strip_config = {
        .strip_gpio_num = config->gpio_pin,
        .led_model = LED_MODEL_WS2812,
        .max_leds = 1,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
    };

    led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = strip_config,
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = rmt_config,
    };

    const led_indicator_config_t led_config = {
        .blink_lists = blink_mode,
        .blink_list_num = BLINK_MAX,
    };

    esp_err_t ret = led_indicator_new_strips_device(&led_config, &strips_config, &handle->led_indicator);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t float_blink_delete(float_blink_handle_t handle) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    led_indicator_delete(handle->led_indicator);
    free(handle);
    return ESP_OK;
}

esp_err_t float_blink_start(float_blink_handle_t handle, float_blink_type_t blink_type) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    return led_indicator_start(handle->led_indicator, blink_type);
}

esp_err_t float_blink_stop(float_blink_handle_t handle, float_blink_type_t blink_type) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    return led_indicator_stop(handle->led_indicator, blink_type);
}
