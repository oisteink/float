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
