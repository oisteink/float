# Research: Sensor-Typed Data Model & Context-Aware UI

Builds on [spec.md](spec.md).

## Current Architecture Summary

### Data Flow

```
Node: sensor_read_data() → datapoints[] → ESP-NOW packet → Core: hub_rx_callback()
  → registry_store_datapoints() → event(READING_UPDATED) → registry_event_handler()
  → node_list_update_sensors() → LVGL labels
```

### What Exists

| Layer | What it does | Key limitation |
|-------|-------------|----------------|
| `float_data.h` | `float_sensor_type_t` enum: TEMPERATURE, HUMIDITY, PRESSURE | No context (water vs air) |
| `float_sensor.h` | Vtable with `read_temperature/humidity/pressure` pointers | Fixed 3 reading types |
| `float_sensor_read_data()` | Checks non-NULL vtable pointers, builds datapoints array | Tags with generic type only |
| `float_now.h` | Pairing packet has 1-byte `flags` payload (unused) | No sensor descriptor exchange |
| `float_registry.h` | `float_node_info_t` stores MAC only; ring buffers keyed by sensor_type | No sensor class awareness |
| `float_node_list.c` | Hardcoded 3 labels per card (temp/hum/pres), same icons+colors for all | No dynamic rendering |

## Key Findings

### ESP-NOW Payload Budget

- Max ESP-NOW frame: **250 bytes**
- Packet header: 4 bytes (type, version, payload_size)
- Available for payload: **246 bytes**
- Current pairing payload: 1 byte (flags=0) → **245 bytes free**
- Current data payload: `1 + 5n` bytes (n = datapoint count)

Plenty of room to include sensor class descriptors in the pairing packet.

### Protocol Versioning

Current version: **1.2** (`(major << 4) | minor`). Packets with mismatched version are dropped. Any format change requires a version bump.

### Registry Persistence

- NVS blob format: 2-byte header + 6 bytes per node (MAC only)
- Ring buffers are **runtime-only** — lost on reboot, rebuilt as data arrives
- Ring buffers auto-created on first datapoint for a given sensor type
- `store_datapoints` validates `reading_type < FLOAT_SENSOR_TYPE_MAX`

To persist sensor descriptors, `float_node_info_t` and the NVS blob must grow.

### Sensor Vtable (In-Scope for Refactor)

The vtable has hardcoded function pointers (`read_temperature`, `read_humidity`, `read_pressure`). `float_sensor_read_data()` checks which are non-NULL and builds the datapoints array accordingly, tagging each with the generic `FLOAT_SENSOR_TYPE_*`.

This is the root of the problem — the abstraction only speaks in generic physical quantities with no context. A translation layer between sensor and wire format would be a seam that breaks when a node has two sensors measuring the same quantity (e.g. water temp + air temp).

The vtable must be refactored so sensor drivers declare their sensor classes directly. Datapoints come out of the sensor layer already tagged with the correct class. This touches:
- `float_sensor.h` — new vtable shape
- `float_sensor.c` — new `read_data` implementation
- `float_sensor_bmp280.c`, `float_sensor_aht30.c`, `float_sensor_mock.c` — all drivers
- `float_data.h` — new sensor class enum replaces `float_sensor_type_t`
- Tests for the above

### UI Component

`float_node_list_add_node()` creates exactly 4 labels per card: title, temp, humidity, pressure. Pressure starts hidden and is shown on first pressure reading. Update function switches on `reading_type` to pick the label to update.

This entire approach must change to: create labels dynamically based on the node's declared sensor classes.

## Technical Constraints

1. **Datapoint `reading_type` is uint8_t** — supports up to 255 sensor classes. Currently 3 used (0, 1, 2).

2. **Ring buffer lookup** uses sensor type as key. Switching to sensor class as key works the same way — it's just a uint8_t match.

3. **LVGL on ESP32-S3** — dynamic widget creation is fine as long as we stay within memory. Each label is ~100-200 bytes of LVGL overhead. With 10 nodes × ~5 sensors = ~50 labels — no issue.

4. **NVS blob size** — current max is `2 + 10×6 = 62 bytes`. Adding e.g. up to 8 sensor classes per node: `2 + 10×(6+1+8) = 152 bytes`. Well within NVS page limits.

5. **Backwards compatibility** — if we bump the protocol version, old nodes can't talk to new core and vice versa. Since we flash both, this is acceptable but worth noting.

## Decisions

### 1. Sensor class enum values

Renumber from 0. Backwards compatibility is handled by the protocol version field — bump the version, all devices get flashed together.

### 2. Presentation table location → new `float_sensor_class` component

The table has two known consumers from the start: `float_node_list` (UI cards) and the upcoming Zigbee bridge (exposes sensors to Home Assistant). Putting it in the UI component would force the Zigbee bridge to depend on an LVGL component or duplicate the table.

A new `float_sensor_class` component owns:
- The sensor class enum (moves out of `float_data`)
- A static lookup table: label, unit string, value format, range per class
- No LVGL dependency — presentation is data (strings, numbers), not widgets

LVGL-specific rendering (icon selection, color) stays in `float_node_list` — it can derive these from the sensor class or maintain its own small mapping. The key metadata (unit, label, range) lives in `float_sensor_class` where any component can use it.

`float_data` keeps the wire-format structs (`float_datapoint_t`, `float_datapoints_t`) but depends on `float_sensor_class` for the enum.

### 3. Node declares classes via pairing payload

Extend the existing pairing payload with `uint8_t num_sensors` + `uint8_t sensor_classes[]`. Pairing is self-introduction — the sensor list belongs there.

Re-pairing (which already works) refreshes the sensor class list. If a node is reflashed with different sensors and re-pairs, the core gets the updated descriptor. The registry updates its stored info for that MAC and the UI rebuilds the card.

No new packet type needed.

### 4. Mock sensor → configurable via Kconfig

One mock component with Kconfig multi-select for which sensor classes it declares. This mirrors how real sensors work (compile-time selection via Kconfig) and lets you flash two mock nodes with different personalities to test the "different nodes show different cards" scenario.

```
menu "Mock sensor classes"
    visible if FLOAT_NODE_SENSOR_MOCK
    config FLOAT_MOCK_AIR_TEMPERATURE
        bool "Air temperature"
        default y
    config FLOAT_MOCK_AIR_HUMIDITY
        bool "Air humidity"
        default y
    ...
endmenu
```
