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

TEST_CASE("sensor read_temperature via mock", "[sensor]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    float_sensor_init(sensor);

    float_sensor_datatype_t temp = 0;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_read_temperature(sensor, &temp));
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 20.0f, temp);
    free(sensor);
}

TEST_CASE("sensor read_humidity via mock", "[sensor]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    float_sensor_init(sensor);

    float_sensor_datatype_t hum = 0;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_read_humidity(sensor, &hum));
    TEST_ASSERT_FLOAT_WITHIN(25.0f, 50.0f, hum);
    free(sensor);
}

TEST_CASE("sensor read_pressure via mock", "[sensor]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    float_sensor_init(sensor);

    float_sensor_datatype_t pres = 0;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_read_pressure(sensor, &pres));
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 1013.25f, pres);
    free(sensor);
}

TEST_CASE("sensor read_data collects all readings", "[sensor]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    float_sensor_init(sensor);

    float_datapoints_handle_t dp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_read_data(sensor, &dp));
    TEST_ASSERT_NOT_NULL(dp);
    TEST_ASSERT_EQUAL(3, dp->num_datapoints);

    bool has_temp = false, has_hum = false, has_pres = false;
    for (int i = 0; i < dp->num_datapoints; i++) {
        switch (dp->datapoints[i].reading_type) {
            case FLOAT_SENSOR_TYPE_TEMPERATURE: has_temp = true; break;
            case FLOAT_SENSOR_TYPE_HUMIDITY:    has_hum  = true; break;
            case FLOAT_SENSOR_TYPE_PRESSURE:    has_pres = true; break;
        }
    }
    TEST_ASSERT_TRUE(has_temp);
    TEST_ASSERT_TRUE(has_hum);
    TEST_ASSERT_TRUE(has_pres);

    free(dp);
    free(sensor);
}

static esp_err_t temp_only_read(float_sensor_handle_t sensor, float_sensor_datatype_t *out) {
    *out = 22.5f;
    return ESP_OK;
}

TEST_CASE("sensor read_data with partial capabilities", "[sensor]")
{
    float_sensor_t partial = {
        .initialize = NULL,
        .read_temperature = temp_only_read,
        .read_humidity = NULL,
        .read_pressure = NULL,
    };

    float_datapoints_handle_t dp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_read_data(&partial, &dp));
    TEST_ASSERT_NOT_NULL(dp);
    TEST_ASSERT_EQUAL(1, dp->num_datapoints);
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_TYPE_TEMPERATURE, dp->datapoints[0].reading_type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.5f, dp->datapoints[0].value);

    free(dp);
}

TEST_CASE("sensor functions reject NULL", "[sensor]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_init(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_read_temperature(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_read_humidity(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_read_pressure(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_read_data(NULL, NULL));

    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_read_temperature(sensor, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_sensor_read_data(sensor, NULL));
    free(sensor);
}
