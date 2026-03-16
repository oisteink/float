# Spec: Sensor-Typed Data Model & Context-Aware UI

## Goal

Nodes describe their sensors so the core can present readings appropriately without hardcoded assumptions about what a node carries.

## Problem

A datapoint carries only a generic `reading_type` (temperature/humidity/pressure). The UI shows every node identically — same icons, same colors, same layout. A water temperature sensor looks the same as an air temperature sensor. Adding a new sensor type requires changing the enum, the sensor vtable, and the UI in lockstep.

## Requirements

1. **Sensor classes** — Each sensor reading carries a *sensor class* that conveys both what it measures and the context. Replaces the current `float_sensor_type_t` enum. Examples: `water_temperature`, `air_temperature`, `air_humidity`, `air_pressure`, `wind_speed`, `rainfall`, `battery_voltage`.

2. **Node self-description** — When a node pairs with core, it declares what sensor classes it carries. Core stores this in the registry alongside the MAC. No manual configuration on the core side.

3. **Presentation hints** — Each sensor class maps to presentation metadata on the core: display label, unit string, icon, color, value format, and reasonable min/max range. This mapping lives on the core only — nodes just declare their sensor classes.

4. **Dynamic UI cards** — The node_list UI renders only the sensors a node actually has, using the presentation hints for that sensor class. No more hardcoded temp/hum/pres labels.

5. **Sensor vtable refactor** — The sensor abstraction layer (`float_sensor.h`) replaces hardcoded `read_temperature/humidity/pressure` function pointers with a class-aware interface. Sensor drivers declare their sensor classes directly, and datapoints come out tagged with the correct class — no translation layer between sensor and wire format.

6. **Configurable mock sensor** — The mock sensor driver supports Kconfig selection of which sensor classes it declares. This allows flashing two mock nodes with different personalities to test diversity scenarios without physical sensors.

7. **Extensibility** — Adding a new sensor class requires: (a) adding it to the sensor class enum, (b) adding a sensor driver (or extending one) that declares it, (c) adding its presentation entry on core. No changes to the protocol or UI component API.

## Acceptance Criteria

Two physical nodes, each with a different sensor:

- **Node A** (ttyACM0): AHT20 — reports `air_temperature` + `air_humidity`
- **Node B** (ttyACM1): BMP280 — reports `air_temperature` + `air_pressure`

Verify:

- Node A's card shows only temperature and humidity, with appropriate styling for each
- Node B's card shows only temperature and pressure, with appropriate styling for each
- Both cards show temperature, but are visually distinguishable if their sensor classes differ (e.g. one changed to `water_temperature`)
- Adding a new sensor class to the enum, a driver that declares it, and a presentation entry is sufficient to support it end-to-end — no protocol or UI component changes needed
- Nodes re-pair correctly after core reboot (registry persists sensor descriptors)

## Non-Goals

- Graphs, history views, or detail screens (card/list view only)
- Home Assistant / Zigbee integration for new sensor types
- Node configuration from the core side
- Battery voltage reporting (no hardware divider wired)
