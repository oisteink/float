#include <stdlib.h>
#include "unity.h"
#include "float_sensor.h"
#include "float_sensor_mock.h"
#include "float_data.h"

TEST_CASE("sensor init with mock succeeds", "[sensor]")
{
    float_sensor_handle_t sensor = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_new_mock(&sensor));
    TEST_ASSERT_NOT_NULL(sensor);

    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_init(sensor));
    free(sensor);
}

TEST_CASE("sensor read_data collects all channel readings", "[sensor]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    float_sensor_init(sensor);

    float_datapoints_handle_t dp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_read_data(sensor, &dp));
    TEST_ASSERT_NOT_NULL(dp);
    TEST_ASSERT_EQUAL(3, dp->num_datapoints);

    bool has_air_temp = false, has_hum = false, has_pres = false;
    for (int i = 0; i < dp->num_datapoints; i++) {
        switch (dp->datapoints[i].reading_type) {
            case FLOAT_SENSOR_CLASS_AIR_TEMPERATURE: has_air_temp = true; break;
            case FLOAT_SENSOR_CLASS_AIR_HUMIDITY:    has_hum      = true; break;
            case FLOAT_SENSOR_CLASS_AIR_PRESSURE:    has_pres     = true; break;
        }
    }
    TEST_ASSERT_TRUE(has_air_temp);
    TEST_ASSERT_TRUE(has_hum);
    TEST_ASSERT_TRUE(has_pres);

    free(dp);
    free(sensor);
}

static esp_err_t temp_only_read(float_sensor_handle_t sensor, float_sensor_datatype_t *out) {
    *out = 22.5f;
    return ESP_OK;
}

TEST_CASE("sensor read_data with single channel", "[sensor]")
{
    float_sensor_t partial = { 0 };
    partial.num_channels = 1;
    partial.channels[0] = (float_sensor_channel_t){
        .sensor_class = FLOAT_SENSOR_CLASS_WATER_TEMPERATURE,
        .read = temp_only_read,
    };

    float_datapoints_handle_t dp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_read_data(&partial, &dp));
    TEST_ASSERT_NOT_NULL(dp);
    TEST_ASSERT_EQUAL(1, dp->num_datapoints);
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_WATER_TEMPERATURE, dp->datapoints[0].reading_type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.5f, dp->datapoints[0].value);

    free(dp);
}

TEST_CASE("sensor get_classes returns channel classes", "[sensor]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);

    float_sensor_class_t classes[FLOAT_SENSOR_MAX_CHANNELS];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_get_classes(sensor, classes, &count));
    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, classes[0]);
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_AIR_HUMIDITY, classes[1]);
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_AIR_PRESSURE, classes[2]);

    free(sensor);
}

TEST_CASE("sensor functions reject NULL", "[sensor]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_init(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_read_data(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_get_classes(NULL, NULL, NULL));

    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_read_data(sensor, NULL));
    free(sensor);
}
