#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_err.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "esp_console.h"
#include "esp_event.h"
#include "argtable3/argtable3.h"

#include "msp3520.h"
#include "float_now.h"
#include "float_blink.h"
#include "float_registry.h"
#include "float_node_list.h"
#include "float_data.h"
#include "cmd_system.h"

static const char *TAG = "float-core";

typedef struct {
    msp3520_handle_t display;
    float_node_list_handle_t node_list;
    float_registry_handle_t registry;
    float_now_handle_t now;
    float_blink_handle_t blink;
} app_context_t;

static app_context_t app_ctx = {0};

static esp_err_t initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static void hub_rx_callback(const uint8_t *mac, const float_now_packet_t *packet)
{
    ESP_RETURN_VOID_ON_FALSE(
        mac && packet,
        TAG, "NULL pointer passed to hub_rx_callback"
    );

    switch (packet->header.type) {
        case FLOAT_PACKET_PAIRING:
            ESP_LOGI(TAG, "Received pairing request from mac: " MACSTR, MAC2STR(mac));

            float_node_info_t node_info;
            memcpy(&node_info.mac, mac, sizeof(float_mac_address_t));

            ESP_ERROR_CHECK(
                float_registry_store_node_info(app_ctx.registry, &node_info)
            );

            ESP_ERROR_CHECK(
                float_now_send_ack(app_ctx.now, mac, FLOAT_PACKET_PAIRING)
            );

            ESP_ERROR_CHECK(
                float_blink_start(app_ctx.blink, BLINK_PAIRING_COMPLETE)
            );

            break;

        case FLOAT_PACKET_DATA:
            float_blink_start(app_ctx.blink, BLINK_DATA_RECEIVE);
            ESP_ERROR_CHECK(
                float_now_send_ack(app_ctx.now, mac, packet->header.type)
            );

            float_datapoints_t *data = (float_datapoints_t *)packet->payload;

            ESP_LOGI(TAG, "Received data from mac: " MACSTR, MAC2STR(mac));
            ESP_LOGI(TAG, "number_of_datapoints: %d", data->num_datapoints);
            for (int i = 0; i < data->num_datapoints; ++i) {
                ESP_LOGI(TAG, "datapoint %d: type: %d value: %.2f",
                         i, data->datapoints[i].reading_type, data->datapoints[i].value);
            }

            ESP_ERROR_CHECK(
                float_registry_store_datapoints(app_ctx.registry, mac,
                                                  data->datapoints, data->num_datapoints)
            );

            break;

        default:
            ESP_LOGI(TAG, "Unhandled packet type %d from mac: " MACSTR,
                     packet->header.type, MAC2STR(mac));
            break;
    }
}

static void registry_event_handler(void *handler_arg, esp_event_base_t base,
                                    int32_t id, void *event_data)
{
    app_context_t *ctx = (app_context_t *)handler_arg;
    uint8_t *mac = (uint8_t *)event_data;

    if (!msp3520_lvgl_lock(ctx->display, 100)) {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock for registry event");
        return;
    }

    switch (id) {
        case FLOAT_REGISTRY_EVENT_NODE_ADDED:
            ESP_LOGI(TAG, "Node added: " MACSTR, MAC2STR(mac));
            float_node_list_add_node(ctx->node_list, mac);
            float_node_list_set_link_status(ctx->node_list, true);
            break;

        case FLOAT_REGISTRY_EVENT_READING_UPDATED: {
            float_reading_t readings[FLOAT_SENSOR_TYPE_MAX];
            size_t count = FLOAT_SENSOR_TYPE_MAX;
            if (float_registry_get_latest_readings(ctx->registry, mac, readings, &count) == ESP_OK) {
                float_datapoint_t points[FLOAT_SENSOR_TYPE_MAX];
                for (size_t i = 0; i < count; i++) {
                    points[i].reading_type = i;
                    points[i].value = readings[i].value;
                }
                float_node_list_update_sensors(ctx->node_list, mac, points, count);
            }
            break;
        }

        case FLOAT_REGISTRY_EVENT_NODE_REMOVED:
            ESP_LOGI(TAG, "Node removed: " MACSTR, MAC2STR(mac));
            float_node_list_remove_node(ctx->node_list, mac);
            break;

        case FLOAT_REGISTRY_EVENT_NODE_UPDATED:
            float_node_list_set_link_status(ctx->node_list, true);
            break;

        default:
            break;
    }

    msp3520_lvgl_unlock(ctx->display);
}

static struct {
    struct arg_str *component;
    struct arg_end *end;
} dump_component_args;

typedef enum dump_component_e {
    DUMP_TARGET_REGISTRY = 0,
    DUMP_TARGET_MAX
} dump_component_t;

static const char *s_dump_component_names[] = {
    "registry",
};

static int command_dump_component(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&dump_component_args);
    if (nerrors) {
        arg_print_errors(stderr, dump_component_args.end, argv[0]);
        return 1;
    }

    if (dump_component_args.component->count != 1) {
        printf("Invalid number of arguments\n");
        return 1;
    }

    const char *target_str = dump_component_args.component->sval[0];
    size_t target_len = strlen(target_str);
    dump_component_t target;

    for (target = DUMP_TARGET_REGISTRY; target < DUMP_TARGET_MAX; target++) {
        if (memcmp(target_str, s_dump_component_names[target], target_len) == 0) {
            break;
        }
    }

    switch (target) {
        case DUMP_TARGET_REGISTRY:
            ESP_ERROR_CHECK(float_registry_full_contents_to_log(app_ctx.registry));
            break;
        default:
            if (target == DUMP_TARGET_MAX) {
                printf("Invalid dump target '%s', choose from registry\n", target_str);
                return 1;
            }
            ESP_LOGW(TAG, "Implementation missing for target %s", target_str);
            break;
    }

    return 0;
}

static void register_dump(void)
{
    dump_component_args.component = arg_str1(NULL, NULL, "<component>",
                                              "The component that you want to dump.");
    dump_component_args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "dump",
        .help = "Dumps information and statistics from various components.",
        .hint = NULL,
        .func = &command_dump_component,
        .argtable = &dump_component_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "app_main()");

    // 1. NVS + event loop
    ESP_ERROR_CHECK(initialize_nvs());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 2. Display
    msp3520_config_t display_cfg = MSP3520_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(msp3520_create(&display_cfg, &app_ctx.display));

    // 3. Registry
    ESP_ERROR_CHECK(float_registry_new(&app_ctx.registry));
    ESP_ERROR_CHECK(float_registry_full_contents_to_log(app_ctx.registry));

    // 4. Node list UI
    msp3520_lvgl_lock(app_ctx.display, 0);
    lv_obj_t *screen = lv_display_get_screen_active(msp3520_get_display(app_ctx.display));
    ESP_ERROR_CHECK(float_node_list_new(screen, &app_ctx.node_list));

    // Populate UI with nodes already in registry (persisted via NVS)
    size_t node_count = 0;
    if (float_registry_get_node_count(app_ctx.registry, &node_count) == ESP_OK && node_count > 0) {
        float_mac_address_t macs[node_count];
        if (float_registry_get_all_node_macs(app_ctx.registry, macs, &node_count) == ESP_OK) {
            for (size_t i = 0; i < node_count; i++) {
                float_node_list_add_node(app_ctx.node_list, macs[i]);
            }
        }
    }

    msp3520_lvgl_unlock(app_ctx.display);

    // 5. Registry events → UI updates
    ESP_ERROR_CHECK(
        esp_event_handler_register(FLOAT_REGISTRY_EVENTS, ESP_EVENT_ANY_ID,
                                    registry_event_handler, &app_ctx)
    );

    // 6. Blink
    float_blink_config_t blink_config = { .gpio_pin = CONFIG_FLOAT_CORE_BLINK_GPIO };
    ESP_ERROR_CHECK(float_blink_new(&blink_config, &app_ctx.blink));

    // 7. ESP-NOW
    float_now_config_t now_config = {
        .rx_cb = hub_rx_callback,
    };
    ESP_ERROR_CHECK(float_now_new(&now_config, &app_ctx.now));

    // 8. Console REPL
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = CONFIG_IDF_TARGET " (float)>";
    repl_config.max_cmdline_length = 1024;

    esp_console_register_help_command();
    register_system_common();
    register_dump();
    msp3520_register_console_commands(app_ctx.display);

    esp_console_dev_uart_config_t hw_config =
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(
        esp_console_new_repl_uart(&hw_config, &repl_config, &repl)
    );

    ESP_ERROR_CHECK(
        esp_console_start_repl(repl)
    );
}
