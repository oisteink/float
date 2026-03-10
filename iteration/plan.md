# Plan: float_node Firmware

Builds on [spec.md](spec.md) and [research.md](research.md).

## Implementation Order

Sensor components first (dependency order), then the node firmware itself.

### Step 1: Port float_sensor

Copy `zenith_sensor` → `float_components/float_sensor/`, rename all symbols.

**Files to create:**
- `float_components/float_sensor/CMakeLists.txt`
- `float_components/float_sensor/include/float_sensor.h`
- `float_components/float_sensor/float_sensor.c`
- `float_components/float_sensor/idf_component.yml`

**Changes:** `zenith_sensor` → `float_sensor`, `zenith_data` → `float_data` in REQUIRES.

### Step 2: Port float_sensor_mock

Copy `zenith_sensor_mock` → `float_components/float_sensor_mock/`.

**Files to create:**
- `float_components/float_sensor_mock/CMakeLists.txt`
- `float_components/float_sensor_mock/include/float_sensor_mock.h`
- `float_components/float_sensor_mock/float_sensor_mock.c`

**Changes:** `zenith_sensor` → `float_sensor` rename throughout.

### Step 3: Port float_sensor_bmp280

Copy `zenith_sensor_bmp280` → `float_components/float_sensor_bmp280/`.

**Files to create:**
- `float_components/float_sensor_bmp280/CMakeLists.txt`
- `float_components/float_sensor_bmp280/include/float_sensor_bmp280.h`
- `float_components/float_sensor_bmp280/float_sensor_bmp280.c`
- `float_components/float_sensor_bmp280/idf_component.yml`

**Changes:** `zenith_sensor` → `float_sensor` rename. BMP280 register defines and enums unchanged.

### Step 4: Port float_sensor_aht30

Copy `zenith_sensor_aht30` → `float_components/float_sensor_aht30/`.

**Files to create:**
- `float_components/float_sensor_aht30/CMakeLists.txt`
- `float_components/float_sensor_aht30/include/float_sensor_aht30.h`
- `float_components/float_sensor_aht30/float_sensor_aht30.c`

**Changes:** `zenith_sensor` → `float_sensor` rename. AHT30 protocol constants unchanged.

### Step 5: Write float_node firmware

Port `zenith_node` → `float_node/main/`, with the debug sleep mode addition.

**Files to create/modify:**
- `float_node/CMakeLists.txt` — add `EXTRA_COMPONENT_DIRS`
- `float_node/main/CMakeLists.txt` — add REQUIRES with sensor selection logic
- `float_node/main/float_node.h` — pin defines (GPIO8 LED, GPIO19/20 I2C)
- `float_node/main/float_node.c` — full app_main with debug/production modes
- `float_node/main/Kconfig.projbuild` — sensor choice + debug sleep toggle
- `float_node/sdkconfig.defaults` — mock sensor, USB serial JTAG

**Kconfig.projbuild:**
```kconfig
menu "Float Node"

    choice FLOAT_NODE_SENSOR
        prompt "Sensor type"
        default FLOAT_NODE_SENSOR_MOCK

        config FLOAT_NODE_SENSOR_AHT30
            bool "AHT30 (temperature + humidity)"
        config FLOAT_NODE_SENSOR_BMP280
            bool "BMP280 (temperature + pressure)"
        config FLOAT_NODE_SENSOR_MOCK
            bool "Mock (fixed values, no hardware required)"
    endchoice

    config FLOAT_NODE_DEBUG_SLEEP
        bool "Use vTaskDelay loop instead of deep sleep"
        default y
        help
            Keeps USB-JTAG alive for continuous logging during development.

    config FLOAT_NODE_SLEEP_DURATION_MS
        int "Sleep duration between readings (ms)"
        default 5000
        depends on FLOAT_NODE_DEBUG_SLEEP

endmenu
```

**app_main structure:**
```c
void app_main(void)
{
    // Boot delay only in production mode (for reflash window)
#if !CONFIG_FLOAT_NODE_DEBUG_SLEEP
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
#endif

    // Init: I2C (if real sensor), sensor, blink, NVS, float_now
    // Same init sequence as zenith_node

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
    esp_deep_sleep(30 * 1000 * 1000);
#endif
}
```

**sdkconfig.defaults:**
```
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_FLOAT_NODE_SENSOR_MOCK=y
```

### Step 6: Build verification

```bash
cd float_node
source ~/esp/v5.5.3/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
```

### Step 7: Flash and test

Flash to the C6 on `/dev/ttyACM0` (coordinate with user for reconnect timing). Verify:
- Node pairs with core (visible on core's display)
- Mock sensor data appears in core's node list
- Debug loop cycles continuously with log output
- LED blinks during pairing

## File Summary

| Step | Path | Action |
|------|------|--------|
| 1 | `float_components/float_sensor/CMakeLists.txt` | Create |
| 1 | `float_components/float_sensor/include/float_sensor.h` | Create (from zenith) |
| 1 | `float_components/float_sensor/float_sensor.c` | Create (from zenith) |
| 1 | `float_components/float_sensor/idf_component.yml` | Create (from zenith) |
| 2 | `float_components/float_sensor_mock/CMakeLists.txt` | Create |
| 2 | `float_components/float_sensor_mock/include/float_sensor_mock.h` | Create (from zenith) |
| 2 | `float_components/float_sensor_mock/float_sensor_mock.c` | Create (from zenith) |
| 3 | `float_components/float_sensor_bmp280/CMakeLists.txt` | Create |
| 3 | `float_components/float_sensor_bmp280/include/float_sensor_bmp280.h` | Create (from zenith) |
| 3 | `float_components/float_sensor_bmp280/float_sensor_bmp280.c` | Create (from zenith) |
| 3 | `float_components/float_sensor_bmp280/idf_component.yml` | Create (from zenith) |
| 4 | `float_components/float_sensor_aht30/CMakeLists.txt` | Create |
| 4 | `float_components/float_sensor_aht30/include/float_sensor_aht30.h` | Create (from zenith) |
| 4 | `float_components/float_sensor_aht30/float_sensor_aht30.c` | Create (from zenith) |
| 5 | `float_node/CMakeLists.txt` | Modify |
| 5 | `float_node/main/CMakeLists.txt` | Modify |
| 5 | `float_node/main/float_node.h` | Create |
| 5 | `float_node/main/float_node.c` | Rewrite |
| 5 | `float_node/main/Kconfig.projbuild` | Create |
| 5 | `float_node/sdkconfig.defaults` | Create |

## Verification

All acceptance criteria from spec.md are addressed:
- ESP-NOW pairing → `float_now` + `pair_with_core()`
- Sensor reading → `float_sensor` + mock driver
- Data transmission → `float_now_send_data()` + ACK wait
- Display on core → existing `float_core` registry + node list
- Deep sleep → production mode path
- Debug mode → `CONFIG_FLOAT_NODE_DEBUG_SLEEP` with `vTaskDelay` loop
- LED feedback → `float_blink`
