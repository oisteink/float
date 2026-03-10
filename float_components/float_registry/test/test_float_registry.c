#include "unity.h"
#include "nvs_flash.h"
#include "float_registry.h"

TEST_CASE("registry new and delete", "[registry]")
{
    nvs_flash_init();
    float_registry_handle_t reg = NULL;
    esp_err_t ret = float_registry_new(&reg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(reg);

    size_t count = 99;
    float_registry_get_node_count(reg, &count);
    TEST_ASSERT_EQUAL(0, count);

    float_registry_delete(reg);
}

TEST_CASE("registry stores and retrieves node info", "[registry]")
{
    nvs_flash_init();
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
}
