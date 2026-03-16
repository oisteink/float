#include "float_sensor_class.h"
#include <stddef.h>

static const float_sensor_class_info_t s_class_table[] = {
    { FLOAT_SENSOR_CLASS_AIR_TEMPERATURE,   "Air Temp",   "\xC2\xB0""C", "%.1f", -40.0f, 60.0f   },
    { FLOAT_SENSOR_CLASS_WATER_TEMPERATURE, "Water Temp", "\xC2\xB0""C", "%.1f", -2.0f,  40.0f   },
    { FLOAT_SENSOR_CLASS_AIR_HUMIDITY,      "Humidity",   "%",            "%.0f", 0.0f,   100.0f  },
    { FLOAT_SENSOR_CLASS_AIR_PRESSURE,      "Pressure",   "hPa",         "%.0f", 800.0f, 1200.0f },
};

const float_sensor_class_info_t *float_sensor_class_get_info( float_sensor_class_t sensor_class )
{
    if ( sensor_class >= FLOAT_SENSOR_CLASS_MAX )
        return NULL;

    for ( size_t i = 0; i < sizeof( s_class_table ) / sizeof( s_class_table[0] ); i++ ) {
        if ( s_class_table[i].sensor_class == sensor_class )
            return &s_class_table[i];
    }

    return NULL;
}
