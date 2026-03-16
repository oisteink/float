#include <stdlib.h>
#include "unity.h"
#include "float_sensor.h"
#include "float_sensor_mock.h"
#include "float_data.h"

TEST_CASE("mock factory creates valid handle", "[sensor_mock]")
{
    float_sensor_handle_t sensor = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_new_mock(&sensor));
    TEST_ASSERT_NOT_NULL(sensor);
    TEST_ASSERT_NOT_NULL(sensor->initialize);
    TEST_ASSERT_TRUE(sensor->num_channels > 0);
    free(sensor);
}

TEST_CASE("mock readings via read_data in expected ranges", "[sensor_mock]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    float_sensor_init(sensor);

    float_datapoints_handle_t dp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_read_data(sensor, &dp));
    TEST_ASSERT_NOT_NULL(dp);

    for (int i = 0; i < dp->num_datapoints; i++) {
        float val = dp->datapoints[i].value;
        switch (dp->datapoints[i].reading_type) {
            case FLOAT_SENSOR_CLASS_AIR_TEMPERATURE:
                TEST_ASSERT_FLOAT_WITHIN(5.0f, 20.0f, val);
                break;
            case FLOAT_SENSOR_CLASS_AIR_HUMIDITY:
                TEST_ASSERT_FLOAT_WITHIN(25.0f, 50.0f, val);
                break;
            case FLOAT_SENSOR_CLASS_AIR_PRESSURE:
                TEST_ASSERT_FLOAT_WITHIN(15.0f, 1013.25f, val);
                break;
            default:
                break;
        }
    }

    free(dp);
    free(sensor);
}

TEST_CASE("mock init returns ESP_OK", "[sensor_mock]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_init(sensor));
    free(sensor);
}
