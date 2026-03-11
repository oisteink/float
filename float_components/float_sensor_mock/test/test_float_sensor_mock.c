#include <stdlib.h>
#include "unity.h"
#include "float_sensor.h"
#include "float_sensor_mock.h"

TEST_CASE("mock factory creates valid handle", "[sensor_mock]")
{
    float_sensor_handle_t sensor = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_new_mock(&sensor));
    TEST_ASSERT_NOT_NULL(sensor);
    TEST_ASSERT_NOT_NULL(sensor->initialize);
    TEST_ASSERT_NOT_NULL(sensor->read_temperature);
    TEST_ASSERT_NOT_NULL(sensor->read_humidity);
    TEST_ASSERT_NOT_NULL(sensor->read_pressure);
    free(sensor);
}

TEST_CASE("mock readings in expected ranges", "[sensor_mock]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    float_sensor_init(sensor);

    for (int i = 0; i < 100; i++) {
        float_sensor_datatype_t val;

        float_sensor_read_temperature(sensor, &val);
        TEST_ASSERT_FLOAT_WITHIN(5.0f, 20.0f, val);

        float_sensor_read_humidity(sensor, &val);
        TEST_ASSERT_FLOAT_WITHIN(25.0f, 50.0f, val);

        float_sensor_read_pressure(sensor, &val);
        TEST_ASSERT_FLOAT_WITHIN(15.0f, 1013.25f, val);
    }

    free(sensor);
}

TEST_CASE("mock init returns ESP_OK", "[sensor_mock]")
{
    float_sensor_handle_t sensor = NULL;
    float_sensor_new_mock(&sensor);
    TEST_ASSERT_EQUAL(ESP_OK, float_sensor_init(sensor));
    free(sensor);
}
