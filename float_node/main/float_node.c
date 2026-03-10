#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_sleep.h"

#include "float_now.h"
#include "float_blink.h"

#include "float_node.h"
#include "float_data.h"

#if CONFIG_FLOAT_NODE_SENSOR_BMP280
#include "float_sensor_bmp280.h"
#elif CONFIG_FLOAT_NODE_SENSOR_AHT30
#include "float_sensor_aht30.h"
#elif CONFIG_FLOAT_NODE_SENSOR_MOCK
#include "float_sensor_mock.h"
#include "driver/i2c_types.h"
#endif
#include "float_sensor.h"

static const char *TAG = "float-node";
static float_now_handle_t now_handle = NULL;
static float_blink_handle_t blink_handle = NULL;
RTC_DATA_ATTR static uint8_t paired_core[ ESP_NOW_ETH_ALEN ] = { 0 };
RTC_DATA_ATTR uint8_t failed_sends = 0;


bool saved_peer( void ){
    uint8_t empty_mac[ ESP_NOW_ETH_ALEN ] = { 0 };
    return ( memcmp( paired_core, empty_mac, ESP_NOW_ETH_ALEN ) != 0 );
}

void pair_with_core( void ){
    uint8_t broadcast[ ESP_NOW_ETH_ALEN ] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    ESP_ERROR_CHECK(
        float_now_add_peer( now_handle, broadcast )
    );

    float_now_packet_t data_packet = {
        .header.type = FLOAT_PACKET_PAIRING,
        .header.version = FLOAT_NOW_VERSION,
    };

    uint8_t peering_tries = 0;
    do {
        if (peering_tries++ >= 5)
            esp_deep_sleep(NODE_SLEEP_NO_PEER);

        ESP_ERROR_CHECK(
            float_blink_start( blink_handle, BLINK_PAIRING )
        );

        ESP_ERROR_CHECK(
            float_now_send_packet( now_handle, broadcast, &data_packet )
        );
    } while ( float_now_wait_for_ack( now_handle, FLOAT_PACKET_PAIRING, 5000 ) != ESP_OK );

    ESP_ERROR_CHECK(
        float_now_remove_peer( now_handle, broadcast )
    );

    while ( ! saved_peer() )
        vTaskDelay( pdMS_TO_TICKS( 50 ) );
}

#if !CONFIG_FLOAT_NODE_SENSOR_MOCK
esp_err_t i2c_init(i2c_master_bus_handle_t *i2c_bus){
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(
        i2c_new_master_bus( &bus_config, i2c_bus )
    );

    return ESP_OK;
}
#endif

esp_err_t init_sensor(float_sensor_handle_t *sensor, i2c_master_bus_handle_t i2c_bus) {
#if CONFIG_FLOAT_NODE_SENSOR_BMP280
    float_sensor_bmp280_config_t config = DEFAULT_FLOAT_SENSOR_BMP280_CONFIG;
    ESP_RETURN_ON_ERROR(
        float_sensor_new_bmp280(i2c_bus, &config, sensor),
        TAG, "Error creating sensor"
    );
#elif CONFIG_FLOAT_NODE_SENSOR_AHT30
    float_sensor_aht30_config_t config = DEFAULT_FLOAT_SENSOR_AHT30_CONFIG;
    ESP_RETURN_ON_ERROR(
        float_sensor_new_aht30(i2c_bus, &config, sensor),
        TAG, "Error creating sensor"
    );
#elif CONFIG_FLOAT_NODE_SENSOR_MOCK
    ESP_RETURN_ON_ERROR(
        float_sensor_new_mock(sensor),
        TAG, "Error creating mock sensor"
    );
#endif
    ESP_RETURN_ON_ERROR(
        float_sensor_init(*sensor),
        TAG, "Error initializing sensor"
    );
    return ESP_OK;
}

void send_data( float_sensor_handle_t sensor ) {

    float_datapoints_t *sensor_data = NULL;

    ESP_ERROR_CHECK(
        float_sensor_read_data( sensor, &sensor_data )
    );

    ESP_LOGI( TAG, "%d sensor data read", sensor_data->num_datapoints );

    ESP_ERROR_CHECK(
        float_now_add_peer( now_handle, paired_core )
    );

    ESP_ERROR_CHECK(
        float_now_send_data( now_handle, paired_core, sensor_data )
    );
    failed_sends = ( float_now_wait_for_ack( now_handle, FLOAT_PACKET_DATA, 2000 ) == ESP_OK ) ? 0 : failed_sends + 1;

    if ( failed_sends >= 5 ) {
        memset( paired_core, 0, ESP_NOW_ETH_ALEN );
        failed_sends = 0;
    }
}


void node_rx_callback( const uint8_t *mac, const float_now_packet_t *packet ) {
    switch ( packet->header.type ) {
        case FLOAT_PACKET_ACK:
            {
                float_now_payload_ack_t *ack = ( float_now_payload_ack_t * ) packet->payload;
                switch ( ack->ack_for_type ) {

                    case FLOAT_PACKET_PAIRING:
                        memcpy( paired_core, mac, ESP_NOW_ETH_ALEN );
                        break;

                    case FLOAT_PACKET_DATA:
                        failed_sends = 0;
                        break;
                }
            }
            break;
        default:
            ESP_LOGI( TAG, "node_rx_cb: unhandled packet type %d", packet->header.type );
            break;
    }
}


void app_main( void ){
#if !CONFIG_FLOAT_NODE_DEBUG_SLEEP
    if ( esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER )
    {
        ESP_LOGI( TAG, "Sleeping 30 sec for reflash purposes" );
        vTaskDelay( pdMS_TO_TICKS( 30000 ) );
    }
#endif

#if !CONFIG_FLOAT_NODE_SENSOR_MOCK
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_init(&i2c_bus);
#else
    i2c_master_bus_handle_t i2c_bus = NULL;
#endif
    float_sensor_handle_t sensor = NULL;
    ESP_ERROR_CHECK(init_sensor(&sensor, i2c_bus));

    float_blink_config_t blink_config = { .gpio_pin = GPIO_NUM_8 };
    ESP_ERROR_CHECK( float_blink_new( &blink_config, &blink_handle ) );

    esp_err_t nvs_err = nvs_flash_init();
    if ( nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND ) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( nvs_err );

    float_now_config_t float_now_config = {
        .rx_cb = node_rx_callback,
        .tx_cb = NULL,
    };
    ESP_ERROR_CHECK(
        float_now_new( &float_now_config, &now_handle )
    );

#if CONFIG_FLOAT_NODE_DEBUG_SLEEP
    ESP_LOGI( TAG, "Debug mode: looping with %d ms delay", CONFIG_FLOAT_NODE_SLEEP_DURATION_MS );
    while (1) {
        if ( !saved_peer() )
            pair_with_core();
        send_data( sensor );
        vTaskDelay( pdMS_TO_TICKS( CONFIG_FLOAT_NODE_SLEEP_DURATION_MS ) );
    }
#else
    if ( !saved_peer() )
        pair_with_core();
    send_data( sensor );
    esp_deep_sleep( 30 * 1000 * 1000 );
#endif
}
