#include "unity.h"
#include "float_sensor_class.h"

TEST_CASE("sensor class get_info returns correct data for each class", "[sensor_class]")
{
    const float_sensor_class_info_t *info;

    info = float_sensor_class_get_info( FLOAT_SENSOR_CLASS_AIR_TEMPERATURE );
    TEST_ASSERT_NOT_NULL( info );
    TEST_ASSERT_EQUAL( FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, info->sensor_class );
    TEST_ASSERT_EQUAL_STRING( "Air Temp", info->label );

    info = float_sensor_class_get_info( FLOAT_SENSOR_CLASS_WATER_TEMPERATURE );
    TEST_ASSERT_NOT_NULL( info );
    TEST_ASSERT_EQUAL( FLOAT_SENSOR_CLASS_WATER_TEMPERATURE, info->sensor_class );
    TEST_ASSERT_EQUAL_STRING( "Water Temp", info->label );

    info = float_sensor_class_get_info( FLOAT_SENSOR_CLASS_AIR_HUMIDITY );
    TEST_ASSERT_NOT_NULL( info );
    TEST_ASSERT_EQUAL_STRING( "Humidity", info->label );

    info = float_sensor_class_get_info( FLOAT_SENSOR_CLASS_AIR_PRESSURE );
    TEST_ASSERT_NOT_NULL( info );
    TEST_ASSERT_EQUAL_STRING( "Pressure", info->label );
}

TEST_CASE("sensor class get_info returns NULL for invalid class", "[sensor_class]")
{
    TEST_ASSERT_NULL( float_sensor_class_get_info( FLOAT_SENSOR_CLASS_MAX ) );
    TEST_ASSERT_NULL( float_sensor_class_get_info( 255 ) );
}
