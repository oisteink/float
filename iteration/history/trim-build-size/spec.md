# Spec: Trim float_core Build Size

## Goal

Reduce the number of compiled source files (currently ~1525) and resulting binary size in `float_core` by disabling ESP-IDF components and features that aren't needed.

## Context

`float_core` runs on ESP32-S3 and uses:
- **ESP-NOW** for receiving sensor data from buoy nodes (requires WiFi radio + minimal crypto)
- **LVGL** on ILI9488 via SPI (msp3520 component) for display
- **UART** to Zigbee NCP (float_zigbee)
- **NVS** for persistent storage
- **Console** for debug/CLI
- **WS2812 LED** via led_indicator (uses RMT driver)
- **SPI** for display + touch
- **ESP event loop** for inter-component comms

It does **not** use:
- Bluetooth (any flavor)
- TCP/IP networking, sockets, HTTP, MQTT, mDNS
- WiFi station/AP association (only raw ESP-NOW)
- TLS/HTTPS
- WiFi provisioning
- FAT/SPIFFS/SD card filesystems
- USB host/device
- ADC, I2S, MCPWM, PCNT, I2C (on core — node uses some of these)
- WiFi Enterprise (EAP/PEAP/etc.)
- WPA3-SAE, OWE (ESP-NOW doesn't do WPA handshakes)

## Requirements

1. Disable all unused ESP-IDF components and sub-features via Kconfig (`sdkconfig.defaults`)
2. Do not break any existing functionality (ESP-NOW, LVGL display, Zigbee UART bridge, console, LED blink, NVS)
3. Document each change with rationale
4. Measure before/after: file count and binary size (app partition)

## Acceptance Criteria

- Clean build with no warnings introduced
- All existing functionality still works (flash + manual test)
- Measurable reduction in compiled file count and binary size
- Changes are only in `sdkconfig.defaults` (no source code changes needed)

## Non-Goals

- Optimizing LVGL itself (already has demos/examples off; further font/widget trimming is a separate effort)
- Changing compiler optimization level
- Stripping debug info (useful during development)
