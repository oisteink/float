# Research: float_node Firmware

Builds on [spec.md](spec.md).

## Sensor Components to Port

All four are trivial `zenith_` → `float_` renames (~522 lines total). No logic changes needed.

### float_sensor (base abstraction, ~117 lines)

Vtable-based interface — function pointers for `initialize`, `read_temperature`, `read_humidity`, `read_pressure`. The `read_data()` function aggregates whichever reads succeed into a `float_datapoints_t`.

- **Depends on**: `driver`, `float_data`
- **Has**: `idf_component.yml` (IDF ≥5.5.0)

### float_sensor_mock (test fixture, ~33 lines)

Returns fixed values: 20.0°C, 50.0%, 101325 Pa. No I2C, no hardware.

- **Depends on**: `float_sensor`
- **No** `idf_component.yml`

### float_sensor_bmp280 (temp + pressure, ~222 lines)

Bosch BMP280 I2C driver with forced-mode read, calibration coefficient loading, and Bosch compensation algorithm. Has a 1-second data cache to avoid redundant I2C reads.

- **Depends on**: `driver`, `float_sensor`, PRIV_REQUIRES `esp_timer`
- **Has**: `idf_component.yml` (IDF ≥5.5.0)
- BMP280 register defines and enums stay as-is (hardware constants)

### float_sensor_aht30 (temp + humidity, ~150 lines)

AHT30 I2C driver with 80ms measurement delay and busy-bit retry (up to 5x). Also has a 1-second cache.

- **Depends on**: `driver`, `float_sensor`, PRIV_REQUIRES `esp_timer`
- **No** `idf_component.yml`

## Debug Sleep Mode

### RTC_DATA_ATTR in debug mode

`RTC_DATA_ATTR` variables (`paired_core`, `failed_sends`) persist across deep sleep resets via RTC memory. In debug mode (no deep sleep), they're just regular statics in RAM — persist naturally across loop iterations. No special handling needed; same declarations work in both modes.

### Loop structure

`app_main()` can contain the loop directly — float_node has no REPL or background services that need `app_main` to return. Pattern:

```c
#if CONFIG_FLOAT_NODE_DEBUG_SLEEP
    while (1) {
        if (!saved_peer())
            pair_with_core();
        send_data(sensor);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_FLOAT_NODE_SLEEP_DURATION_MS));
    }
#else
    if (!saved_peer())
        pair_with_core();
    send_data(sensor);
    esp_deep_sleep(CONFIG_FLOAT_NODE_SLEEP_DURATION_MS * 1000);
#endif
```

### Boot delay removal

The 30s/10s boot delays in zenith_node exist solely because `idf.py monitor` loses connection on deep sleep reset. In debug mode there's no reset, so no delays needed. In production mode we can keep a shorter delay for reflash convenience.

### float_now is safe for repeated calls

Verified from source:
- `float_now_add_peer()` is idempotent — checks `esp_now_is_peer_exist()` first
- Send functions allocate/free per call, no state leaks
- ACK event bits are cleared on wait (`pdTRUE` flag)
- Background event task runs continuously
- No teardown/reinit needed between iterations

### Kconfig pattern

```kconfig
config FLOAT_NODE_DEBUG_SLEEP
    bool "Debug mode: replace deep sleep with vTaskDelay loop"
    default y
    help
        Keeps USB-JTAG connection alive for continuous logging.

config FLOAT_NODE_SLEEP_DURATION_MS
    int "Sleep duration between readings (ms)"
    default 5000
    depends on FLOAT_NODE_DEBUG_SLEEP
```

For production deep sleep, can use the same Kconfig value (converted to µs) or a separate config. Using a shared `SLEEP_DURATION_MS` with unit conversion keeps it simple.

## Existing Shared Components

Already ported and available in `float_components/`:
- `float_data` — sensor data structures
- `float_now` — ESP-NOW send/receive/ACK
- `float_blink` — LED feedback

These are used by both `float_core` and `float_node`.

## Build Considerations

- Target: ESP32-C6 (`idf.py set-target esp32c6`)
- `EXTRA_COMPONENT_DIRS` must point to `../float_components/`
- `sdkconfig.defaults`: need `CONFIG_FLOAT_NODE_SENSOR_MOCK=y`, USB serial JTAG console
- No LVGL fonts needed (unlike zenith_node which pulled in zenith_ui_core transitively)
