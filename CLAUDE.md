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

## Tooling

Activate the Serena project (`/home/ok/src/float`) at conversation start for LSP access to C/C++, markdown, and YAML. Use symbolic tools (`find_symbol`, `get_symbols_overview`) for code navigation and `search_for_pattern` for cross-file search including documentation.

## Build & Device Interaction

ESP-IDF v5.5.3. Each firmware has its own tmux session with ESP-IDF pre-sourced. See the `idf` skill for all `idf.py` operations: build, flash, monitor, REPL, testing, diagnostics, project scaffolding.

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

