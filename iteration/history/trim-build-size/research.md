# Research: Trim float_core Build Size

Builds on [spec.md](spec.md).

## Baseline Measurements

| Metric | Value |
|--------|-------|
| Compiled source files | 1,525 |
| Flash Code (.text) | 910,474 bytes |
| Flash Data (.rodata) | 229,224 bytes |
| DIRAM used | 206,487 / 341,760 bytes (60%) |
| App binary | 0x138970 (1,280,368 bytes) |

## Top Contributors by Linked Size

| Archive | Total Size | Notes |
|---------|-----------|-------|
| liblvgl__lvgl.a | 422 KB | Required — demos/examples already off |
| libnet80211.a | 139 KB | WiFi MAC layer — required for ESP-NOW |
| liblwip.a | 78 KB | TCP/IP stack — **not needed** |
| libwpa_supplicant.a | 65 KB | WPA auth — can be trimmed |
| libpp.a | 65 KB | WiFi baseband — required for ESP-NOW |
| libmbedcrypto.a | 62 KB | Crypto — can be trimmed heavily |
| libc.a | 61 KB | C library — required |
| libesp_hw_support.a | 41 KB | Required |
| libphy.a | 36 KB | PHY layer — required for WiFi radio |

## MINIMAL_BUILD (Primary Lever)

ESP-IDF's build system has a `MINIMAL_BUILD` property that only compiles the "common" components (freertos, heap, newlib, hal, etc.) plus the direct and transitive dependencies of `main`. Everything else is excluded automatically — no need to manually list `EXCLUDE_COMPONENTS`.

**Usage:** One line in `CMakeLists.txt`, after `include()` but before `project()`:

```cmake
idf_build_set_property(MINIMAL_BUILD ON)
```

Since `main/CMakeLists.txt` already declares its dependencies (`nvs_flash esp_event float_now float_registry float_blink float_node_list console esp_msp3520`), the build system traces the full dependency graph and skips everything else.

This replaces the need for manual `EXCLUDE_COMPONENTS` and is already used in official ESP-IDF examples (zigbee, vbat, eth2ap, vlan_support).

**Caveat from docs:** "Certain features and configurations, such as those provided by esp_psram or espcoredump components, may not be available to your project by default if the minimal list of components is used." We use PSRAM (`CONFIG_SPIRAM=y`), so we may need to add `esp_psram` as an explicit dependency if it's not already transitively pulled in.

**Source:** [Build System — ESP-IDF v5.5.3 docs](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html)

## Findings by Category (Kconfig — Binary Size)

### WiFi Stack (lwip, wpa_supplicant, WiFi features)

**LWIP can be disabled.** ESP-NOW operates at MAC layer, doesn't use IP stack. WiFi's dependency on LWIP is conditional (`#ifdef CONFIG_ESP_NETIF_TCPIP_LWIP` in `wifi_init.c`). Setting `CONFIG_LWIP_ENABLE=n` is safe.

**SoftAP support can be disabled.** `float_now` uses `WIFI_MODE_STA` only. `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=n` removes AP code from WiFi and wpa_supplicant.

**WiFi Enterprise can be disabled.** EAP/PEAP/TTLS authentication is for WiFi association, not ESP-NOW. `CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT=n` removes EAP state machines.

**WPA3 can be disabled.** ESP-NOW uses CCMP (AES-128 CTR+CBC-MAC) for encryption, completely independent of WPA3-SAE/OWE. Both `CONFIG_ESP_WIFI_ENABLE_WPA3_SAE=n` and `CONFIG_ESP_WIFI_ENABLE_WPA3_OWE_STA=n` are safe.

### mbedTLS

**TLS can be fully disabled.** ESP-NOW's CCMP encryption is implemented in the WiFi driver (hardware AES), not via mbedTLS TLS stack. Setting `CONFIG_MBEDTLS_TLS_DISABLED=y` removes SSL/TLS handshake code, cipher suites, and client/server implementations (~19 source files).

**Certificate bundle can be disabled.** No X.509 cert validation needed. `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=n` saves 50+ KB flash.

**X.509/PEM parsing can be disabled.** `CONFIG_MBEDTLS_PEM_PARSE_C=n`, `CONFIG_MBEDTLS_X509_CRL_PARSE_C=n`, `CONFIG_MBEDTLS_X509_CSR_PARSE_C=n`.

**AES must stay.** `CONFIG_MBEDTLS_AES_C=y` and hardware AES acceleration are needed.

### Unused Components

With `MINIMAL_BUILD ON`, these are automatically excluded — no manual intervention needed. Without it, they compile but the linker dead-strips them (wasting build time, not binary size).

### Components That Must Stay

| Component | Reason |
|-----------|--------|
| esp_wifi | ESP-NOW requires WiFi MAC |
| esp_lcd | Required by esp_msp3520 (display driver) |
| esp_lcd_touch | Required by esp_msp3520 |
| esp_driver_spi | SPI for display + touch |
| esp_driver_gpio | GPIO for display pins |
| esp_driver_ledc | Backlight PWM (msp3520 + led_indicator) |
| esp_driver_rmt | WS2812 LED strip (led_indicator → led_strip) |
| lvgl | UI framework |
| nvs_flash | Persistent storage |
| console | Debug CLI |
| freertos, heap, newlib, hal, soc | Core platform |

### LWIP Disable — Caveat

When `CONFIG_LWIP_ENABLE=n`, `esp_netif` falls back to loopback mode. This is fine for ESP-NOW but means any future IP networking would need it re-enabled. The WiFi driver handles this gracefully.

## Estimated Impact

### Binary size savings (linked code):

| Change | Estimated Savings |
|--------|-------------------|
| Disable LWIP | ~78 KB |
| Disable WiFi Enterprise + WPA3 + SoftAP | ~20-40 KB from wpa_supplicant |
| Disable mbedTLS TLS + certs | ~30-40 KB from mbedcrypto |
| **Total binary** | **~130-160 KB** (~10-12% of app binary) |

### Build time savings (compiled files):

| Change | Files Saved |
|--------|-------------|
| MINIMAL_BUILD ON | Hundreds — excludes all non-dependency components |
| LWIP disable | ~93 files |
| mbedTLS TLS disable | ~30-50 files |
| wpa_supplicant trimming | ~20-30 files |

`MINIMAL_BUILD` is the big hammer for build time. Kconfig tweaks are for binary size of components that are still in the dependency graph.

## Risks

1. **PSRAM with MINIMAL_BUILD** — `esp_psram` may not be transitively required; need to verify it's pulled in or add it explicitly
2. **LWIP disable** — need to verify ESP-NOW init doesn't fail when LWIP is absent
3. **mbedTLS trimming could break wpa_supplicant** — the supplicant may reference TLS functions even if ESP-NOW doesn't use them; needs build verification
