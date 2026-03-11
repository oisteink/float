# Spec: Comprehensive Unit Testing

## Goal

Add Unity-based target tests to all components with testable logic. Expand existing test coverage for `float_data` and `float_registry`.

## Scope

### New tests

| Component | Testable surface |
|-----------|-----------------|
| `float_sensor` | vtable dispatch via mock, null guards, partial-capability sensors, `read_data` aggregation |
| `float_sensor_mock` | Factory function, returned values in expected ranges |
| `float_now` | Packet construction (`new_packet`), `packet_type_to_str`, `FLOAT_NOW_PAYLOAD_SIZE` macro |

### Expanded tests

| Component | Current | What to add |
|-----------|---------|-------------|
| `float_data` | 3 tests (size calc, alloc, null guard) | Zero-datapoint edge case, datapoint field access |
| `float_registry` | 2 tests (new/delete, store/retrieve) | forget_node, store_datapoints, get_latest_readings, get_history, ringbuffer wrap-around, max/min 24h, node enumeration, max-nodes limit |

### Skipped (minimal testable logic)

- **float_blink** — pure delegation to `led_indicator`, no logic beyond null guards
- **float_node_list** — LVGL UI widget, requires display init, low ROI for unit tests
- **float_sensor_aht30 / float_sensor_bmp280** — I2C hardware drivers, untestable without hardware
- **float_core / float_node / float_zigbee** — app-level init sequences, integration scope

## Acceptance criteria

- All new tests follow existing pattern: `component/test/CMakeLists.txt` + `test_component.c`
- Tests use Unity (`TEST_CASE` macros with tag groups)
- Each test is self-contained (setup/teardown within the test case)
- Tests build as part of ESP-IDF's component test framework
