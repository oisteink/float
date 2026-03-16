cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "../float_components/"
)

set(TEST_COMPONENTS float_sensor_class float_registry float_data float_sensor_mock float_now float_sensor)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(float_test)
