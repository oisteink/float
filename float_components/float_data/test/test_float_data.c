#include "unity.h"
#include "float_data.h"

TEST_CASE("datapoints_calculate_size returns correct size", "[data]")
{
    int size = float_datapoints_calculate_size(3);
    int expected = sizeof(float_datapoints_t) + 3 * sizeof(float_datapoint_t);
    TEST_ASSERT_EQUAL(expected, size);
}

TEST_CASE("datapoints_new allocates correctly", "[data]")
{
    float_datapoints_handle_t dp = NULL;
    esp_err_t ret = float_datapoints_new(&dp, 2);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(dp);
    TEST_ASSERT_EQUAL(2, dp->num_datapoints);
    free(dp);
}

TEST_CASE("datapoints_new rejects NULL handle", "[data]")
{
    esp_err_t ret = float_datapoints_new(NULL, 1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("datapoints_new with zero count", "[data]")
{
    float_datapoints_handle_t dp = NULL;
    esp_err_t ret = float_datapoints_new(&dp, 0);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(dp);
    TEST_ASSERT_EQUAL(0, dp->num_datapoints);
    free(dp);
}

TEST_CASE("datapoints fields accessible after alloc", "[data]")
{
    float_datapoints_handle_t dp = NULL;
    float_datapoints_new(&dp, 2);

    dp->datapoints[0].reading_type = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE;
    dp->datapoints[0].value = 23.5f;
    dp->datapoints[1].reading_type = FLOAT_SENSOR_CLASS_AIR_HUMIDITY;
    dp->datapoints[1].value = 65.0f;

    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, dp->datapoints[0].reading_type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.5f, dp->datapoints[0].value);
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_AIR_HUMIDITY, dp->datapoints[1].reading_type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, dp->datapoints[1].value);

    free(dp);
}
