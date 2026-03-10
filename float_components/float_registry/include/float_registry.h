#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_now.h"
#include "time.h"
#include "float_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLOAT_MAC_ADDR_LEN 6

typedef uint8_t float_mac_address_t[FLOAT_MAC_ADDR_LEN];

typedef struct float_registry_s *float_registry_handle_t;
typedef float float_reading_datatype_t;

typedef struct float_reading_s {
    float_reading_datatype_t value;
    time_t timestamp;
} float_reading_t;

#define FLOAT_RING_CAPACITY 32

typedef struct float_ringbuffer_s {
    float_sensor_type_t type;
    float_reading_t entries[FLOAT_RING_CAPACITY];
    size_t head;
    size_t size;
} float_ringbuffer_t;

typedef struct float_node_info_s {
    float_mac_address_t mac;
} float_node_info_t;

typedef struct float_registry_nvs_header_s {
    uint8_t registry_version;
    uint8_t count;
} float_registry_nvs_header_t;

typedef struct float_registry_nvs_blob_s {
    float_registry_nvs_header_t header;
    float_node_info_t nodes[];
} float_registry_nvs_blob_t;

typedef enum float_registry_event_e {
    FLOAT_REGISTRY_EVENT_NODE_ADDED,
    FLOAT_REGISTRY_EVENT_NODE_REMOVED,
    FLOAT_REGISTRY_EVENT_READING_UPDATED,
    FLOAT_REGISTRY_EVENT_NODE_UPDATED
} float_registry_event_t;

ESP_EVENT_DECLARE_BASE( FLOAT_REGISTRY_EVENTS );

// Lifecycle
esp_err_t float_registry_new( float_registry_handle_t *out_handle );
esp_err_t float_registry_delete( float_registry_handle_t handle );

// Node information management
esp_err_t float_registry_store_node_info( float_registry_handle_t handle, const float_node_info_t *info );
esp_err_t float_registry_get_node_info( float_registry_handle_t handle, const float_mac_address_t mac, float_node_info_t *out_info );
esp_err_t float_registry_forget_node( float_registry_handle_t handle, const float_mac_address_t mac );

// Node enumeration
esp_err_t float_registry_get_node_count( float_registry_handle_t handle, size_t *out_count );
esp_err_t float_registry_get_all_node_macs( float_registry_handle_t handle, float_mac_address_t *out_macs, size_t *inout_count );

// Reading management
esp_err_t float_registry_store_datapoints( float_registry_handle_t handle, const float_mac_address_t mac, const float_datapoint_t *datapoints, size_t count );

esp_err_t float_registry_get_latest_readings( float_registry_handle_t handle, const float_mac_address_t mac, float_reading_t *out_readings, size_t *inout_count );
esp_err_t float_registry_get_history( float_registry_handle_t handle, const float_mac_address_t mac, float_sensor_type_t type, float_reading_t *out_history, size_t *inout_count );
esp_err_t float_registry_get_max_last_24h( float_registry_handle_t handle, const float_mac_address_t mac, float_sensor_type_t type, float_reading_t *out_max_reading );
esp_err_t float_registry_get_min_last_24h( float_registry_handle_t handle, const float_mac_address_t mac, float_sensor_type_t type, float_reading_t *out_min_reading );

esp_err_t float_registry_full_contents_to_log( float_registry_handle_t handle );

#ifdef __cplusplus
}
#endif
