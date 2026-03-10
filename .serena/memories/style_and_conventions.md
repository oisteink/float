# Code Style and Conventions

Follow ESP-IDF patterns throughout.

- **Error handling**: `esp_err_t` returns everywhere; use `ESP_RETURN_ON_ERROR` / `ESP_GOTO_ON_ERROR`
- **APIs**: Handle-based, never global singletons — pattern: `float_xxx_handle_t`
- **Events**: `esp_event_post()` + `esp_event_handler_register()` for inter-component communication
- **Config**: Kconfig (`Kconfig.projbuild`) for compile-time options, not `#ifdef`
- **Structs**: Opaque handles in `include/`, full struct definition in implementation file or `xxx_private.h`
- **Factory functions**: `float_xxx_new(config, &handle)` / `float_xxx_delete(handle)`
- **No unnecessary comments**: names should be self-documenting
- **No global state in components**: all state owned by handle structs
- **Language**: C (not C++)
