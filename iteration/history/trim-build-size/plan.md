# Plan: Trim float_core Build Size

Builds on [spec.md](spec.md) and [research.md](research.md).

## Scope

Enable `MINIMAL_BUILD` only. Kconfig tweaks (LWIP, mbedTLS, WPA3, etc.) deferred to a future iteration.

## Changes

### 1. `float_core/CMakeLists.txt`

Add `idf_build_set_property(MINIMAL_BUILD ON)` between `include()` and `project()`:

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "../float_components/"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(float_core)
```

### 2. Verify PSRAM

`CONFIG_SPIRAM=y` requires `esp_psram`. If it's not transitively pulled in via the dependency graph, the build will either fail or silently lose PSRAM support. If this happens, add `esp_psram` to `main/CMakeLists.txt` PRIV_REQUIRES.

## Verification

1. Clean build (`idf.py fullclean && idf.py build`)
2. Count compiled files from `compile_commands.json` — compare to baseline (1,525)
3. Check binary size via `idf.py size` — compare to baseline (1,280,368 bytes)
4. Flash and verify ESP-NOW, display, console, LED all still work
