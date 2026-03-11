# Research: Comprehensive Unit Testing

Builds on [spec.md](spec.md).

## Existing test pattern

Both `float_data` and `float_registry` follow the same structure:
- `component/test/CMakeLists.txt` — registers test sources with `idf_component_register`, `REQUIRES unity <component>`
- `component/test/test_*.c` — uses `#include "unity.h"` and `TEST_CASE("description", "[tag]")` macros
- No test app exists yet — tests are compiled as component test sources discoverable by ESP-IDF's test runner

## Component analysis

### float_sensor (new tests)

- **Vtable pattern**: `float_sensor_t` struct with function pointers (`initialize`, `read_temperature`, `read_humidity`, `read_pressure`)
- **Dispatch functions**: `float_sensor_init`, `float_sensor_read_temperature`, `float_sensor_read_humidity`, `float_sensor_read_pressure` — each checks for NULL, calls vtable if set
- **`float_sensor_read_data`**: Aggregation function — counts non-NULL capabilities, allocates `float_datapoints_t`, calls each reader, fills datapoints array
- **Key edge case**: A sensor with only some capabilities (e.g. temp-only) — `read_data` should only produce datapoints for available readings
- **Dependencies**: `float_data` (for types), `driver` (for I2C types in header — but not used in dispatch logic)
- **Test requires**: `float_sensor`, `float_sensor_mock`, `float_data`

### float_sensor_mock (new tests)

- Factory: `float_sensor_new_mock` — allocates, wires all vtable entries
- All readings use `esp_random()` for variation — values are bounded but not deterministic
- Temperature: 20.0 ± 5.0, Humidity: 50.0 ± 25.0, Pressure: 1013.25 ± 15.0
- Test: verify factory returns valid handle, readings are within expected bounds

### float_now (new tests)

- **`float_now_new_packet`**: Pure allocation + header fill. Takes packet type + num_datapoints, returns allocated packet with correct header fields. No WiFi/ESP-NOW dependency.
- **`float_now_packet_type_to_str`**: Static string lookup, bounds-checked. Uses `float_now_packet_type_str[]` from private header.
- **`FLOAT_NOW_PAYLOAD_SIZE` macro**: Arithmetic on packet size, testable with known struct sizes.
- **Functions NOT testable without WiFi**: `float_now_new`, `float_now_send_*`, `float_now_add_peer`, etc.
- **Test requires**: `float_now`, `float_data` — but `float_now_new_packet` and `packet_type_to_str` don't need WiFi init

### float_data (expand)

- Current tests cover: `calculate_size`, `new` allocation, NULL rejection
- Missing: zero datapoints (`float_datapoints_new(&dp, 0)`), field access after allocation

### float_registry (expand)

- Current tests cover: new/delete, store+retrieve node info
- Rich API surface untested:
  - `forget_node` — removal + compaction (last node swapped into removed slot)
  - `store_datapoints` — creates ringbuffers, stores readings
  - `get_latest_readings` — retrieves most recent from each ring
  - `get_history` — oldest-to-newest iteration through ring
  - Ringbuffer wrap-around — fill beyond `FLOAT_RING_CAPACITY` (32)
  - `get_max_last_24h` / `get_min_last_24h` — time-windowed queries
  - `get_node_count` / `get_all_node_macs` — enumeration
  - Max-nodes limit (10) — should return `ESP_ERR_NO_MEM`
- **NVS dependency**: `float_registry_new` calls `float_registry_load_from_nvs`. Tests already call `nvs_flash_init()` which is fine on target.
- **Event loop dependency**: `store_node_info`, `forget_node`, `store_datapoints` call `esp_event_post`. Tests should call `esp_event_loop_create_default()` in setup or events will silently fail (non-fatal, functions still return ESP_OK).

## CMakeLists.txt pattern

```cmake
idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "."
                    REQUIRES unity <component> [additional deps])
```

## No open questions

All testable surfaces are well-understood. Implementation can proceed.
