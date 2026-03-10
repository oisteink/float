#pragma once

#define FLOAT_NOW_MAJOR_VERSION 1
#define FLOAT_NOW_MINOR_VERSION 2
#define FLOAT_NOW_VERSION ((FLOAT_NOW_MAJOR_VERSION << 4) | FLOAT_NOW_MINOR_VERSION)

#define FLOAT_WIFI_CHANNEL 1

#include <stdint.h>
#include <stdbool.h>
#include "esp_now.h"
#include "esp_err.h"

#define FLOAT_NOW_PAYLOAD_SIZE( packet_size )  ( ( packet_size ) - sizeof( float_now_packet_header_t ) )

typedef uint8_t float_now_packet_type_t;

enum {
    FLOAT_PACKET_PAIRING = 1,
    FLOAT_PACKET_DATA,
    FLOAT_PACKET_ACK,
    FLOAT_PACKET_MAX
};

typedef struct float_datapoints_s float_now_payload_data_t;

typedef struct __attribute__((packed)) float_now_payload_ack_s {
    float_now_packet_type_t ack_for_type;
} float_now_payload_ack_t;

typedef struct __attribute__((packed)) float_now_payload_pairing_s {
    uint8_t flags;
} float_now_payload_pairing_t;

typedef struct __attribute__((packed)) float_now_packet_header_s {
    float_now_packet_type_t type;
    uint8_t version;
    uint16_t payload_size;
} float_now_packet_header_t;

typedef struct __attribute__((packed)) float_now_packet_s {
    float_now_packet_header_t header;
    uint8_t payload[];
} float_now_packet_t;

typedef float_now_packet_t *float_now_packet_handle_t;

typedef void ( *float_now_receive_callback_t ) ( const uint8_t *mac_addr, const float_now_packet_t *packet );
typedef void ( *float_now_send_callback_t ) ( const uint8_t *mac_addr, esp_now_send_status_t status );

typedef struct float_now_config_s {
    float_now_receive_callback_t rx_cb;
    float_now_send_callback_t tx_cb;
} float_now_config_t;

typedef struct float_now_s *float_now_handle_t;

// Lifecycle
esp_err_t float_now_new( const float_now_config_t *config, float_now_handle_t *out_handle );
esp_err_t float_now_delete( float_now_handle_t handle );

// Sending packets
esp_err_t float_now_new_packet( float_now_handle_t handle, float_now_packet_type_t packet_type, uint8_t num_datapoints, float_now_packet_handle_t *out_packet );
esp_err_t float_now_send_ack( float_now_handle_t handle, const uint8_t *peer_mac, float_now_packet_type_t ack_type );
esp_err_t float_now_send_pairing( float_now_handle_t handle, const uint8_t *peer_mac );
esp_err_t float_now_send_data( float_now_handle_t handle, const uint8_t *peer_mac, const float_now_payload_data_t *data_payload );
esp_err_t float_now_send_packet( float_now_handle_t handle, const uint8_t *peer_mac, const float_now_packet_t *packet );

// Peer management
esp_err_t float_now_add_peer( float_now_handle_t handle, const uint8_t *peer_mac );
esp_err_t float_now_remove_peer( float_now_handle_t handle, const uint8_t *peer_mac );
bool      float_now_is_peer_known( float_now_handle_t handle, const uint8_t *peer_id );

// ACK waiting
esp_err_t float_now_wait_for_ack( float_now_handle_t handle, float_now_packet_type_t packet_type, uint32_t wait_ms );
