# Spec: Initial float_core Setup

## Goal

Port the zenith hub functionality into `float_core` on the ESP32-S3, replacing the UART bridge to a separate display MCU with direct display integration via the `msp3520` component.

## What We're Building

A working `float_core` firmware that:

1. Receives ESP-NOW pairing requests and data packets from sensor nodes
2. Stores node info and sensor readings in a registry
3. Displays node data on an ILI9488 touchscreen via LVGL
4. Provides a console REPL with extensible command registration (msp3520 commands first, ready for more components to add their own)
5. Provides LED feedback for pairing and data events

## Components to Port

Copy from `zenith_components/` into `float_components/`, renaming `zenith_` → `float_`:

| Component | Purpose | Port complexity |
|-----------|---------|-----------------|
| `zenith_data` → `float_data` | Shared data structures (sensor types, datapoints) | Trivial rename |
| `zenith_now` → `float_now` | ESP-NOW reception, pairing, ACKs | Rename, note: uses file-static `s_instance` for ESP-NOW callbacks |
| `zenith_registry` → `float_registry` | Node storage, ring buffers, NVS persistence, events | Rename |
| `zenith_node_list` → `float_node_list` | LVGL UI cards showing node data | Rename + swap `zenith_display_lock/unlock` → `msp3520_lvgl_lock/unlock` |
| `zenith_blink` → `float_blink` | WS2812 LED feedback patterns | Rename |

## External Dependency

`msp3520` component from `~/src/esp-msp3520/components/msp3520` — provides:
- ILI9488 display + XPT2046 touch + LVGL integration
- `msp3520_create()` / `msp3520_destroy()`
- `msp3520_lvgl_lock()` / `msp3520_lvgl_unlock()`
- `msp3520_get_display()` — returns `lv_display_t*`
- `msp3520_set_backlight()`
- `msp3520_register_console_commands()` — touch/display REPL
- `msp3520_start_calibration()` — 3-point touch calibration with NVS persistence

## Data Flow

```
float_now (ESP-NOW RX)
  → hub_rx_callback
    → float_registry_store_node_info / store_datapoints
      → esp_event_post(FLOAT_REGISTRY_EVENTS, ...)

registry_event_handler (subscribed in app_main)
  → msp3520_lvgl_lock
  → float_node_list_add_node / update_sensors / remove_node
  → msp3520_lvgl_unlock
```

## What Gets Removed (vs zenith)

- `zenith_uart_bridge`, `zenith_uart_ui`, `zenith_uart_codec` — no longer needed
- `zenith_display` — replaced by `msp3520`
- `esp_lcd_ili9488` — bundled inside `msp3520`

## app_main Initialization Order

1. NVS flash init
2. `msp3520_create()` — display + touch + LVGL
3. `float_registry_new()` — node storage
4. Create `float_node_list` UI (under LVGL lock)
5. Register registry event handler → updates node list UI
6. `float_blink_new()` — LED feedback
7. `float_now_new()` with `hub_rx_callback` — ESP-NOW receiver
8. Start console REPL
9. Register commands from components (msp3520, and future float_* components)

## Acceptance Criteria

- [ ] `idf.py build` succeeds for `float_core` targeting ESP32-S3
- [ ] Core receives ESP-NOW pairing requests from nodes and stores them in registry
- [ ] Core receives ESP-NOW data packets and stores sensor readings
- [ ] Node data (MAC, temperature, humidity, pressure) appears on ILI9488 display via `float_node_list`
- [ ] Touch input works (calibration available via CLI `touch cal start`)
- [ ] Console REPL available, with msp3520 commands registered (pattern supports adding more component commands later)
- [ ] LED blinks on pairing and data reception
- [ ] Registry persists nodes across reboots (NVS)

## Testing

Research how ESP-IDF handles unit/integration testing. At minimum, scaffold the test infrastructure so future iterations can add tests incrementally. This is a research topic for Stage 2.

## Out of Scope (Backlog Candidates)

Items below are out of scope for this iteration. After completion, we'll review this list to build a prioritized backlog.

- `float_node` firmware
- `float_zigbee` NCP firmware
- Zigbee bridge to Home Assistant
- Any UI beyond the node list (charts, settings, etc.)
- OTA updates
- Component-level unit tests (scaffold only in this iteration)
- Per-component REPL commands beyond msp3520
