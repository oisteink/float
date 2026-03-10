#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"

#include "float_data.h"
#include "float_now.h"
#include "float_now_private.h"

static const char *TAG = "float-now";

// File-static pointer used only by ESP-NOW send/recv callbacks,
// which have fixed signatures and cannot accept user data.
static float_now_t *s_instance = NULL;

static esp_err_t float_now_configure_wifi( void ) {
    ESP_LOGD( TAG, "float_now_configure_wifi()" );

    ESP_RETURN_ON_ERROR(
        esp_netif_init(),
        TAG, "Error initializing netif"
    );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(
        esp_wifi_init( &cfg ),
        TAG, "Error initializing WiFi"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_storage( WIFI_STORAGE_RAM ),
        TAG, "Error setting WiFi storage"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_mode( WIFI_MODE_STA ),
        TAG, "Error setting WiFi mode"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_start(),
        TAG, "Error starting WiFi"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_channel( FLOAT_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE ),
        TAG, "Error setting WiFi channel"
    );

    return ESP_OK;
}

const char *float_now_packet_type_to_str( float_now_packet_type_t packet_type ) {
    if ( packet_type < FLOAT_PACKET_PAIRING || packet_type >= FLOAT_PACKET_MAX )
        return "Unknown";
    return float_now_packet_type_str[packet_type - 1];
}

static int get_payload_size( const float_now_packet_t *data_packet ) {
    switch ( data_packet->header.type ) {
        case FLOAT_PACKET_PAIRING:
            return sizeof( float_now_payload_pairing_t );
        case FLOAT_PACKET_DATA: {
            float_now_payload_data_t *data_payload = ( float_now_payload_data_t * ) data_packet->payload;
            return sizeof( float_now_payload_data_t ) + sizeof( float_datapoint_t ) * data_payload->num_datapoints;
        }
        case FLOAT_PACKET_ACK:
            return sizeof( float_now_payload_ack_t );
        default:
            ESP_LOGE( TAG, "Unimplemented packet type" );
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t float_now_add_peer( float_now_handle_t handle, const uint8_t *mac ) {
    ESP_LOGD( TAG, "float_now_add_peer()" );

    esp_err_t ret = ESP_OK;
    if ( !esp_now_is_peer_exist( mac ) ) {
        esp_now_peer_info_t peer = {
            .peer_addr = { 0 },
            .channel = FLOAT_WIFI_CHANNEL,
            .encrypt = false,
            .ifidx = ESP_IF_WIFI_STA
        };
        memcpy( peer.peer_addr, mac, ESP_NOW_ETH_ALEN );
        ret = esp_now_add_peer( &peer );
    }
    return ret;
}

esp_err_t float_now_remove_peer( float_now_handle_t handle, const uint8_t *mac ) {
    ESP_LOGD( TAG, "float_now_remove_peer()" );

    esp_err_t ret = ESP_OK;
    if ( esp_now_is_peer_exist( mac ) )
        ret = esp_now_del_peer( mac );
    return ret;
}

bool float_now_is_peer_known( float_now_handle_t handle, const uint8_t *peer_id ) {
    ESP_LOGD( TAG, "float_now_is_peer_known()" );
    return esp_now_is_peer_exist( peer_id );
}

esp_err_t float_now_wait_for_ack( float_now_handle_t handle, float_now_packet_type_t packet_type, uint32_t wait_ms ) {
    ESP_LOGD( TAG, "float_now_wait_for_ack()" );
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_INVALID_ARG, TAG, "handle is NULL" );

    EventBits_t event_bit = 0;
    switch ( packet_type ) {
        case FLOAT_PACKET_PAIRING:
            event_bit = PAIRING_ACK_BIT;
            break;
        case FLOAT_PACKET_DATA:
            event_bit = DATA_ACK_BIT;
            break;
        default:
            ESP_LOGE( TAG, "Illegal packet type for ACK" );
            return ESP_ERR_INVALID_ARG;
    }
    return
        ( xEventGroupWaitBits( handle->event_group, event_bit, pdTRUE, pdFALSE, pdMS_TO_TICKS( wait_ms ) ) > 0 )
        ? ESP_OK
        : ESP_ERR_TIMEOUT;
}

esp_err_t float_now_send_data( float_now_handle_t handle, const uint8_t *peer_mac, const float_now_payload_data_t *data_payload ) {
    ESP_LOGD( TAG, "float_now_send_data()" );
    ESP_RETURN_ON_FALSE( data_payload, ESP_ERR_INVALID_ARG, TAG, "data_payload is NULL" );

    size_t payload_size = sizeof( float_now_payload_data_t ) + sizeof( float_datapoint_t ) * data_payload->num_datapoints;
    size_t packet_size = sizeof( float_now_packet_t ) + payload_size;

    float_now_packet_t *data_packet = calloc( 1, packet_size );
    ESP_RETURN_ON_FALSE( data_packet, ESP_ERR_NO_MEM, TAG, "Error allocating memory for data packet" );

    data_packet->header.type = FLOAT_PACKET_DATA;
    data_packet->header.payload_size = payload_size;
    data_packet->header.version = FLOAT_NOW_VERSION;
    memcpy( data_packet->payload, data_payload, payload_size );

    esp_err_t ret = float_now_send_packet( handle, peer_mac, data_packet );
    free( data_packet );
    return ret;
}

esp_err_t float_now_send_ack( float_now_handle_t handle, const uint8_t *peer_mac, float_now_packet_type_t ack_type ) {
    ESP_LOGD( TAG, "float_now_send_ack()" );

    size_t payload_size = sizeof( float_now_payload_ack_t );
    size_t packet_size = sizeof( float_now_packet_t ) + payload_size;

    float_now_packet_t *ack = calloc( 1, packet_size );
    ESP_RETURN_ON_FALSE( ack, ESP_ERR_NO_MEM, TAG, "Error allocating memory for ack packet" );

    ack->header.type = FLOAT_PACKET_ACK;
    ack->header.payload_size = payload_size;
    ack->header.version = FLOAT_NOW_VERSION;
    (( float_now_payload_ack_t * ) ack->payload)->ack_for_type = ack_type;

    esp_err_t ret = float_now_send_packet( handle, peer_mac, ack );
    free( ack );
    return ret;
}

esp_err_t float_now_send_pairing( float_now_handle_t handle, const uint8_t *peer_mac ) {
    ESP_LOGD( TAG, "float_now_send_pairing()" );

    size_t payload_size = sizeof( float_now_payload_pairing_t );
    size_t packet_size = sizeof( float_now_packet_t ) + payload_size;

    float_now_packet_t *pairing = calloc( 1, packet_size );
    ESP_RETURN_ON_FALSE( pairing, ESP_ERR_NO_MEM, TAG, "Error allocating memory for pairing packet" );

    pairing->header.type = FLOAT_PACKET_PAIRING;
    pairing->header.payload_size = payload_size;
    pairing->header.version = FLOAT_NOW_VERSION;
    (( float_now_payload_pairing_t * ) pairing->payload)->flags = 0;

    esp_err_t ret = float_now_send_packet( handle, peer_mac, pairing );
    free( pairing );
    return ret;
}

esp_err_t float_now_new_packet( float_now_handle_t handle, float_now_packet_type_t packet_type, uint8_t num_datapoints, float_now_packet_handle_t *out_packet ) {
    ESP_LOGD( TAG, "float_now_new_packet()" );

    size_t payload_size = 0;
    switch ( packet_type ) {
        case FLOAT_PACKET_PAIRING:
            payload_size = sizeof( float_now_payload_pairing_t );
            break;
        case FLOAT_PACKET_DATA:
            payload_size = sizeof( float_now_payload_data_t ) + sizeof( float_datapoint_t ) * num_datapoints;
            break;
        case FLOAT_PACKET_ACK:
            payload_size = sizeof( float_now_payload_ack_t );
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    size_t packet_size = sizeof( float_now_packet_t ) + payload_size;
    float_now_packet_t *packet = calloc( 1, packet_size );
    ESP_RETURN_ON_FALSE( packet, ESP_ERR_NO_MEM, TAG, "Error allocating memory for packet" );

    packet->header.type = packet_type;
    packet->header.payload_size = payload_size;
    packet->header.version = FLOAT_NOW_VERSION;
    *out_packet = packet;
    return ESP_OK;
}

esp_err_t float_now_send_packet( float_now_handle_t handle, const uint8_t *peer_mac, const float_now_packet_t *packet ) {
    ESP_LOGD( TAG, "float_now_send_packet()" );
    ESP_RETURN_ON_FALSE( peer_mac && packet, ESP_ERR_INVALID_ARG, TAG, "NULL pointer passed to float_now_send_packet" );

    if ( !esp_now_is_peer_exist( peer_mac ) )
        ESP_RETURN_ON_ERROR(
            float_now_add_peer( handle, peer_mac ),
            TAG, "Error adding esp_now peer during send"
        );

    size_t payload_size = get_payload_size( packet );
    size_t packet_size = sizeof( float_now_packet_t ) + payload_size;
    return esp_now_send( peer_mac, ( const uint8_t * ) packet, packet_size );
}

static void float_now_espnow_send_cb( const esp_now_send_info_t *send_info, esp_now_send_status_t status ) {
    float_now_event_t event = {
        .type = SEND_EVENT,
        .send.status = status
    };
    memcpy( &event.send.dest_mac, send_info->des_addr, ESP_NOW_ETH_ALEN );
    xQueueSend( s_instance->event_queue, &event, 0 );
}

static void float_now_espnow_recv_cb( const esp_now_recv_info_t *recv_info, const uint8_t *data, int len ) {
    float_now_packet_t *packet = malloc( len );
    if ( !packet ) {
        ESP_LOGE( TAG, "Failed to allocate memory for received packet" );
        return;
    }
    memcpy( packet, data, len );

    float_now_event_t event = {
        .type = RECEIVE_EVENT,
        .receive.data_packet = packet,
    };
    memcpy( &event.receive.source_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN );
    xQueueSend( s_instance->event_queue, &event, 0 );
}

static void float_now_event_handler( void *pvParameters ) {
    float_now_handle_t handle = ( float_now_handle_t ) pvParameters;
    float_now_event_t event;
    while ( xQueueReceive( handle->event_queue, &event, portMAX_DELAY ) == pdTRUE ) {
        switch ( event.type ) {
            case SEND_EVENT:
                if ( handle->config.tx_cb )
                    handle->config.tx_cb( event.send.dest_mac, event.send.status );
                break;
            case RECEIVE_EVENT:
                if ( event.receive.data_packet->header.version != FLOAT_NOW_VERSION ) {
                    ESP_LOGW( TAG, "Version mismatch: got %d, expected %d",
                              event.receive.data_packet->header.version, FLOAT_NOW_VERSION );
                    free( event.receive.data_packet );
                    break;
                }
                if ( event.receive.data_packet->header.type == FLOAT_PACKET_ACK ) {
                    float_now_payload_ack_t *ack_payload = ( float_now_payload_ack_t * ) event.receive.data_packet->payload;
                    switch ( ack_payload->ack_for_type ) {
                        case FLOAT_PACKET_DATA:
                            xEventGroupSetBits( handle->event_group, DATA_ACK_BIT );
                            break;
                        case FLOAT_PACKET_PAIRING:
                            xEventGroupSetBits( handle->event_group, PAIRING_ACK_BIT );
                            break;
                        default:
                            ESP_LOGE( TAG, "ACK unsupported for packet type %d", ack_payload->ack_for_type );
                            break;
                    }
                }
                if ( handle->config.rx_cb )
                    handle->config.rx_cb( event.receive.source_mac, event.receive.data_packet );
                free( event.receive.data_packet );
                break;
            default:
                ESP_LOGE( TAG, "Unknown event type" );
                break;
        }
    }
}

esp_err_t float_now_new( const float_now_config_t *config, float_now_handle_t *out_handle ) {
    ESP_LOGD( TAG, "float_now_new()" );
    ESP_RETURN_ON_FALSE( config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid args to float_now_new" );

    float_now_handle_t handle = calloc( 1, sizeof( struct float_now_s ) );
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_NO_MEM, TAG, "Failed to allocate float_now handle" );

    memcpy( &handle->config, config, sizeof( float_now_config_t ) );

    handle->event_queue = xQueueCreate( 10, sizeof( float_now_event_t ) );
    ESP_RETURN_ON_FALSE( handle->event_queue, ESP_ERR_NO_MEM, TAG, "Error creating event queue" );

    handle->event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE( handle->event_group, ESP_ERR_NO_MEM, TAG, "Error creating event group" );

    ESP_RETURN_ON_ERROR(
        float_now_configure_wifi(),
        TAG, "Error configuring WiFi"
    );

    ESP_RETURN_ON_ERROR(
        esp_now_init(),
        TAG, "Error initializing ESP-NOW"
    );

    s_instance = handle;

    ESP_RETURN_ON_ERROR(
        esp_now_register_recv_cb( float_now_espnow_recv_cb ),
        TAG, "Error registering float_now receive callback"
    );
    ESP_RETURN_ON_ERROR(
        esp_now_register_send_cb( float_now_espnow_send_cb ),
        TAG, "Error registering float_now send callback"
    );

    esp_err_t ret = xTaskCreate( float_now_event_handler, "fn_events", 4096, handle, tskIDLE_PRIORITY, &handle->task_handle ) == pdPASS
        ? ESP_OK
        : ESP_FAIL;
    ESP_RETURN_ON_ERROR( ret, TAG, "Error creating float_now_event_handler task" );

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t float_now_delete( float_now_handle_t handle ) {
    if ( !handle ) return ESP_OK;
    if ( handle->task_handle )  vTaskDelete( handle->task_handle );
    if ( handle->event_queue )  vQueueDelete( handle->event_queue );
    if ( handle->event_group )  vEventGroupDelete( handle->event_group );
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    s_instance = NULL;
    free( handle );
    return ESP_OK;
}
