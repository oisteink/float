# Research: Initial float_core Setup

Builds on [spec.md](spec.md).

## 1. Component Porting Details

### Rename scope

All five components are a straightforward `zenith_` → `float_` rename across filenames, types, macros, and Kconfig prefixes. No structural changes needed beyond the node_list display lock swap.

### zenith_now internals (→ float_now)

- Uses a **file-static singleton** `s_instance` because ESP-NOW callbacks don't support user data. This is internal — not visible to float_core's app layer.
- WiFi setup is self-contained inside `zenith_now_new()`:
  ```
  esp_netif_init() → esp_event_loop_create_default() → esp_wifi_init()
  → esp_wifi_set_mode(STA) → esp_wifi_start() → esp_wifi_set_channel(1)
  ```
- Internal architecture: ESP-NOW ISR callbacks → FreeRTOS queue → event handler task → user rx_cb/tx_cb.
- Handle struct: config, event_queue, event_group (ACK signaling), task_handle.

### zenith_registry events

Registry defines `ZENITH_REGISTRY_EVENTS` event base and posts events internally when nodes are added/removed/updated. In zenith_hub, these events are **not subscribed to** — hub_rx_callback calls registry functions directly.

For float_core, we **will** subscribe to these events in app_main to update the LVGL UI. This is the pattern from CLAUDE.md:

```
registry_event_handler (subscribed in app_main)
  → msp3520_lvgl_lock
  → float_node_list_add_node / update_sensors
  → msp3520_lvgl_unlock
```

This is cleaner than zenith's approach and decouples the UI from the ESP-NOW callback.

### zenith_node_list display lock swap

Only change beyond rename:
- `zenith_display_lock(h)` → `msp3520_lvgl_lock(h, 0)` (adds timeout param, returns bool)
- `zenith_display_unlock(h)` → `msp3520_lvgl_unlock(h)`
- `zenith_display_get_lvgl_display(h)` → `msp3520_get_display(h)`

The node_list component itself should **not** take the LVGL lock. Callers (the event handler in app_main) should hold the lock around node_list calls. This matches the zenith_ui pattern and keeps the component lock-agnostic.

### zenith_blink

Uses `led_indicator` component with WS2812 (NeoPixel) via RMT driver. GPIO pin configured via Kconfig. No display or registry dependency — standalone.

### zenith_data

Pure data structures (sensor types, datapoints). No dependencies. Trivial rename.

## 2. Build System Wiring

### CMakeLists.txt pattern from zenith_hub

**Root CMakeLists.txt** (float_core/CMakeLists.txt):
```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "../float_components/"
    "/home/ok/src/esp-msp3520/components/"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(float_core)
```

**Main component** (float_core/main/CMakeLists.txt):
```cmake
idf_component_register(
    SRCS "float_core.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_event float_now float_registry float_blink float_node_list console msp3520
)
```

### sdkconfig.defaults

From zenith_hub, relevant for ESP32-S3:
```
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
```

## 3. Console REPL — Extensible Command Registration

### Two patterns available

**Pattern A (REPL task)**: `esp_console_new_repl_uart/usb()` → spawns background REPL task. Simpler.

**Pattern B (manual loop)**: Application owns the loop with `linenoise()` + `esp_console_run()`. More control.

### Recommendation: Pattern A (REPL task)

Simpler for our use case. Key points:

- `esp_console_cmd_register()` is **thread-safe** — commands can be registered before or after REPL starts.
- Each component provides a `float_xxx_register_console_commands(handle)` function.
- Use `func_w_context` + `context` fields to pass component handles to command handlers (no globals).

### Registration order in app_main

```c
// 1. Console init + REPL setup (but don't start yet)
esp_console_repl_t *repl = NULL;
esp_console_new_repl_usb_serial_jtag(..., &repl);

// 2. Register built-in help
esp_console_register_help_command();

// 3. Each component registers its commands
msp3520_register_console_commands(display);
// future: float_registry_register_console_commands(registry);
// future: float_now_register_console_commands(now);

// 4. Start REPL
esp_console_start_repl(repl);
```

Commands can also be registered after start — useful if a component initializes later.

## 4. ESP-IDF Testing

### Available frameworks

| Framework | Where it runs | Best for |
|-----------|--------------|----------|
| Unity (component tests) | On target device | Data structures, component logic |
| pytest-embedded | Host orchestrates target | CI/CD automation |
| CMock + Linux target | Host machine | Pure logic without HW deps |

### Recommended approach for FLOAT

**Start with Unity component tests on-target.** Lowest setup cost, highest value for a small project.

#### Directory structure

```
float_components/
├── float_data/
│   ├── include/float_data.h
│   ├── float_data.c
│   ├── CMakeLists.txt
│   └── test/
│       ├── CMakeLists.txt      # REQUIRES unity float_data
│       └── test_float_data.c
├── float_registry/
│   └── test/
│       ├── CMakeLists.txt
│       └── test_float_registry.c
```

#### Test file format

```c
#include "unity.h"
#include "float_data.h"

TEST_CASE("datapoints allocation", "[data]")
{
    float_datapoints_t *dp = NULL;
    esp_err_t ret = float_datapoints_new(3, &dp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(dp);
    // ...cleanup
}
```

#### Running tests

```bash
# From esp-idf's unit-test-app directory
cd ~/esp/v5.5.3/esp-idf/tools/unit-test-app
idf.py -T float_data -T float_registry build flash monitor
# Interactive menu lets you pick tests by name, tag, or run all
```

#### What to test in this iteration

- `float_data`: datapoint allocation, size calculation — pure data, easy to test
- `float_registry`: node storage, retrieval, ring buffer behavior — testable on device
- `float_now`, `float_blink`, `float_node_list`: hardware-dependent, skip unit tests for now

### Scaffolding for this iteration

Add `test/` directories with CMakeLists.txt to `float_data` and `float_registry`. Include one or two basic test cases each to prove the pattern works. This establishes the convention for future components.

## 5. Key Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| `s_instance` singleton in float_now | Accept it — ESP-NOW API limitation. Document it. |
| LVGL thread safety with event handler | Event handler acquires lock before calling node_list. Node_list itself stays lock-agnostic. |
| msp3520 path hardcoded in CMakeLists | Use relative path `../../esp-msp3520/components/` or environment variable. Keep it simple for now. |
| NVS namespace collision zenith↔float | Rename NVS namespace in float_registry from `"zenith_registry"` to `"float_registry"`. |
