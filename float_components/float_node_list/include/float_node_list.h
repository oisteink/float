#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
#include "float_data.h"

typedef struct float_node_list_s *float_node_list_handle_t;

esp_err_t float_node_list_new( lv_obj_t *parent,
                                float_node_list_handle_t *out_handle );

esp_err_t float_node_list_delete( float_node_list_handle_t handle );

esp_err_t float_node_list_add_node( float_node_list_handle_t handle,
                                     const uint8_t mac[6] );

esp_err_t float_node_list_remove_node( float_node_list_handle_t handle,
                                        const uint8_t mac[6] );

esp_err_t float_node_list_update_sensors( float_node_list_handle_t handle,
                                           const uint8_t mac[6],
                                           const float_datapoint_t *points,
                                           uint8_t count );

esp_err_t float_node_list_set_link_status( float_node_list_handle_t handle,
                                            bool up );
