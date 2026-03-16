#include <string.h>
#include "unity.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "float_registry.h"

static void setup(void)
{
    nvs_flash_erase();
    nvs_flash_init();
    esp_event_loop_create_default();
}

static void teardown(void)
{
    esp_event_loop_delete_default();
}

TEST_CASE("registry new and delete", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    esp_err_t ret = float_registry_new(&reg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(reg);

    size_t count = 99;
    float_registry_get_node_count(reg, &count);
    TEST_ASSERT_EQUAL(0, count);

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry stores and retrieves node info", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info = { .mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF} };
    esp_err_t ret = float_registry_store_node_info(reg, &info);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    float_node_info_t out = {0};
    ret = float_registry_get_node_info(reg, info.mac, &out);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_MEMORY(info.mac, out.mac, 6);

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry forget_node removes node", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info = { .mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06} };
    float_registry_store_node_info(reg, &info);

    size_t count = 0;
    float_registry_get_node_count(reg, &count);
    TEST_ASSERT_EQUAL(1, count);

    TEST_ASSERT_EQUAL(ESP_OK, float_registry_forget_node(reg, info.mac));

    float_registry_get_node_count(reg, &count);
    TEST_ASSERT_EQUAL(0, count);

    float_node_info_t out = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, float_registry_get_node_info(reg, info.mac, &out));

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry forget_node not found", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_mac_address_t mac = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, float_registry_forget_node(reg, mac));

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry store_datapoints creates readings", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info = { .mac = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60} };
    float_registry_store_node_info(reg, &info);

    float_datapoint_t dp = {
        .reading_type = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE,
        .value = 21.5f
    };
    TEST_ASSERT_EQUAL(ESP_OK, float_registry_store_datapoints(reg, info.mac, &dp, 1));

    float_registry_reading_t readings[3];
    size_t count = 3;
    TEST_ASSERT_EQUAL(ESP_OK, float_registry_get_latest_readings(reg, info.mac, readings, &count));
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, readings[0].sensor_class);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, readings[0].reading.value);

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry get_latest_readings multiple types", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info = { .mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66} };
    float_registry_store_node_info(reg, &info);

    float_datapoint_t dps[2] = {
        { .reading_type = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, .value = 19.0f },
        { .reading_type = FLOAT_SENSOR_CLASS_AIR_HUMIDITY,    .value = 55.0f },
    };
    float_registry_store_datapoints(reg, info.mac, dps, 2);

    // Store again with updated temperature
    float_datapoint_t update = { .reading_type = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, .value = 20.0f };
    float_registry_store_datapoints(reg, info.mac, &update, 1);

    float_registry_reading_t readings[3];
    size_t count = 3;
    float_registry_get_latest_readings(reg, info.mac, readings, &count);
    TEST_ASSERT_EQUAL(2, count);

    // Latest temperature should be 20.0
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, readings[0].sensor_class);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, readings[0].reading.value);
    // Humidity should still be 55.0
    TEST_ASSERT_EQUAL(FLOAT_SENSOR_CLASS_AIR_HUMIDITY, readings[1].sensor_class);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.0f, readings[1].reading.value);

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry get_history returns oldest first", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info = { .mac = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6} };
    float_registry_store_node_info(reg, &info);

    for (int i = 0; i < 5; i++) {
        float_datapoint_t dp = { .reading_type = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, .value = (float)(10 + i) };
        float_registry_store_datapoints(reg, info.mac, &dp, 1);
    }

    float_reading_t history[5];
    size_t count = 5;
    TEST_ASSERT_EQUAL(ESP_OK, float_registry_get_history(reg, info.mac, FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, history, &count));
    TEST_ASSERT_EQUAL(5, count);

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)(10 + i), history[i].value);
    }

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry ringbuffer wraps around", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info = { .mac = {0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6} };
    float_registry_store_node_info(reg, &info);

    // Fill beyond capacity (32)
    for (int i = 0; i < 40; i++) {
        float_datapoint_t dp = { .reading_type = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, .value = (float)i };
        float_registry_store_datapoints(reg, info.mac, &dp, 1);
    }

    float_reading_t history[FLOAT_RING_CAPACITY];
    size_t count = FLOAT_RING_CAPACITY;
    float_registry_get_history(reg, info.mac, FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, history, &count);
    TEST_ASSERT_EQUAL(FLOAT_RING_CAPACITY, count);

    // Should contain values 8..39 (oldest 0..7 evicted)
    for (size_t i = 0; i < count; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)(8 + i), history[i].value);
    }

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry get_max_last_24h", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info = { .mac = {0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6} };
    float_registry_store_node_info(reg, &info);

    float values[] = {15.0f, 25.0f, 20.0f, 10.0f, 22.0f};
    for (int i = 0; i < 5; i++) {
        float_datapoint_t dp = { .reading_type = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, .value = values[i] };
        float_registry_store_datapoints(reg, info.mac, &dp, 1);
    }

    float_reading_t max_reading;
    TEST_ASSERT_EQUAL(ESP_OK, float_registry_get_max_last_24h(reg, info.mac, FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, &max_reading));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, max_reading.value);

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry get_min_last_24h", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info = { .mac = {0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6} };
    float_registry_store_node_info(reg, &info);

    float values[] = {15.0f, 25.0f, 20.0f, 10.0f, 22.0f};
    for (int i = 0; i < 5; i++) {
        float_datapoint_t dp = { .reading_type = FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, .value = values[i] };
        float_registry_store_datapoints(reg, info.mac, &dp, 1);
    }

    float_reading_t min_reading;
    TEST_ASSERT_EQUAL(ESP_OK, float_registry_get_min_last_24h(reg, info.mac, FLOAT_SENSOR_CLASS_AIR_TEMPERATURE, &min_reading));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, min_reading.value);

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry get_node_count multiple nodes", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    for (int i = 0; i < 5; i++) {
        float_node_info_t info = { .mac = {0xE0, 0xE1, 0xE2, 0xE3, 0xE4, (uint8_t)i} };
        float_registry_store_node_info(reg, &info);
    }

    size_t count = 0;
    float_registry_get_node_count(reg, &count);
    TEST_ASSERT_EQUAL(5, count);

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry get_all_node_macs", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    float_node_info_t info1 = { .mac = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0x01} };
    float_node_info_t info2 = { .mac = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0x02} };
    float_registry_store_node_info(reg, &info1);
    float_registry_store_node_info(reg, &info2);

    float_mac_address_t macs[10];
    size_t count = 10;
    TEST_ASSERT_EQUAL(ESP_OK, float_registry_get_all_node_macs(reg, macs, &count));
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_MEMORY(info1.mac, macs[0], 6);
    TEST_ASSERT_EQUAL_MEMORY(info2.mac, macs[1], 6);

    float_registry_delete(reg);
    teardown();
}

TEST_CASE("registry max nodes limit", "[registry]")
{
    setup();
    float_registry_handle_t reg = NULL;
    float_registry_new(&reg);

    // Add 10 nodes (max)
    for (int i = 0; i < 10; i++) {
        float_node_info_t info = { .mac = {0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t)i} };
        TEST_ASSERT_EQUAL(ESP_OK, float_registry_store_node_info(reg, &info));
    }

    size_t count = 0;
    float_registry_get_node_count(reg, &count);
    TEST_ASSERT_EQUAL(10, count);

    // 11th should fail
    float_node_info_t overflow = { .mac = {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF} };
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, float_registry_store_node_info(reg, &overflow));

    float_registry_delete(reg);
    teardown();
}
