# Plan: Sensor-Typed Data Model & Context-Aware UI

Builds on [spec.md](spec.md) and [research.md](research.md).

## Overview

Six phases, each building on the previous. Each phase produces a compilable + testable result. Core principle: change data model bottom-up, then adapt producers (node) and consumers (core/UI) to match.

---

## Phase 1: New `float_sensor_class` Component

Create the shared sensor class definition that everything else depends on.

### New component: `float_components/float_sensor_class/`

**`include/float_sensor_class.h`** — sensor class enum + metadata struct:

```c
typedef enum float_sensor_class_e {
    FLOAT_SENSOR_CLASS_AIR_TEMPERATURE = 0,
    FLOAT_SENSOR_CLASS_WATER_TEMPERATURE,
    FLOAT_SENSOR_CLASS_AIR_HUMIDITY,
    FLOAT_SENSOR_CLASS_AIR_PRESSURE,
    // Future: WIND_SPEED, RAINFALL, BATTERY_VOLTAGE, ...
    FLOAT_SENSOR_CLASS_MAX
} float_sensor_class_t;

typedef struct float_sensor_class_info_s {
    float_sensor_class_t sensor_class;
    const char *label;          // "Air Temp", "Water Temp", "Humidity", ...
    const char *unit;           // "°C", "%", "hPa", ...
    const char *format;         // "%.1f", "%.0f", ...
    float range_min;
    float range_max;
} float_sensor_class_info_t;

const float_sensor_class_info_t *float_sensor_class_get_info( float_sensor_class_t sensor_class );
```

**`float_sensor_class.c`** — static lookup table:

```c
static const float_sensor_class_info_t s_class_table[] = {
    { FLOAT_SENSOR_CLASS_AIR_TEMPERATURE,   "Air Temp",   "°C",  "%.1f", -40.0f, 60.0f },
    { FLOAT_SENSOR_CLASS_WATER_TEMPERATURE, "Water Temp", "°C",  "%.1f", -2.0f,  40.0f },
    { FLOAT_SENSOR_CLASS_AIR_HUMIDITY,      "Humidity",   "%",   "%.0f", 0.0f,  100.0f },
    { FLOAT_SENSOR_CLASS_AIR_PRESSURE,      "Pressure",   "hPa", "%.0f", 800.0f, 1200.0f },
};
```

**`CMakeLists.txt`** — no dependencies (pure data).

**`test/test_float_sensor_class.c`** — lookup returns correct info for each class, returns NULL for invalid class.

### Modify: `float_data.h`

- Remove `float_sensor_type_t` enum (moves to `float_sensor_class`)
- `float_datapoint_t.reading_type` stays `uint8_t` (wire-compatible) but now holds a `float_sensor_class_t` value
- Remove `FLOAT_SENSOR_TYPE_MAX`
- Add `#include "float_sensor_class.h"`

### Modify: `float_data/CMakeLists.txt`

- Add `REQUIRES float_sensor_class`

### Modify: tests

- `test_float_data.c` — update enum references from `FLOAT_SENSOR_TYPE_*` to `FLOAT_SENSOR_CLASS_*`

### Build verification

- `float_test` builds and all `[data]` tests pass

---

## Phase 2: Sensor Vtable Refactor

Replace hardcoded `read_temperature/humidity/pressure` function pointers with a channel-based interface.

### Modify: `float_sensor.h`

Replace the vtable:

```c
typedef struct float_sensor_channel_s {
    float_sensor_class_t sensor_class;
    esp_err_t (*read)( float_sensor_handle_t sensor, float_sensor_datatype_t *out );
} float_sensor_channel_t;

#define FLOAT_SENSOR_MAX_CHANNELS 8

struct float_sensor_s {
    esp_err_t (*initialize)( float_sensor_handle_t sensor );
    uint8_t num_channels;
    float_sensor_channel_t channels[FLOAT_SENSOR_MAX_CHANNELS];
};
```

Remove the individual `float_sensor_read_temperature/humidity/pressure` declarations from the public API.

### Modify: `float_sensor.c`

- `float_sensor_read_data()` iterates `sensor->channels[0..num_channels-1]`, calls each `channel.read()`, tags datapoint with `channel.sensor_class`
- Remove `float_sensor_read_temperature()`, `float_sensor_read_humidity()`, `float_sensor_read_pressure()` — they no longer exist

Add new helper:

```c
esp_err_t float_sensor_get_classes( float_sensor_handle_t sensor, float_sensor_class_t *out_classes, uint8_t *out_count );
```

This lets the node query "what sensor classes do I have?" for the pairing payload.

### Modify: `float_sensor.h` / `CMakeLists.txt`

- Add `REQUIRES float_sensor_class` (needs the enum)
- Already depends on `float_data` which now depends on `float_sensor_class`

### Modify: `float_sensor_bmp280.c`

Replace vtable setup in `float_sensor_new_bmp280()`:

```c
// Before:
sensor->base.read_temperature = bmp280_read_temperature;
sensor->base.read_pressure    = bmp280_read_pressure;

// After:
sensor->base.num_channels = 2;
sensor->base.channels[0] = (float_sensor_channel_t){
    .sensor_class = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE,
    .read = bmp280_read_temperature
};
sensor->base.channels[1] = (float_sensor_channel_t){
    .sensor_class = FLOAT_SENSOR_CLASS_AIR_PRESSURE,
    .read = bmp280_read_pressure
};
```

Static read functions stay — only their signatures change from `(float_sensor_handle_t, float_sensor_datatype_t *)` to match the channel read signature (same signature, no change needed).

### Modify: `float_sensor_aht30.c`

Same pattern — 2 channels: `AIR_TEMPERATURE` + `AIR_HUMIDITY`.

### Modify: `float_sensor_mock.c` + Kconfig

Add Kconfig for mock sensor classes:

**`float_node/main/Kconfig.projbuild`** (extend existing):

```kconfig
menu "Mock sensor classes"
    visible if FLOAT_NODE_SENSOR_MOCK
    config FLOAT_MOCK_AIR_TEMPERATURE
        bool "Air temperature"
        default y
    config FLOAT_MOCK_AIR_HUMIDITY
        bool "Air humidity"
        default y
    config FLOAT_MOCK_AIR_PRESSURE
        bool "Air pressure"
        default n
    config FLOAT_MOCK_WATER_TEMPERATURE
        bool "Water temperature"
        default n
endmenu
```

`float_sensor_new_mock()` builds channels based on enabled Kconfig options.

### Modify: `float_sensor_mock.h`

No structural change — `float_sensor_mock_t` already embeds `float_sensor_t base`.

### Modify: tests

- `test_float_sensor.c` — rewrite to use new channel-based API. Test that mock produces correct sensor classes. Test partial capabilities by creating a sensor with only 1 channel. Remove tests for removed `read_temperature/humidity/pressure` functions.
- `test_float_sensor_mock.c` — update to verify channel-based output.

### Build verification

- `float_test` builds and all `[sensor]` tests pass

---

## Phase 3: Wire Protocol — Pairing Payload

Extend the pairing packet so nodes declare their sensor classes on pair.

### Modify: `float_now.h`

Bump protocol version to 1.3:

```c
#define FLOAT_NOW_MINOR_VERSION 3
```

Extend pairing payload:

```c
#define FLOAT_NOW_MAX_SENSOR_CLASSES 8

typedef struct __attribute__((packed)) float_now_payload_pairing_s {
    uint8_t flags;
    uint8_t num_sensor_classes;
    uint8_t sensor_classes[FLOAT_NOW_MAX_SENSOR_CLASSES];
} float_now_payload_pairing_t;
```

### Modify: `float_now.c`

`float_now_send_pairing()` — accept sensor class list and populate the payload. Update signature:

```c
esp_err_t float_now_send_pairing( float_now_handle_t handle, const uint8_t *peer_mac,
                                   const uint8_t *sensor_classes, uint8_t num_classes );
```

Note: the signature uses `uint8_t *` (not `float_sensor_class_t *`) because this is wire format. Callers must convert the enum array to uint8_t before calling.

### Modify: `float_now/test/test_float_now.c`

Update pairing tests for new payload format.

### Build verification

- `float_test` builds and all `[now]` tests pass

---

## Phase 4: Registry — Store Sensor Classes

The registry must persist sensor class info and key ring buffers by sensor class.

### Modify: `float_registry.h`

Extend `float_node_info_t`:

```c
typedef struct float_node_info_s {
    float_mac_address_t mac;
    uint8_t num_sensor_classes;
    uint8_t sensor_classes[FLOAT_MAX_SENSOR_CLASSES];
} float_node_info_t;
```

Note: `FLOAT_MAX_SENSOR_CLASSES` is defined in `float_sensor_class.h` (shared constant), not in this header.

Change ring buffer to use `float_sensor_class_t` instead of `float_sensor_type_t`:

```c
typedef struct float_ringbuffer_s {
    float_sensor_class_t sensor_class;   // was: float_sensor_type_t type
    ...
} float_ringbuffer_t;
```

Update API signatures:
- `float_registry_get_history()` — takes `float_sensor_class_t` instead of `float_sensor_type_t`
- `float_registry_get_max_last_24h()` / `get_min_last_24h()` — same change

Sensor classes are accessible via the existing `float_registry_get_node_info()` which returns the full `float_node_info_t` struct. No separate getter needed.

### Modify: `float_registry.c`

- `_get_ringbuffer()` — match on `sensor_class` instead of `type`
- `store_datapoints()` — remove the `< FLOAT_SENSOR_TYPE_MAX` validation (sensor class range validated by `float_sensor_class_get_info()` returning non-NULL, or just `< FLOAT_SENSOR_CLASS_MAX`)
- `get_latest_readings()` — needs to also return the sensor class for each reading. Change to return a struct that includes the class:

```c
typedef struct float_registry_reading_s {
    float_sensor_class_t sensor_class;
    float_reading_t reading;
} float_registry_reading_t;

esp_err_t float_registry_get_latest_readings( float_registry_handle_t handle,
    const float_mac_address_t mac, float_registry_reading_t *out_readings, size_t *inout_count );
```

This fixes the existing bug in `registry_event_handler` where `points[i].reading_type = i` assumed ring index equals sensor type.

### Modify: NVS persistence

Bump `FLOAT_REGISTRY_VERSION` to 2. The NVS blob now stores the sensor class list per node:

```
header (2 bytes) → [ node: mac(6) + num_classes(1) + classes(8) ] × count
```

`float_registry_load_from_nvs()` — handle version 2 blob. Drop version 1 blobs (clean start is fine).
`float_registry_save_to_nvs()` — write version 2 format.

### Modify: tests

- `test_float_registry.c` — update all enum references. Add test for sensor class storage/retrieval. Update `store_datapoints` tests to use `FLOAT_SENSOR_CLASS_*` values. Test that `get_latest_readings` returns correct sensor classes.

### Build verification

- `float_test` builds and all `[registry]` tests pass

---

## Phase 5: Core — Reception + UI

Wire everything together on the base station.

### Modify: `float_core.c`

**`hub_rx_callback()` — FLOAT_PACKET_PAIRING case:**

Extract sensor classes from pairing payload and populate `float_node_info_t`:

```c
float_now_payload_pairing_t *pairing = (float_now_payload_pairing_t *)packet->payload;
float_node_info_t node_info;
memcpy(&node_info.mac, mac, sizeof(float_mac_address_t));
node_info.num_sensor_classes = pairing->num_sensor_classes;
memcpy(node_info.sensor_classes, pairing->sensor_classes, pairing->num_sensor_classes);
```

**`registry_event_handler()` — READING_UPDATED case:**

Use new `float_registry_reading_t` to get sensor class with each reading:

```c
float_registry_reading_t readings[FLOAT_MAX_SENSOR_CLASSES];
size_t count = FLOAT_MAX_SENSOR_CLASSES;
if (float_registry_get_latest_readings(ctx->registry, mac, readings, &count) == ESP_OK) {
    float_datapoint_t points[FLOAT_MAX_SENSOR_CLASSES];
    for (size_t i = 0; i < count; i++) {
        points[i].reading_type = readings[i].sensor_class;
        points[i].value = readings[i].reading.value;
    }
    float_node_list_update_sensors(ctx->node_list, mac, points, count);
}
```

**`registry_event_handler()` — NODE_ADDED case:**

Pass sensor classes to node_list so it can build the right card:

```c
float_node_info_t info;
float_registry_get_node_info(ctx->registry, mac, &info);
float_node_list_add_node(ctx->node_list, mac, info.sensor_classes, info.num_sensor_classes);
```

**Startup — populate UI from persisted nodes:**

Same change — pass sensor classes when adding persisted nodes at boot.

### Modify: `float_node_list.h`

Update `float_node_list_add_node()` signature:

```c
esp_err_t float_node_list_add_node( float_node_list_handle_t handle,
                                     const uint8_t mac[6],
                                     const uint8_t *sensor_classes,
                                     uint8_t num_sensor_classes );
```

Add dependency on `float_sensor_class` in CMakeLists.txt.

### Modify: `float_node_list.c`

**`node_entry_t`** — replace fixed `lbl_temp/hum/pres` with dynamic array:

```c
typedef struct {
    float_sensor_class_t sensor_class;
    lv_obj_t *label;
} sensor_label_t;

typedef struct {
    uint8_t mac[6];
    lv_obj_t *card;
    lv_obj_t *lbl_title;
    sensor_label_t sensors[FLOAT_MAX_SENSOR_CLASSES];
    uint8_t num_sensors;
    bool in_use;
} node_entry_t;
```

**`float_node_list_add_node()`** — create labels dynamically based on sensor classes. For each class, look up `float_sensor_class_get_info()` to get label text. LVGL-specific styling (icon, color) comes from a local mapping table in this file:

```c
typedef struct {
    float_sensor_class_t sensor_class;
    const char *icon;           // LV_SYMBOL_*
    uint32_t color;             // hex color
} sensor_ui_style_t;

static const sensor_ui_style_t s_ui_styles[] = {
    { FLOAT_SENSOR_CLASS_AIR_TEMPERATURE,   LV_SYMBOL_HOME,   0x4fc3f7 },
    { FLOAT_SENSOR_CLASS_WATER_TEMPERATURE, LV_SYMBOL_DOWNLOAD, 0x0288d1 },
    { FLOAT_SENSOR_CLASS_AIR_HUMIDITY,      LV_SYMBOL_CHARGE, 0x81c784 },
    { FLOAT_SENSOR_CLASS_AIR_PRESSURE,      LV_SYMBOL_LOOP,   0xffb74d },
};
```

For each sensor class, create one label with the icon and "--" placeholder, styled with the matching color.

**`float_node_list_update_sensors()`** — iterate datapoints, find matching `sensor_label_t` by sensor class, format using `float_sensor_class_get_info()->format` and `->unit`.

### Build verification

- `float_core` builds cleanly
- `float_test` builds and all tests pass

---

## Phase 6: Node Firmware + Hardware Test

### Modify: `float_node.c`

- After `init_sensor()`, call `float_sensor_get_classes()` to get the sensor class list as `float_sensor_class_t[]`
- Convert to `uint8_t[]` before passing to pairing (enum is int-sized, wire format is uint8_t)
- Update `pair_with_core()` to accept and forward sensor classes to `float_now_send_pairing()`

### Flash + verify

1. **Node A** (ttyACM0): set Kconfig to `FLOAT_NODE_SENSOR_AHT30`, build + flash
2. **Node B** (ttyACM1): set Kconfig to `FLOAT_NODE_SENSOR_BMP280`, build + flash
3. **Core** (ttyUSB0 or whatever): build + flash
4. Verify:
   - Both nodes pair successfully (monitor logs)
   - Node A card shows "Air Temp" + "Humidity" only
   - Node B card shows "Air Temp" + "Pressure" only
   - Styling differs per sensor class
   - Reboot core → nodes re-pair → cards rebuilt correctly

---

## Files Changed Summary

| File | Phase | Change |
|------|-------|--------|
| `float_components/float_sensor_class/` (new) | 1 | New component: enum, metadata table, `FLOAT_MAX_SENSOR_CLASSES` |
| `float_components/float_data/include/float_data.h` | 1 | Remove old enum, depend on float_sensor_class |
| `float_components/float_data/CMakeLists.txt` | 1 | Add REQUIRES float_sensor_class |
| `float_components/float_sensor/include/float_sensor.h` | 2 | Channel-based vtable, get_classes API |
| `float_components/float_sensor/float_sensor.c` | 2 | New read_data + get_classes, remove individual readers |
| `float_components/float_sensor_bmp280/float_sensor_bmp280.c` | 2 | Channel setup |
| `float_components/float_sensor_aht30/float_sensor_aht30.c` | 2 | Channel setup |
| `float_components/float_sensor_mock/float_sensor_mock.c` | 2 | Kconfig-driven channels with default-all fallback |
| `float_node/main/Kconfig.projbuild` | 2 | Mock sensor class menu |
| `float_components/float_now/include/float_now.h` | 3 | Version bump to 1.3, extended pairing payload |
| `float_components/float_now/float_now.c` | 3 | Updated send_pairing with uint8_t sensor class array |
| `float_components/float_registry/include/float_registry.h` | 4 | Extended node_info, float_registry_reading_t, sensor_class in rings |
| `float_components/float_registry/float_registry.c` | 4 | NVS v2, sensor class storage, ring key change |
| `float_components/float_node_list/include/float_node_list.h` | 5 | Updated add_node signature with sensor classes |
| `float_components/float_node_list/float_node_list.c` | 5 | Dynamic card rendering with per-class styling |
| `float_components/float_node_list/CMakeLists.txt` | 5 | Add REQUIRES float_sensor_class |
| `float_core/main/float_core.c` | 5 | Pairing handler, event handler (bug fix), startup |
| `float_node/main/float_node.c` | 6 | Query + convert sensor classes, pass in pairing |
| `float_test/CMakeLists.txt` | 1 | Add float_sensor_class to TEST_COMPONENTS |
| Tests (6 files) | 1-4 | Updated for new enums + APIs |

## Bugs Fixed

1. `registry_event_handler` in `float_core.c`: `points[i].reading_type = i` assumed ring buffer index equals sensor type. Fixed by using `float_registry_reading_t` which carries the sensor class explicitly.

2. `float_node.c` enum-to-wire conversion: `float_sensor_class_t` is an int-sized enum. Casting the array directly to `uint8_t *` for the pairing payload caused only the first sensor class to transmit correctly (little-endian byte 0 was correct, bytes 1-3 were zero). Fixed by explicitly converting enum values to a `uint8_t[]` before sending.
