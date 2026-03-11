# Plan: Comprehensive Unit Testing

Builds on [spec.md](spec.md) and [research.md](research.md).

## File changes

### New files

| File | Purpose |
|------|---------|
| `float_components/float_sensor/test/CMakeLists.txt` | Test registration |
| `float_components/float_sensor/test/test_float_sensor.c` | Sensor dispatch tests |
| `float_components/float_sensor_mock/test/CMakeLists.txt` | Test registration |
| `float_components/float_sensor_mock/test/test_float_sensor_mock.c` | Mock factory tests |
| `float_components/float_now/test/CMakeLists.txt` | Test registration |
| `float_components/float_now/test/test_float_now.c` | Packet construction tests |

### Modified files

| File | Change |
|------|--------|
| `float_components/float_data/test/test_float_data.c` | Add edge case tests |
| `float_components/float_registry/test/test_float_registry.c` | Add comprehensive API tests |

## Test inventory

### float_sensor (7 tests)

1. `sensor init with mock succeeds` — init via mock, verify ESP_OK
2. `sensor read_temperature via mock` — read through vtable, verify value in range
3. `sensor read_humidity via mock` — same for humidity
4. `sensor read_pressure via mock` — same for pressure
5. `sensor read_data collects all readings` — verify 3 datapoints with correct types
6. `sensor read_data with partial capabilities` — temp-only sensor, verify 1 datapoint
7. `sensor functions reject NULL` — NULL handle/output checks

### float_sensor_mock (3 tests)

1. `mock factory creates valid handle` — non-NULL, all vtable entries set
2. `mock readings in expected ranges` — 100 samples, all within bounds
3. `mock rejects NULL handle` — NULL output check

### float_now (5 tests)

1. `new_packet creates pairing packet` — correct header type, version, payload_size
2. `new_packet creates data packet` — correct payload_size for N datapoints
3. `new_packet creates ack packet` — correct header
4. `new_packet rejects invalid type` — returns ESP_ERR_INVALID_ARG
5. `FLOAT_NOW_PAYLOAD_SIZE macro` — arithmetic check against known struct sizes

### float_data (2 new tests, 5 total)

4. `datapoints_new with zero count` — should succeed, num_datapoints == 0
5. `datapoints fields accessible after alloc` — write/read datapoint values

### float_registry (11 new tests, 13 total)

3. `registry forget_node removes node` — store, forget, verify count
4. `registry forget_node not found` — returns ESP_ERR_NOT_FOUND
5. `registry store_datapoints creates readings` — store + get_latest
6. `registry get_latest_readings` — store multiple types, verify latest of each
7. `registry get_history returns oldest first` — store sequence, verify order
8. `registry ringbuffer wraps around` — fill > FLOAT_RING_CAPACITY, verify size capped
9. `registry get_max_last_24h` — store known values, verify max
10. `registry get_min_last_24h` — store known values, verify min
11. `registry get_node_count multiple nodes` — add several, verify count
12. `registry get_all_node_macs` — add several, verify MACs returned
13. `registry max nodes limit` — add 11 nodes, verify ESP_ERR_NO_MEM on 11th

## Implementation order

1. New test files (float_sensor, float_sensor_mock, float_now) — independent, can be done in parallel
2. Expand float_data tests
3. Expand float_registry tests
