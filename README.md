# FLOAT — Fjord-Linked Observation of Aquatic Temperature

Buoy-mounted ESP32-C6 sensor node measures water temperature at 1m depth and transmits via ESP-NOW to an ESP32-S3 base station, which shows data on an ILI9488 touchscreen (via LVGL) and bridges to Home Assistant over Zigbee.

## Architecture

```
Node (buoy) → ESP-NOW → Core (S3) → UART → NCP (C6, Zigbee) → Home Assistant
                            ↓
                         ILI9488 + XPT2046 (via msp3520 component)
                            ↓
                         LVGL UI
```

## Firmware

| Firmware | Board | Chip | Role |
|----------|-------|------|------|
| `float_core` | [ESP32-S3-DevKitC-1 N32R16V](docs/references/boards/esp32-s3-devkitc-1.md) | ESP32-S3 | Base station: display + hub + Zigbee host |
| `float_node` | [ESP32-C6 Super Mini](docs/references/boards/esp32-c6-super-mini.md) | ESP32-C6 | Buoy sensor node, battery/solar, ESP-NOW TX |
| `float_zigbee` | XIAO ESP32-C6 | ESP32-C6 | Zigbee NCP (UART to core) |

## Wiring

### float_core (ESP32-S3-DevKitC-1 N32R16V)

| GPIO | Function | Config |
|------|----------|--------|
| GPIO8 | WS2812 LED (status blink) | Kconfig `FLOAT_CORE_BLINK_GPIO` |
| Display + touch | ILI9488 + XPT2046 via SPI | `msp3520` component Kconfig |

### float_node (ESP32-C6 Super Mini)

| GPIO | Function |
|------|----------|
| GPIO8 | WS2812 LED (status indicator) |
| GPIO19 | I2C SDA (AHT30 / BMP280) |
| GPIO20 | I2C SCL (AHT30 / BMP280) |

## Repository Structure

```
float/
├── float_core/          # ESP32-S3: hub + display + zigbee host
├── float_node/          # ESP32-C6: buoy sensor node
├── float_zigbee/        # XIAO ESP32-C6: Zigbee NCP
├── float_components/    # Shared components
├── docs/                # Hardware docs, references
└── iteration/           # Workflow artifacts
```

## Build

```bash
source ~/esp/v5.5.3/esp-idf/export.sh

# From float_core/ or float_node/
idf.py set-target <chip>
idf.py build
idf.py flash monitor
```
