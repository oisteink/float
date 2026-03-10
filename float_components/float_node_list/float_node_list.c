#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"
#include "float_node_list.h"

static const char *TAG = "float_node_list";

#define MAX_NODES 10

typedef struct {
    uint8_t mac[6];
    lv_obj_t *card;
    lv_obj_t *lbl_title;
    lv_obj_t *lbl_temp;
    lv_obj_t *lbl_hum;
    lv_obj_t *lbl_pres;
    bool in_use;
} node_entry_t;

struct float_node_list_s {
    lv_obj_t *container;
    lv_obj_t *lbl_status;
    node_entry_t nodes[MAX_NODES];
    uint8_t count;
};

static node_entry_t *find_entry( float_node_list_handle_t h, const uint8_t mac[6] )
{
    for ( int i = 0; i < MAX_NODES; i++ ) {
        if ( h->nodes[i].in_use && memcmp( h->nodes[i].mac, mac, 6 ) == 0 ) {
            return &h->nodes[i];
        }
    }
    return NULL;
}

static node_entry_t *alloc_entry( float_node_list_handle_t h )
{
    for ( int i = 0; i < MAX_NODES; i++ ) {
        if ( !h->nodes[i].in_use ) {
            return &h->nodes[i];
        }
    }
    return NULL;
}

esp_err_t float_node_list_new( lv_obj_t *parent,
                                float_node_list_handle_t *out_handle )
{
    ESP_RETURN_ON_FALSE( parent && out_handle, ESP_ERR_INVALID_ARG,
                         TAG, "NULL argument" );

    float_node_list_handle_t h = calloc( 1, sizeof( struct float_node_list_s ) );
    ESP_RETURN_ON_FALSE( h, ESP_ERR_NO_MEM, TAG, "Handle alloc failed" );

    lv_obj_t *cont = lv_obj_create( parent );
    lv_obj_set_size( cont, LV_PCT( 100 ), LV_PCT( 100 ) );
    lv_obj_set_flex_flow( cont, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER );
    lv_obj_set_scroll_dir( cont, LV_DIR_VER );
    lv_obj_set_style_pad_row( cont, 8, 0 );
    lv_obj_set_style_pad_all( cont, 8, 0 );
    lv_obj_set_style_bg_color( cont, lv_color_hex( 0x1a1a2e ), 0 );
    lv_obj_set_style_bg_opa( cont, LV_OPA_COVER, 0 );
    lv_obj_set_style_border_width( cont, 0, 0 );

    lv_obj_t *status = lv_label_create( cont );
    lv_label_set_text( status, LV_SYMBOL_WARNING " LINK DOWN" );
    lv_obj_set_style_text_color( status, lv_color_hex( 0xff4444 ), 0 );
    lv_obj_set_style_text_font( status, &lv_font_montserrat_24, 0 );
    lv_obj_set_style_pad_bottom( status, 4, 0 );

    h->container = cont;
    h->lbl_status = status;
    *out_handle = h;
    return ESP_OK;
}

esp_err_t float_node_list_delete( float_node_list_handle_t handle )
{
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_INVALID_ARG, TAG, "NULL handle" );
    if ( handle->container ) {
        lv_obj_del( handle->container );
    }
    free( handle );
    return ESP_OK;
}

esp_err_t float_node_list_add_node( float_node_list_handle_t handle,
                                     const uint8_t mac[6] )
{
    ESP_RETURN_ON_FALSE( handle && mac, ESP_ERR_INVALID_ARG, TAG, "NULL argument" );

    if ( find_entry( handle, mac ) ) {
        return ESP_OK;
    }

    node_entry_t *entry = alloc_entry( handle );
    ESP_RETURN_ON_FALSE( entry, ESP_ERR_NO_MEM, TAG, "Node list full" );

    memcpy( entry->mac, mac, 6 );
    entry->in_use = true;
    handle->count++;

    lv_obj_t *card = lv_obj_create( handle->container );
    lv_obj_set_size( card, LV_PCT( 100 ), LV_SIZE_CONTENT );
    lv_obj_set_flex_flow( card, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_style_pad_all( card, 8, 0 );
    lv_obj_set_style_pad_row( card, 4, 0 );
    lv_obj_set_style_bg_color( card, lv_color_hex( 0x16213e ), 0 );
    lv_obj_set_style_bg_opa( card, LV_OPA_COVER, 0 );
    lv_obj_set_style_radius( card, 8, 0 );
    lv_obj_set_style_border_color( card, lv_color_hex( 0x0f3460 ), 0 );
    lv_obj_set_style_border_width( card, 1, 0 );
    lv_obj_clear_flag( card, LV_OBJ_FLAG_SCROLLABLE );

    lv_obj_t *title = lv_label_create( card );
    lv_label_set_text_fmt( title, LV_SYMBOL_WIFI " %02X:%02X:%02X",
                           mac[3], mac[4], mac[5] );
    lv_obj_set_style_text_color( title, lv_color_hex( 0xe0e0e0 ), 0 );
    lv_obj_set_style_text_font( title, &lv_font_montserrat_24, 0 );

    lv_obj_t *temp = lv_label_create( card );
    lv_label_set_text( temp, LV_SYMBOL_HOME " --" );
    lv_obj_set_style_text_color( temp, lv_color_hex( 0x4fc3f7 ), 0 );
    lv_obj_set_style_text_font( temp, &lv_font_montserrat_24, 0 );

    lv_obj_t *hum = lv_label_create( card );
    lv_label_set_text( hum, LV_SYMBOL_CHARGE " --" );
    lv_obj_set_style_text_color( hum, lv_color_hex( 0x81c784 ), 0 );
    lv_obj_set_style_text_font( hum, &lv_font_montserrat_24, 0 );

    lv_obj_t *pres = lv_label_create( card );
    lv_label_set_text( pres, LV_SYMBOL_LOOP " --" );
    lv_obj_set_style_text_color( pres, lv_color_hex( 0xffb74d ), 0 );
    lv_obj_set_style_text_font( pres, &lv_font_montserrat_24, 0 );
    lv_obj_add_flag( pres, LV_OBJ_FLAG_HIDDEN );

    entry->card = card;
    entry->lbl_title = title;
    entry->lbl_temp = temp;
    entry->lbl_hum = hum;
    entry->lbl_pres = pres;

    return ESP_OK;
}

esp_err_t float_node_list_remove_node( float_node_list_handle_t handle,
                                        const uint8_t mac[6] )
{
    ESP_RETURN_ON_FALSE( handle && mac, ESP_ERR_INVALID_ARG, TAG, "NULL argument" );

    node_entry_t *entry = find_entry( handle, mac );
    if ( !entry ) {
        return ESP_ERR_NOT_FOUND;
    }

    lv_obj_del( entry->card );
    memset( entry, 0, sizeof( node_entry_t ) );
    handle->count--;

    return ESP_OK;
}

esp_err_t float_node_list_update_sensors( float_node_list_handle_t handle,
                                           const uint8_t mac[6],
                                           const float_datapoint_t *points,
                                           uint8_t count )
{
    ESP_RETURN_ON_FALSE( handle && mac && points, ESP_ERR_INVALID_ARG,
                         TAG, "NULL argument" );

    node_entry_t *entry = find_entry( handle, mac );
    if ( !entry ) {
        return ESP_ERR_NOT_FOUND;
    }

    for ( uint8_t i = 0; i < count; i++ ) {
        switch ( points[i].reading_type ) {
        case FLOAT_SENSOR_TYPE_TEMPERATURE:
            lv_label_set_text_fmt( entry->lbl_temp, LV_SYMBOL_HOME " %.1f°C",
                                   (double)points[i].value );
            break;
        case FLOAT_SENSOR_TYPE_HUMIDITY:
            lv_label_set_text_fmt( entry->lbl_hum, LV_SYMBOL_CHARGE " %.0f%%",
                                   (double)points[i].value );
            break;
        case FLOAT_SENSOR_TYPE_PRESSURE:
            lv_label_set_text_fmt( entry->lbl_pres, LV_SYMBOL_LOOP " %.0f hPa",
                                   (double)points[i].value );
            lv_obj_clear_flag( entry->lbl_pres, LV_OBJ_FLAG_HIDDEN );
            break;
        default:
            break;
        }
    }

    return ESP_OK;
}

esp_err_t float_node_list_set_link_status( float_node_list_handle_t handle,
                                            bool up )
{
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_INVALID_ARG, TAG, "NULL handle" );

    if ( up ) {
        lv_label_set_text( handle->lbl_status, LV_SYMBOL_OK " LINK UP" );
        lv_obj_set_style_text_color( handle->lbl_status,
                                     lv_color_hex( 0x66bb6a ), 0 );
    } else {
        lv_label_set_text( handle->lbl_status, LV_SYMBOL_WARNING " LINK DOWN" );
        lv_obj_set_style_text_color( handle->lbl_status,
                                     lv_color_hex( 0xff4444 ), 0 );
    }

    return ESP_OK;
}
