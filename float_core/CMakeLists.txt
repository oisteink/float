cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "../float_components/"
    "$ENV{HOME}/src/esp-msp3520/components/"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(float_core)
