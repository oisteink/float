# Plan: Initial float_core Setup

Builds on [spec.md](spec.md) and [research.md](research.md).

## Implementation Order

Work proceeds bottom-up: data structures first, then services, then UI, then wiring.

### Step 1: Port float_data

Copy `zenith_data` → `float_components/float_data/`, rename all symbols.

**Files to create:**
- `float_components/float_data/CMakeLists.txt`
- `float_components/float_data/include/float_data.h`
- `float_components/float_data/float_data.c`

**Changes:** Pure `zenith_` → `float_` rename across types, macros, functions.

**Test scaffold:**
- `float_components/float_data/test/CMakeLists.txt`
- `float_components/float_data/test/test_float_data.c` — test datapoint allocation and size calculation.

### Step 2: Port float_now

Copy `zenith_now` → `float_components/float_now/`, rename all symbols.

**Files to create:**
- `float_components/float_now/CMakeLists.txt`
- `float_components/float_now/include/float_now.h`
- `float_components/float_now/float_now_private.h`
- `float_components/float_now/float_now.c`

**Changes:**
- `zenith_` → `float_` rename
- `ZENITH_WIFI_CHANNEL` → `FLOAT_WIFI_CHANNEL`
- NVS: no NVS usage in this component (confirm)
- Keep `s_instance` pattern as-is (ESP-NOW callback limitation)

### Step 3: Port float_registry

Copy `zenith_registry` → `float_components/float_registry/`, rename all symbols.

**Files to create:**
- `float_components/float_registry/CMakeLists.txt`
- `float_components/float_registry/include/float_registry.h`
- `float_components/float_registry/float_registry.c`

**Changes:**
- `zenith_` → `float_` rename
- NVS namespace: `"zenith_registry"` → `"float_registry"`
- Event base: `ZENITH_REGISTRY_EVENTS` → `FLOAT_REGISTRY_EVENTS`

**Test scaffold:**
- `float_components/float_registry/test/CMakeLists.txt`
- `float_components/float_registry/test/test_float_registry.c` — test node storage and retrieval.

### Step 4: Port float_blink

Copy `zenith_blink` → `float_components/float_blink/`, rename all symbols.

**Files to create:**
- `float_components/float_blink/CMakeLists.txt`
- `float_components/float_blink/include/float_blink.h`
- `float_components/float_blink/include/float_blink_private.h`
- `float_components/float_blink/float_blink.c`

**Changes:**
- `zenith_` → `float_` rename
- Kconfig prefix: `ZENITH_HUB_BLINK_GPIO` → `FLOAT_CORE_BLINK_GPIO`

### Step 5: Port float_node_list

Copy `zenith_node_list` → `float_components/float_node_list/`, rename all symbols.

**Files to create:**
- `float_components/float_node_list/CMakeLists.txt`
- `float_components/float_node_list/include/float_node_list.h`
- `float_components/float_node_list/float_node_list.c`
- `float_components/float_node_list/idf_component.yml` (if needed for lvgl dependency)

**Changes:**
- `zenith_` → `float_` rename
- Remove any `zenith_display` lock/unlock calls from the component itself — callers hold the lock
- Dependency: `zenith_data` → `float_data`, `lvgl` stays

### Step 6: Wire float_core app_main

Write the main application that ties everything together.

**Files to modify/create:**
- `float_core/CMakeLists.txt` — add `EXTRA_COMPONENT_DIRS`
- `float_core/main/CMakeLists.txt` — add REQUIRES for all components
- `float_core/main/float_core.c` — full app_main implementation
- `float_core/main/Kconfig.projbuild` — blink GPIO config
- `float_core/sdkconfig.defaults` — target-specific defaults

**app_main flow:**
```c
void app_main(void)
{
    // 1. NVS
    nvs_flash_init();  // with erase-on-error recovery

    // 2. Display
    msp3520_config_t display_cfg = MSP3520_CONFIG_DEFAULT();
    msp3520_handle_t display;
    msp3520_create(&display_cfg, &display);

    // 3. Registry
    float_registry_handle_t registry;
    float_registry_new(&registry);

    // 4. Node list UI (under LVGL lock)
    msp3520_lvgl_lock(display, 0);
    lv_obj_t *screen = lv_display_get_screen_active(msp3520_get_display(display));
    float_node_list_handle_t node_list;
    float_node_list_new(screen, &node_list);
    msp3520_lvgl_unlock(display);

    // 5. Subscribe to registry events → update UI
    //    Event handler takes display + node_list via context struct
    esp_event_handler_register(FLOAT_REGISTRY_EVENTS, ESP_EVENT_ANY_ID,
                               registry_event_handler, &app_ctx);

    // 6. Blink
    float_blink_config_t blink_cfg = { .gpio_pin = CONFIG_FLOAT_CORE_BLINK_GPIO };
    float_blink_handle_t blink;
    float_blink_new(&blink_cfg, &blink);

    // 7. ESP-NOW
    float_now_config_t now_cfg = { .rx_cb = hub_rx_callback };
    float_now_handle_t now;
    float_now_new(&now_cfg, &now);

    // 8. Console REPL
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_usb_serial_jtag_config_t hw_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
    esp_console_register_help_command();
    msp3520_register_console_commands(display);
    esp_console_start_repl(repl);
}
```

**hub_rx_callback** — same logic as zenith_hub:
- PAIRING → `float_registry_store_node_info()` + ACK + blink
- DATA → `float_registry_store_datapoints()` + ACK + blink

**registry_event_handler** — new for float (decoupled from callback):
- NODE_ADDED → lock, `float_node_list_add_node()`, unlock
- READING_UPDATED → lock, `float_node_list_update_sensors()`, unlock
- NODE_REMOVED → lock, `float_node_list_remove_node()`, unlock

**Context struct** for event handler:
```c
typedef struct {
    msp3520_handle_t display;
    float_node_list_handle_t node_list;
    float_registry_handle_t registry;
} app_context_t;
```

### Step 7: sdkconfig.defaults

```
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
```

### Step 8: Build verification

```bash
cd float_core
source ~/esp/v5.5.3/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

## File Summary

| Step | Path | Action |
|------|------|--------|
| 1 | `float_components/float_data/CMakeLists.txt` | Create |
| 1 | `float_components/float_data/include/float_data.h` | Create (from zenith) |
| 1 | `float_components/float_data/float_data.c` | Create (from zenith) |
| 1 | `float_components/float_data/test/CMakeLists.txt` | Create |
| 1 | `float_components/float_data/test/test_float_data.c` | Create |
| 2 | `float_components/float_now/CMakeLists.txt` | Create |
| 2 | `float_components/float_now/include/float_now.h` | Create (from zenith) |
| 2 | `float_components/float_now/float_now_private.h` | Create (from zenith) |
| 2 | `float_components/float_now/float_now.c` | Create (from zenith) |
| 3 | `float_components/float_registry/CMakeLists.txt` | Create |
| 3 | `float_components/float_registry/include/float_registry.h` | Create (from zenith) |
| 3 | `float_components/float_registry/float_registry.c` | Create (from zenith) |
| 3 | `float_components/float_registry/test/CMakeLists.txt` | Create |
| 3 | `float_components/float_registry/test/test_float_registry.c` | Create |
| 4 | `float_components/float_blink/CMakeLists.txt` | Create |
| 4 | `float_components/float_blink/include/float_blink.h` | Create (from zenith) |
| 4 | `float_components/float_blink/include/float_blink_private.h` | Create (from zenith) |
| 4 | `float_components/float_blink/float_blink.c` | Create (from zenith) |
| 5 | `float_components/float_node_list/CMakeLists.txt` | Create |
| 5 | `float_components/float_node_list/include/float_node_list.h` | Create (from zenith) |
| 5 | `float_components/float_node_list/float_node_list.c` | Create (from zenith) |
| 6 | `float_core/CMakeLists.txt` | Modify (add EXTRA_COMPONENT_DIRS) |
| 6 | `float_core/main/CMakeLists.txt` | Modify (add REQUIRES) |
| 6 | `float_core/main/float_core.c` | Rewrite (full app_main) |
| 6 | `float_core/main/Kconfig.projbuild` | Create |
| 7 | `float_core/sdkconfig.defaults` | Create |

## Verification

1. `idf.py build` succeeds for float_core targeting ESP32-S3
2. All acceptance criteria from spec.md are addressed:
   - ESP-NOW reception → float_now + hub_rx_callback
   - Registry storage → float_registry
   - Display UI → float_node_list + msp3520
   - Touch/console → msp3520_register_console_commands + REPL
   - LED feedback → float_blink
   - NVS persistence → float_registry with `"float_registry"` namespace
