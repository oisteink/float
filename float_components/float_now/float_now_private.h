#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "float_now.h"

#define PAIRING_ACK_BIT BIT0
#define DATA_ACK_BIT    BIT1

typedef enum float_now_event_type_e {
    SEND_EVENT = 1,
    RECEIVE_EVENT = 2
} float_now_event_type_t;

typedef struct float_now_send_event_s {
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} float_now_send_event_t;

typedef struct float_now_receive_event_s {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    float_now_packet_t *data_packet;
} float_now_receive_event_t;

typedef struct float_event_s {
    float_now_event_type_t type;
    union {
        float_now_send_event_t send;
        float_now_receive_event_t receive;
    };
} float_now_event_t;

typedef struct float_now_s {
    float_now_config_t config;
    QueueHandle_t event_queue;
    EventGroupHandle_t event_group;
    TaskHandle_t task_handle;
} float_now_t;

static const char *float_now_packet_type_str[] = {
    "Pairing",
    "Data",
    "ACK"
};
