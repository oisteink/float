#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "time.h"
#include "float_registry.h"

ESP_EVENT_DEFINE_BASE( FLOAT_REGISTRY_EVENTS );

#define FLOAT_REGISTRY_NVS_NAMESPACE "float_registry"
#define FLOAT_REGISTRY_NVS_KEY "nodes"
#define FLOAT_REGISTRY_VERSION 1
#define FLOAT_REGISTRY_MAX_NODES 10

typedef struct float_registry_node_s {
    float_node_info_t info;
    size_t ring_count;
    float_ringbuffer_t *rings;
} float_registry_node_t;

struct float_registry_s {
    float_registry_node_t nodes[FLOAT_REGISTRY_MAX_NODES];
    uint8_t node_count;
    SemaphoreHandle_t mutex;
};

static const char *TAG = "float_registry";

static int _index_of_mac( float_registry_handle_t handle, const float_mac_address_t mac ) {
    if ( handle == NULL ) {
        ESP_LOGE( TAG, "Handle is NULL" );
        abort();
    }

    for ( uint8_t i = 0; i < handle->node_count; i++ )
        if ( memcmp( handle->nodes[i].info.mac, mac, ESP_NOW_ETH_ALEN ) == 0 )
            return i;

    return -1;
}

static esp_err_t _get_ringbuffer( float_registry_node_t *node, float_sensor_type_t type, float_ringbuffer_t **ringbuffer ) {
    for ( size_t i = 0; i < node->ring_count; ++i ) {
        if ( node->rings[i].type == type ) {
            *ringbuffer = &node->rings[i];
            return ESP_OK;
        }
    }

    float_ringbuffer_t *new_rings = realloc( node->rings, (node->ring_count + 1) * sizeof( float_ringbuffer_t ) );
    ESP_RETURN_ON_FALSE(
        new_rings,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for new ring"
    );
    node->rings = new_rings;
    float_ringbuffer_t *new_ring = &node->rings[node->ring_count];
    node->ring_count++;

    memset( new_ring, 0, sizeof( *new_ring ) );
    new_ring->type = type;

    *ringbuffer = new_ring;

    return ESP_OK;
}

static esp_err_t _get_node_runtime_data( float_registry_handle_t handle, const float_mac_address_t mac, float_registry_node_t **out_node )
{
    ESP_RETURN_ON_FALSE( handle && mac && out_node, ESP_ERR_INVALID_ARG, TAG, "Invalid args to get_node_runtime" );

    int index = _index_of_mac( handle, mac );
    if ( index < 0 ) {
        ESP_RETURN_ON_FALSE( handle->node_count < FLOAT_REGISTRY_MAX_NODES, ESP_ERR_NO_MEM, TAG, "Max node limit reached" );
        index = handle->node_count++;
        memset( &handle->nodes[index], 0, sizeof( float_registry_node_t ) );
        memcpy( handle->nodes[index].info.mac, mac, sizeof( float_mac_address_t ) );
    }

    *out_node = &handle->nodes[index];
    return ESP_OK;
}

static esp_err_t _ringbuffer_add_reading( float_ringbuffer_t *ring, float_reading_datatype_t value ) {
    float_reading_t *slot = &ring->entries[ring->head];
    slot->timestamp = time( NULL );
    slot->value = value;

    ring->head = (ring->head + 1) % FLOAT_RING_CAPACITY;
    if ( ring->size < FLOAT_RING_CAPACITY ) {
        ring->size++;
    }
    return ESP_OK;
}

static esp_err_t float_registry_load_from_nvs( float_registry_handle_t handle )
{
    esp_err_t ret = ESP_OK;

    float_registry_nvs_blob_t *blob = NULL;

    nvs_handle_t nvs;
    ESP_GOTO_ON_ERROR(
        nvs_open( FLOAT_REGISTRY_NVS_NAMESPACE, NVS_READWRITE, &nvs ),
        end, TAG, "Failed to open NVS namespace %s", FLOAT_REGISTRY_NVS_NAMESPACE
    );

    size_t required_size = 0;
    ESP_GOTO_ON_ERROR(
        nvs_get_blob( nvs, FLOAT_REGISTRY_NVS_KEY, NULL, &required_size ),
        end, TAG, "Failed to get size of NVS key %s", FLOAT_REGISTRY_NVS_KEY
    );

    ESP_GOTO_ON_FALSE(
        required_size >= sizeof( float_registry_nvs_header_t ),
        ESP_OK,
        end, TAG, "NVS key %s is too small", FLOAT_REGISTRY_NVS_KEY
    );

    blob = malloc( required_size );
    ESP_GOTO_ON_FALSE(
        blob,
        ESP_ERR_NO_MEM,
        end, TAG, "Failed to allocate memory for NVS key %s", FLOAT_REGISTRY_NVS_KEY
    );

    ESP_GOTO_ON_FALSE(
        nvs_get_blob( nvs, FLOAT_REGISTRY_NVS_KEY, blob, &required_size ) == ESP_OK,
        ESP_OK,
        end, TAG, "Failed to get NVS key %s", FLOAT_REGISTRY_NVS_KEY
    );

    ESP_GOTO_ON_FALSE(
        blob->header.registry_version == FLOAT_REGISTRY_VERSION,
        ESP_OK,
        end, TAG, "NVS key %s has invalid version %d", FLOAT_REGISTRY_NVS_KEY, blob->header.registry_version
    );

    ESP_GOTO_ON_FALSE(
        blob->header.count <= FLOAT_REGISTRY_MAX_NODES,
        ESP_OK,
        end, TAG, "NVS key %s has too many nodes %d", FLOAT_REGISTRY_NVS_KEY, blob->header.count
    );

    for ( uint8_t i = 0; i < blob->header.count; i++ )
        handle->nodes[i].info = blob->nodes[i];
    handle->node_count = blob->header.count;
    ESP_LOGD( TAG, "Loaded %d nodes from NVS", handle->node_count );

end:
    nvs_close( nvs );

    if ( blob)
        free( blob );

    if ( ret == ESP_ERR_NVS_NOT_FOUND )
        ret = ESP_OK;

    return ret;
}

static esp_err_t float_registry_save_to_nvs( float_registry_handle_t handle )
{
    esp_err_t ret = ESP_OK;
    size_t blob_size = sizeof( float_registry_nvs_header_t ) + handle->node_count * sizeof( float_node_info_t );

    float_registry_nvs_blob_t *blob = NULL;
    blob = malloc( blob_size );
    ESP_RETURN_ON_FALSE(
        blob,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for NVS key %s", FLOAT_REGISTRY_NVS_KEY
    );

    blob->header.registry_version = FLOAT_REGISTRY_VERSION;
    blob->header.count = handle->node_count;
    for ( uint8_t i = 0; i < handle->node_count; i++ )
        blob->nodes[i] = handle->nodes[i].info;

    nvs_handle_t nvs;
    ESP_GOTO_ON_ERROR(
        nvs_open( FLOAT_REGISTRY_NVS_NAMESPACE, NVS_READWRITE, &nvs ),
        end,
        TAG, "Failed to open NVS namespace %s", FLOAT_REGISTRY_NVS_NAMESPACE
    );

    ESP_GOTO_ON_ERROR(
        nvs_set_blob( nvs, FLOAT_REGISTRY_NVS_KEY, blob, blob_size ),
        end,
        TAG, "Failed to set NVS key %s", FLOAT_REGISTRY_NVS_KEY
    );

    ESP_GOTO_ON_ERROR(
        nvs_commit( nvs ),
        end,
        TAG, "Failed to commit NVS key %s", FLOAT_REGISTRY_NVS_KEY
    );

end:
    nvs_close( nvs );
    if ( blob )
        free( blob );

    return ret;
}

esp_err_t float_registry_new( float_registry_handle_t *out_handle )
{
    ESP_RETURN_ON_FALSE( out_handle, ESP_ERR_INVALID_ARG, TAG, "out_handle is NULL" );

    float_registry_handle_t handle = NULL;
    handle = calloc( 1, sizeof( struct float_registry_s ) );
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_NO_MEM, TAG, "Failed to allocate registry handle" );

    handle->mutex = xSemaphoreCreateMutex();
    if ( !handle->mutex ) {
        free( handle );
        ESP_RETURN_ON_FALSE( false, ESP_ERR_NO_MEM, TAG, "Failed to create registry mutex" );
    }

    if ( float_registry_load_from_nvs( handle ) != ESP_OK )
        ESP_LOGD( TAG, "Failed to load registry from NVS" );

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t float_registry_delete( float_registry_handle_t handle )
{
    if ( handle ) {
        for ( uint8_t i = 0; i < handle->node_count; i++ )
            if ( handle->nodes[i].rings )
                free( handle->nodes[i].rings );
        if ( handle->mutex )
            vSemaphoreDelete( handle->mutex );
        free( handle );
    }
    return ESP_OK;
}

esp_err_t float_registry_store_node_info( float_registry_handle_t handle, const float_node_info_t *info )
{
    ESP_RETURN_ON_FALSE( handle && info, ESP_ERR_INVALID_ARG, TAG, "Invalid args to store_node_info" );

    esp_err_t ret = ESP_OK;
    float_registry_event_t event = FLOAT_REGISTRY_EVENT_NODE_ADDED;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    int index = _index_of_mac( handle, info->mac );

    if ( index >= 0 ) {
        handle->nodes[index].info = *info;
        event = FLOAT_REGISTRY_EVENT_NODE_UPDATED;
    } else {
        ESP_GOTO_ON_FALSE( handle->node_count < FLOAT_REGISTRY_MAX_NODES, ESP_ERR_NO_MEM, end, TAG, "Max node limit reached" );
        memset( &handle->nodes[handle->node_count], 0, sizeof( float_registry_node_t ) );
        handle->nodes[handle->node_count++].info = *info;
    }

    ESP_GOTO_ON_ERROR( float_registry_save_to_nvs( handle ), end, TAG, "Failed to save updated node list to NVS" );

end:
    xSemaphoreGive( handle->mutex );
    if ( ret == ESP_OK )
        esp_event_post( FLOAT_REGISTRY_EVENTS, event, info->mac, sizeof( float_mac_address_t ), 0 );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_get_node_info( float_registry_handle_t handle, const float_mac_address_t mac, float_node_info_t *out_info )
{
    ESP_RETURN_ON_FALSE( handle && mac && out_info, ESP_ERR_INVALID_ARG, TAG, "Invalid args to get_node_info" );

    esp_err_t ret = ESP_ERR_NOT_FOUND;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    int index = _index_of_mac( handle, mac );
    if ( index >= 0 ) {
        *out_info = handle->nodes[index].info;
        ret = ESP_OK;
    }

    xSemaphoreGive( handle->mutex );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_forget_node( float_registry_handle_t handle, const float_mac_address_t mac )
{
    ESP_RETURN_ON_FALSE( handle && mac, ESP_ERR_INVALID_ARG, TAG, "Invalid args to remove_node" );

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    int index = _index_of_mac( handle, mac );

    if ( index < 0 ) {
        ESP_LOGD( TAG, "Forget node not found" );
        ret = ESP_ERR_NOT_FOUND;
        goto end;
    }

    if ( handle->nodes[index].rings )
        free( handle->nodes[index].rings );

    if ( index != handle->node_count - 1 )
        handle->nodes[index] = handle->nodes[handle->node_count - 1];

    memset( &handle->nodes[handle->node_count - 1], 0, sizeof( float_registry_node_t ) );
    handle->node_count--;
    float_registry_save_to_nvs( handle );

end:
    xSemaphoreGive( handle->mutex );
    if ( ret == ESP_OK )
        esp_event_post( FLOAT_REGISTRY_EVENTS, FLOAT_REGISTRY_EVENT_NODE_REMOVED, mac, sizeof( float_mac_address_t ), 0 );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_store_datapoints( float_registry_handle_t handle, const float_mac_address_t mac, const float_datapoint_t *datapoints, size_t count ) {
    ESP_RETURN_ON_FALSE( handle && mac && datapoints, ESP_ERR_INVALID_ARG, TAG, "Invalid args to store_readings" );

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    float_registry_node_t *node = NULL;
    ESP_GOTO_ON_ERROR(
        _get_node_runtime_data( handle, mac, &node ),
        end, TAG, "Failed to get node runtime data"
    );

    for ( size_t i = 0; i < count; ++i ) {
        const float_datapoint_t *dp = &datapoints[i];
        ESP_GOTO_ON_FALSE(
            dp->reading_type < FLOAT_SENSOR_TYPE_MAX,
            ESP_ERR_INVALID_ARG, end, TAG, "Invalid sensor type %d", dp->reading_type
        );
        float_ringbuffer_t *ring = NULL;
        ESP_GOTO_ON_ERROR(
            _get_ringbuffer( node, (float_sensor_type_t)dp->reading_type, &ring ),
            end, TAG, "Failed to get ringbuffer for sensor type %d", dp->reading_type
        );
        _ringbuffer_add_reading( ring, dp->value );
    }

end:
    xSemaphoreGive( handle->mutex );
    if ( ret == ESP_OK )
        esp_event_post( FLOAT_REGISTRY_EVENTS, FLOAT_REGISTRY_EVENT_READING_UPDATED, mac, sizeof( float_mac_address_t ), 0 );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_get_latest_readings( float_registry_handle_t handle, const float_mac_address_t mac, float_reading_t *out_readings, size_t *inout_count )
{
    ESP_RETURN_ON_FALSE( handle && mac && out_readings && inout_count, ESP_ERR_INVALID_ARG, TAG, "Invalid args to get_latest_readings" );

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    int index = _index_of_mac( handle, mac );
    ESP_GOTO_ON_FALSE( index >= 0, ESP_ERR_NOT_FOUND, end, TAG, "Node not found" );

    float_registry_node_t *node = &handle->nodes[index];
    size_t available = node->ring_count;
    size_t to_copy = available < *inout_count ? available : *inout_count;

    for ( size_t i = 0; i < to_copy; ++i ) {
        float_ringbuffer_t *ring = &node->rings[i];
        if ( ring->size == 0 ) {
            out_readings[i].timestamp = 0;
            out_readings[i].value = 0;
            continue;
        }
        size_t latest = ( ring->head + FLOAT_RING_CAPACITY - 1 ) % FLOAT_RING_CAPACITY;
        out_readings[i] = ring->entries[latest];
    }
    *inout_count = to_copy;

end:
    xSemaphoreGive( handle->mutex );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_get_history( float_registry_handle_t handle, const float_mac_address_t mac, float_sensor_type_t type, float_reading_t *out_history, size_t *inout_count )
{
    ESP_RETURN_ON_FALSE( handle && mac && out_history && inout_count, ESP_ERR_INVALID_ARG, TAG, "Invalid args to get_history" );

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    int index = _index_of_mac( handle, mac );
    ESP_GOTO_ON_FALSE( index >= 0, ESP_ERR_NOT_FOUND, end, TAG, "Node not found" );

    float_registry_node_t *node = &handle->nodes[index];
    float_ringbuffer_t *ring = NULL;
    for ( size_t i = 0; i < node->ring_count; ++i ) {
        if ( node->rings[i].type == type ) {
            ring = &node->rings[i];
            break;
        }
    }
    ESP_GOTO_ON_FALSE( ring, ESP_ERR_NOT_FOUND, end, TAG, "Sensor type not found" );

    size_t to_copy = ring->size < *inout_count ? ring->size : *inout_count;
    size_t start = ( ring->head + FLOAT_RING_CAPACITY - ring->size ) % FLOAT_RING_CAPACITY;
    for ( size_t i = 0; i < to_copy; ++i ) {
        out_history[i] = ring->entries[( start + i ) % FLOAT_RING_CAPACITY];
    }
    *inout_count = to_copy;

end:
    xSemaphoreGive( handle->mutex );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_get_max_last_24h( float_registry_handle_t handle, const float_mac_address_t mac, float_sensor_type_t type, float_reading_t *out_max_reading )
{
    ESP_RETURN_ON_FALSE( handle && mac && out_max_reading, ESP_ERR_INVALID_ARG, TAG, "Invalid args to get_max_last_24h" );

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    int index = _index_of_mac( handle, mac );
    ESP_GOTO_ON_FALSE( index >= 0, ESP_ERR_NOT_FOUND, end, TAG, "Node not found" );

    float_registry_node_t *node = &handle->nodes[index];
    float_ringbuffer_t *ring = NULL;
    for ( size_t i = 0; i < node->ring_count; ++i ) {
        if ( node->rings[i].type == type ) {
            ring = &node->rings[i];
            break;
        }
    }
    ESP_GOTO_ON_FALSE( ring && ring->size > 0, ESP_ERR_NOT_FOUND, end, TAG, "No readings for sensor type" );

    time_t now = time( NULL );
    time_t cutoff = now - 86400;
    bool found = false;
    float_reading_t max_reading = { 0 };
    size_t start = ( ring->head + FLOAT_RING_CAPACITY - ring->size ) % FLOAT_RING_CAPACITY;

    for ( size_t i = 0; i < ring->size; ++i ) {
        float_reading_t *r = &ring->entries[( start + i ) % FLOAT_RING_CAPACITY];
        if ( r->timestamp >= cutoff && ( !found || r->value > max_reading.value ) ) {
            max_reading = *r;
            found = true;
        }
    }

    ESP_GOTO_ON_FALSE( found, ESP_ERR_NOT_FOUND, end, TAG, "No readings in last 24h" );
    *out_max_reading = max_reading;

end:
    xSemaphoreGive( handle->mutex );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_get_min_last_24h( float_registry_handle_t handle, const float_mac_address_t mac, float_sensor_type_t type, float_reading_t *out_min_reading )
{
    ESP_RETURN_ON_FALSE( handle && mac && out_min_reading, ESP_ERR_INVALID_ARG, TAG, "Invalid args to get_min_last_24h" );

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    int index = _index_of_mac( handle, mac );
    ESP_GOTO_ON_FALSE( index >= 0, ESP_ERR_NOT_FOUND, end, TAG, "Node not found" );

    float_registry_node_t *node = &handle->nodes[index];
    float_ringbuffer_t *ring = NULL;
    for ( size_t i = 0; i < node->ring_count; ++i ) {
        if ( node->rings[i].type == type ) {
            ring = &node->rings[i];
            break;
        }
    }
    ESP_GOTO_ON_FALSE( ring && ring->size > 0, ESP_ERR_NOT_FOUND, end, TAG, "No readings for sensor type" );

    time_t now = time( NULL );
    time_t cutoff = now - 86400;
    bool found = false;
    float_reading_t min_reading = { 0 };
    size_t start = ( ring->head + FLOAT_RING_CAPACITY - ring->size ) % FLOAT_RING_CAPACITY;

    for ( size_t i = 0; i < ring->size; ++i ) {
        float_reading_t *r = &ring->entries[( start + i ) % FLOAT_RING_CAPACITY];
        if ( r->timestamp >= cutoff && ( !found || r->value < min_reading.value ) ) {
            min_reading = *r;
            found = true;
        }
    }

    ESP_GOTO_ON_FALSE( found, ESP_ERR_NOT_FOUND, end, TAG, "No readings in last 24h" );
    *out_min_reading = min_reading;

end:
    xSemaphoreGive( handle->mutex );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_get_node_count( float_registry_handle_t handle, size_t *out_count )
{
    ESP_RETURN_ON_FALSE( handle && out_count, ESP_ERR_INVALID_ARG, TAG, "Invalid args to get_node_count" );

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    *out_count = handle->node_count;

    xSemaphoreGive( handle->mutex );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_get_all_node_macs( float_registry_handle_t handle, float_mac_address_t *out_macs, size_t *inout_count )
{
    ESP_RETURN_ON_FALSE( handle && inout_count, ESP_ERR_INVALID_ARG, TAG, "Invalid args to get_node_macs" );

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    if ( out_macs == NULL ) {
        *inout_count = handle->node_count;
        goto end;
    }

    ESP_GOTO_ON_FALSE( *inout_count >= handle->node_count, ESP_ERR_NO_MEM, end, TAG, "Not enough space for node macs" );

    for ( size_t i = 0; i < handle->node_count; i++ )
        memcpy( out_macs[i], handle->nodes[i].info.mac, ESP_NOW_ETH_ALEN );

    *inout_count = handle->node_count;

end:
    xSemaphoreGive( handle->mutex );
end_no_unlock:
    return ret;
}

esp_err_t float_registry_full_contents_to_log( float_registry_handle_t handle ) {
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(
        xSemaphoreTake( handle->mutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE,
        ESP_ERR_TIMEOUT, end_no_unlock, TAG, "Registry mutex timeout"
    );

    printf( "---- Float Registry Dump ----\n" );
    printf( "Nodes: %u\n", (unsigned) handle->node_count );
    printf( "------------------------------\n" );

    for ( size_t i = 0; i < handle->node_count; ++i ) {
        float_registry_node_t *node = &handle->nodes[i];
        printf( " Node %zu — MAC: "MACSTR", Rings: %zu\n", i, MAC2STR( node->info.mac ), node->ring_count );

        for ( size_t j = 0; j < node->ring_count; ++j ) {
            float_ringbuffer_t *ring = &node->rings[j];
            printf( "   Sensor Type: %u — Readings: %u\n", (unsigned) ring->type, (unsigned) ring->size );

            for ( size_t k = 0; k < ring->size; ++k ) {
                float_reading_t *r = &ring->entries[k];
                if ( r->timestamp == 0 )
                    continue;
                printf( "     [%zu] ts=%llu, value=%.2f\n", k, r->timestamp, r->value );
            }
        }
    }

    printf( "------------------------------\n" );

    xSemaphoreGive( handle->mutex );
end_no_unlock:
    return ret;
}
