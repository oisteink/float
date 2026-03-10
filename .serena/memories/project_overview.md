# FLOAT — Fjord-Linked Observation of Aquatic Temperature

## Purpose
ESP-IDF project for a buoy-mounted water temperature monitoring system. An ESP32-C6 sensor node measures water temperature at 1m depth and transmits via ESP-NOW to an ESP32-S3 base station, which shows data on an ILI9488 touchscreen (LVGL) and bridges to Home Assistant over Zigbee.

## Firmware Targets
| Firmware | Board | Role |
|----------|-------|------|
| `float_core` | ESP32-S3-DevKitC-1 N32R16V | Base station: display + hub + Zigbee host |
| `float_zigbee` | XIAO ESP32-C6 | Zigbee NCP (UART to core) |
| `float_node` | ESP32-C6 Super Mini | Buoy sensor node, battery/solar, ESP-NOW TX |

## Architecture
```
Node (buoy) → ESP-NOW → Core (S3) → UART → NCP (C6, Zigbee) → Home Assistant
                            ↓
                         ILI9488 + XPT2046 (via msp3520 component)
                            ↓
                         LVGL UI
```

## Tech Stack
- **Framework**: ESP-IDF v5.5.3
- **Language**: C
- **Display**: msp3520 component (external, at ~/src/esp-msp3520)
- **UI**: LVGL
- **Comms**: ESP-NOW (node→core), Zigbee (core→HA)

## Origin
Rewrite of the `zenith` project (`~/src/zenith`), consolidating `zenith_hub` and `zenith_ui` into a single `float_core` firmware.

## Structure
```
float/
├── float_core/          # ESP32-S3 firmware
├── float_node/          # ESP32-C6 firmware
├── float_zigbee/        # XIAO ESP32-C6 firmware
├── float_components/    # Shared components
├── docs/
└── iteration/           # Workflow artifacts
```
