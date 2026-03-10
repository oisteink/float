# FLOAT — Fjord-Linked Observation of Aquatic Temperature

You *NEVER* make changes unless approved. Ask for permission.

## Project

ESP-IDF project. A buoy-mounted ESP32-C6 sensor node measures water temperature at 1m depth and transmits via ESP-NOW to an ESP32-S3 base station, which shows data on an ILI9488 touchscreen (via LVGL) and bridges to Home Assistant over Zigbee.

**Hardware:**

| Firmware | Board | Role |
|----------|-------|------|
| `float_core` | ESP32-S3-DevKitC-1 N32R16V | Base station: display + hub + Zigbee host |
| `float_zigbee` | XIAO ESP32-C6 | Zigbee NCP (UART to core) |
| `float_node` | ESP32-C6 Super Mini | Buoy sensor node, battery/solar, ESP-NOW TX |

**Architecture:**
```
Node (buoy) → ESP-NOW → Core (S3) → UART → NCP (C6, Zigbee) → Home Assistant
                            ↓
                         ILI9488 + XPT2046 (via msp3520 component)
                            ↓
                         LVGL UI
```

## Environment

- **Framework**: ESP-IDF v5.5.3
- **Setup**: `source ~/esp/v5.5.3/esp-idf/export.sh`

## Build Commands

```bash
# From float_core/ or float_node/
idf.py set-target <chip>
idf.py build
idf.py flash monitor
idf.py build flash monitor   # combined

# Generate compile_commands.json for clangd (run once after set-target)
idf.py reconfigure
ln -sf build/compile_commands.json compile_commands.json
```

## Code Conventions

Follow ESP-IDF patterns throughout. When in doubt, read how esp-idf itself does it.

- **Error handling**: `esp_err_t` returns everywhere; use `ESP_RETURN_ON_ERROR` / `ESP_GOTO_ON_ERROR`
- **APIs**: Handle-based, never global singletons — pattern: `float_xxx_handle_t`
- **Events**: `esp_event_post()` + `esp_event_handler_register()` for inter-component communication
- **Config**: Kconfig (`Kconfig.projbuild`) for compile-time options, not `#ifdef`
- **Structs**: Opaque handles in `include/`, full struct definition in implementation file or `xxx_private.h`
- **Factory functions**: `float_xxx_new(config, &handle)` / `float_xxx_delete(handle)`
- **No unnecessary comments**: names should be self-documenting
- **No global state in components**: all state owned by handle structs

## Development Process

Work follows a 4-stage gated process. Each stage requires explicit user approval before moving to the next. All artifacts live in the `iteration/` directory and each stage builds on the previous. Once the iteration as a whole is approved these artifacts are archived in their own folder in `iteration/history/`.

### Stage 1: Spec

Collaborate with the user to define what we're building. Capture requirements, constraints, and acceptance criteria.

**Output**: `iteration/spec.md`

### Stage 2: Research

Deep-dive into the spec. Investigate APIs, hardware details, existing patterns, and potential pitfalls.

**Output**: `iteration/research.md`

### Stage 3: Plan

Design the implementation based on spec + research. Include file changes, architecture decisions, and tests. Show how the plan satisfies the spec.

**Output**: `iteration/plan.md`

### Stage 4: Implementation

Execute the plan. Write code, run tests, verify nothing is broken.

### Rules

- Never skip a stage or start the next without approval.
- Keep stage documents concise and focused.
- Each stage document references the previous ones -- don't repeat, build on.

## Git

- Branches: `feature/short-description`
- Commits: imperative mood, explain the *why* not the *what*
- Never push to master without a PR

## Origin: Rewrite from Zenith

FLOAT is a rewrite of the `zenith` project (`~/src/zenith`). The goal is to consolidate what was previously split across `zenith_hub` (ESP-NOW hub) and `zenith_ui` (dedicated display MCU) into a single `float_core` firmware on the ESP32-S3, eliminating the UART bridge between them.

### What comes from zenith

These zenith components are reused (copied and renamed `float_*`):

| Zenith component | Purpose | Changes needed |
|------------------|---------|----------------|
| `zenith_now` | ESP-NOW reception | Rename to `float_now` |
| `zenith_registry` | Node storage + events | Rename to `float_registry` |
| `zenith_node_list` | LVGL UI for node list | Rename, swap lock calls to msp3520 |
| `zenith_blink` | LED feedback | Rename to `float_blink` |
| `zenith_data` | Shared data structures | Rename to `float_data` |

### What comes from esp-msp3520

The `msp3520` component (`~/src/esp-msp3520`) provides display + touch + LVGL integration. Used as an ESP-IDF component dependency (not copied).

### What gets removed (was in zenith, not needed here)

- `zenith_uart_bridge`, `zenith_uart_ui`, `zenith_uart_codec` — the UART bridge layer between hub and display MCUs
- `zenith_display` — replaced by `msp3520`
- `esp_lcd_ili9488` — bundled inside msp3520

### API mapping: zenith_display → msp3520

| zenith_display | msp3520 | Notes |
|---|---|---|
| `zenith_display_new(config, &handle)` | `msp3520_create(config, &handle)` | Config via Kconfig |
| `zenith_display_lock(handle)` | `msp3520_lvgl_lock(handle, 0)` | Extra timeout param |
| `zenith_display_unlock(handle)` | `msp3520_lvgl_unlock(handle)` | Same |
| `zenith_display_set_brightness(h, pct, fade)` | `msp3520_set_backlight(h, pct)` | No fade |
| `zenith_display_get_lvgl_display(h)` | `msp3520_get_display(h)` | Same |
| N/A | `msp3520_register_console_commands(h)` | Bonus: CLI |
| N/A | `msp3520_start_calibration(h)` | Bonus: touch calibration |

### Data flow in float_core

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

### Target repo structure

```
float/
├── float_core/          # ESP32-S3: hub + display + zigbee host
├── float_node/          # ESP32-C6: buoy sensor node
├── float_zigbee/        # XIAO ESP32-C6: Zigbee NCP (unchanged from zenith)
├── float_components/    # Shared components (float_now, float_registry, etc.)
├── docs/                # Hardware docs, references
└── iteration/           # Workflow artifacts
```

### Acceptance criteria for initial setup

- Core receives ESP-NOW pairing and data from nodes
- Node data appears on the display via `float_node_list` UI
- Touch input works (calibration via CLI)
- Console REPL with touch/display commands
- Builds cleanly with `idf.py build`

## References

- Display/touch component: `esp-msp3520` repo at `~/src/esp-msp3520`
- Zenith (predecessor project, for reference): `~/src/zenith`
