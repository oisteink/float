#include <stdlib.h>
#include "unity.h"
#include "float_now.h"
#include "float_data.h"

TEST_CASE("new_packet creates pairing packet", "[now]")
{
    float_now_packet_handle_t pkt = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_now_new_packet(NULL, FLOAT_PACKET_PAIRING, 0, &pkt));
    TEST_ASSERT_NOT_NULL(pkt);
    TEST_ASSERT_EQUAL(FLOAT_PACKET_PAIRING, pkt->header.type);
    TEST_ASSERT_EQUAL(FLOAT_NOW_VERSION, pkt->header.version);
    TEST_ASSERT_EQUAL(sizeof(float_now_payload_pairing_t), pkt->header.payload_size);
    free(pkt);
}

TEST_CASE("new_packet creates data packet with correct size", "[now]")
{
    uint8_t num_dp = 3;
    float_now_packet_handle_t pkt = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_now_new_packet(NULL, FLOAT_PACKET_DATA, num_dp, &pkt));
    TEST_ASSERT_NOT_NULL(pkt);
    TEST_ASSERT_EQUAL(FLOAT_PACKET_DATA, pkt->header.type);

    size_t expected_payload = sizeof(float_datapoints_t) + sizeof(float_datapoint_t) * num_dp;
    TEST_ASSERT_EQUAL(expected_payload, pkt->header.payload_size);
    free(pkt);
}

TEST_CASE("new_packet creates ack packet", "[now]")
{
    float_now_packet_handle_t pkt = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, float_now_new_packet(NULL, FLOAT_PACKET_ACK, 0, &pkt));
    TEST_ASSERT_NOT_NULL(pkt);
    TEST_ASSERT_EQUAL(FLOAT_PACKET_ACK, pkt->header.type);
    TEST_ASSERT_EQUAL(sizeof(float_now_payload_ack_t), pkt->header.payload_size);
    free(pkt);
}

TEST_CASE("new_packet rejects invalid type", "[now]")
{
    float_now_packet_handle_t pkt = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_now_new_packet(NULL, FLOAT_PACKET_MAX, 0, &pkt));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_now_new_packet(NULL, 0, 0, &pkt));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, float_now_new_packet(NULL, 255, 0, &pkt));
}

TEST_CASE("FLOAT_NOW_PAYLOAD_SIZE macro", "[now]")
{
    size_t packet_size = sizeof(float_now_packet_t) + 42;
    size_t payload = FLOAT_NOW_PAYLOAD_SIZE(packet_size);
    TEST_ASSERT_EQUAL(42, payload);

    payload = FLOAT_NOW_PAYLOAD_SIZE(sizeof(float_now_packet_t));
    TEST_ASSERT_EQUAL(0, payload);
}
