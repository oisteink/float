# Spec: float_node Firmware

## Goal

Port the `zenith_node` firmware to `float_node` on the ESP32-C6 Super Mini. The node wakes from deep sleep, reads sensors, sends data to `float_core` via ESP-NOW, and goes back to sleep.

## What We're Building

A working `float_node` firmware that:

1. Pairs with `float_core` via ESP-NOW broadcast (with retry + deep sleep fallback)
2. Reads sensor data via the `float_sensor` interface (mock, BMP280, or AHT30)
3. Sends data to the paired core via ESP-NOW
4. Enters deep sleep between readings
5. Persists paired core MAC across deep sleep via RTC memory
6. Forgets peer after 5 consecutive failed sends (forces re-pairing)
7. Provides LED feedback during pairing

## Components to Port

Copy from `zenith_components/` into `float_components/`, renaming `zenith_` → `float_`:

| Component | Purpose | Port complexity |
|-----------|---------|-----------------|
| `zenith_sensor` → `float_sensor` | Sensor interface (vtable pattern) | Trivial rename |
| `zenith_sensor_mock` → `float_sensor_mock` | Mock sensor for testing without hardware | Trivial rename |
| `zenith_sensor_bmp280` → `float_sensor_bmp280` | BMP280 temp + pressure sensor | Trivial rename |
| `zenith_sensor_aht30` → `float_sensor_aht30` | AHT30 temp + humidity sensor | Trivial rename |

Already ported (reused from float_core iteration): `float_data`, `float_now`, `float_blink`.

## Data Flow

```
deep sleep → wake
  → init sensor (I2C + driver)
  → init blink, NVS, float_now
  → paired? → no → pair_with_core() (broadcast, retry 5x, deep sleep on fail)
  → send_data() (read sensor, ESP-NOW send, wait for ACK)
  → deep sleep (30s debug / configurable)
```

## Hardware

- **Board**: ESP32-C6 Super Mini
- **LED**: GPIO 8 (onboard)
- **I2C**: SDA=GPIO 19, SCL=GPIO 20 (for real sensors)
- **Sensor selection**: Kconfig choice (mock/BMP280/AHT30)

## Key Behaviors (from zenith_node)

- **Boot delay**: 30s on fresh boot (non-timer wakeup) for reflash window, 10s on timer wakeup
- **Pairing**: broadcast pairing packet, wait 5s for ACK, retry 5x then deep sleep 5min
- **Data send**: read sensor, send to paired core, wait 2s for ACK
- **Failed sends**: counter in RTC memory, 5 failures → forget peer → re-pair next wake
- **RX callback**: only handles ACK packets (pairing ACK stores core MAC, data ACK clears fail counter)

## Acceptance Criteria

- [ ] `idf.py build` succeeds for `float_node` targeting ESP32-C6
- [ ] Node pairs with `float_core` via ESP-NOW (visible on core's display)
- [ ] Node sends mock sensor data that appears on core's node list UI
- [ ] Node enters deep sleep after sending data
- [ ] Node re-pairs after forgetting peer (5 failed sends or fresh boot)
- [ ] LED blinks during pairing attempts

## Debug Mode

Deep sleep resets the CPU, which drops the USB-JTAG connection. `idf.py monitor` is slow to reconnect and misses early boot logs, forcing large startup delays just to see output.

A Kconfig toggle (e.g. `CONFIG_FLOAT_NODE_DEBUG_SLEEP`) should replace `esp_deep_sleep()` with a `vTaskDelay` loop that repeats the read-send cycle without resetting. No reconnect issues, full log visibility, no artificial delays needed.

Requirements:
- Kconfig toggle, default enabled for development
- When enabled: wrap the pair-read-send logic in a loop with `vTaskDelay` instead of deep sleep
- When disabled: original deep sleep behavior (production)
- Remove the boot delay hacks (30s/10s) when in debug mode — they're only needed because of the reconnect problem

## Out of Scope

- Real sensor testing (BMP280/AHT30) — port the drivers but test with mock only
- Battery/solar power management
- OTA updates
- Configurable sleep duration via Kconfig (keep hardcoded for now)
