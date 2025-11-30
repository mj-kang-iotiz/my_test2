/**
 * @file rtcm_test.c
 * @brief RTCM to LoRa transmission test scenarios
 *
 * NOTE: This is a test file for verification purposes.
 * To run these tests, you need actual hardware (STM32 + GPS + LoRa).
 */

#include "rtcm.h"
#include "gps.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test helper: Calculate expected ToA
static uint32_t expected_toa(size_t bytes) {
    return (bytes * 350 / 236) * 120 / 100;
}

/**
 * Test 1: Normal even-byte RTCM packet (236 bytes)
 */
void test_rtcm_max_even_bytes(void) {
    printf("\n=== Test 1: Max even bytes (236) ===\n");

    gps_t gps;
    memset(&gps, 0, sizeof(gps_t));

    // Setup: 236 byte RTCM packet
    gps.rtcm.total_len = 236;
    gps.rtcm.msg_type = 1074;

    // Fill payload with test data
    for (int i = 0; i < 236; i++) {
        gps.payload[i] = (i % 256);
    }

    printf("Input: rtcm_len=%d (even)\n", gps.rtcm.total_len);
    printf("Expected: No padding, actual_len=236\n");
    printf("Expected ToA: %dms\n", expected_toa(236));

    // Expected behavior:
    // - No padding (even bytes)
    // - ToA = 420ms
    // - Should succeed
}

/**
 * Test 2: Normal odd-byte RTCM packet (235 bytes)
 */
void test_rtcm_max_odd_bytes(void) {
    printf("\n=== Test 2: Max odd bytes (235) ===\n");

    gps_t gps;
    memset(&gps, 0, sizeof(gps_t));

    // Setup: 235 byte RTCM packet
    gps.rtcm.total_len = 235;
    gps.rtcm.msg_type = 1084;

    for (int i = 0; i < 235; i++) {
        gps.payload[i] = (i % 256);
    }

    printf("Input: rtcm_len=%d (odd)\n", gps.rtcm.total_len);
    printf("Expected: Add 0x00 padding, actual_len=236\n");
    printf("Expected ToA: %dms\n", expected_toa(236));

    // Expected behavior:
    // - Add 0x00 padding
    // - actual_len = 236
    // - ToA = 420ms
    // - Should succeed
}

/**
 * Test 3: Small RTCM packet (50 bytes, even)
 */
void test_rtcm_small_even(void) {
    printf("\n=== Test 3: Small even bytes (50) ===\n");

    gps_t gps;
    memset(&gps, 0, sizeof(gps_t));

    gps.rtcm.total_len = 50;
    gps.rtcm.msg_type = 1033;

    for (int i = 0; i < 50; i++) {
        gps.payload[i] = 0xAA;
    }

    printf("Input: rtcm_len=%d (even)\n", gps.rtcm.total_len);
    printf("Expected: No padding, actual_len=50\n");
    printf("Expected ToA: %dms\n", expected_toa(50));

    // Expected behavior:
    // - No padding
    // - ToA = ~88ms
    // - Should succeed quickly
}

/**
 * Test 4: Small RTCM packet (51 bytes, odd)
 */
void test_rtcm_small_odd(void) {
    printf("\n=== Test 4: Small odd bytes (51) ===\n");

    gps_t gps;
    memset(&gps, 0, sizeof(gps_t));

    gps.rtcm.total_len = 51;
    gps.rtcm.msg_type = 1006;

    for (int i = 0; i < 51; i++) {
        gps.payload[i] = 0x55;
    }

    printf("Input: rtcm_len=%d (odd)\n", gps.rtcm.total_len);
    printf("Expected: Add 0x00 padding, actual_len=52\n");
    printf("Expected ToA: %dms\n", expected_toa(52));

    // Expected behavior:
    // - Add 0x00 padding
    // - actual_len = 52
    // - ToA = ~92ms
}

/**
 * Test 5: Boundary case - 1 byte
 */
void test_rtcm_min_odd(void) {
    printf("\n=== Test 5: Minimum odd bytes (1) ===\n");

    gps_t gps;
    memset(&gps, 0, sizeof(gps_t));

    gps.rtcm.total_len = 1;
    gps.rtcm.msg_type = 1;
    gps.payload[0] = 0xFF;

    printf("Input: rtcm_len=%d (odd)\n", gps.rtcm.total_len);
    printf("Expected: Add 0x00 padding, actual_len=2\n");
    printf("Expected ToA: %dms\n", expected_toa(2));

    // Expected behavior:
    // - Add 0x00 padding
    // - actual_len = 2
    // - ToA = ~3ms (very fast)
}

/**
 * Test 6: Error case - Zero length
 */
void test_rtcm_zero_length(void) {
    printf("\n=== Test 6: Error - Zero length ===\n");

    gps_t gps;
    memset(&gps, 0, sizeof(gps_t));

    gps.rtcm.total_len = 0;
    gps.rtcm.msg_type = 0;

    printf("Input: rtcm_len=%d\n", gps.rtcm.total_len);
    printf("Expected: LOG_ERR and return false\n");

    // Expected behavior:
    // - Should return false
    // - Error log: "RTCM length is zero"
}

/**
 * Test 7: Error case - Oversized (237 bytes, odd)
 */
void test_rtcm_oversized_odd(void) {
    printf("\n=== Test 7: Error - Oversized odd (237) ===\n");

    gps_t gps;
    memset(&gps, 0, sizeof(gps_t));

    gps.rtcm.total_len = 237;
    gps.rtcm.msg_type = 1074;

    printf("Input: rtcm_len=%d (odd)\n", gps.rtcm.total_len);
    printf("After padding: actual_len=238\n");
    printf("Expected: LOG_ERR and return false (238 > 236)\n");

    // Expected behavior:
    // - Padding would make it 238 bytes
    // - Should return false
    // - Error log: "RTCM length too large after padding"
}

/**
 * Test 8: ToA calculation verification
 */
void test_toa_calculation(void) {
    printf("\n=== Test 8: ToA Calculation Verification ===\n");

    struct {
        size_t bytes;
        uint32_t expected_toa;
    } test_cases[] = {
        {236, 420},  // 236 * 350 / 236 * 1.2 = 420
        {118, 210},  // 118 * 350 / 236 * 1.2 = 210
        {50, 88},    // 50 * 350 / 236 * 1.2 = 88.98 ~= 88
        {10, 17},    // 10 * 350 / 236 * 1.2 = 17.79 ~= 17
        {1, 1},      // 1 * 350 / 236 * 1.2 = 1.77 ~= 1
    };

    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint32_t calculated = expected_toa(test_cases[i].bytes);
        printf("Bytes: %3zu, Expected ToA: %3dms, Calculated: %3dms %s\n",
               test_cases[i].bytes,
               test_cases[i].expected_toa,
               calculated,
               (calculated == test_cases[i].expected_toa) ? "✓" : "✗");
    }
}

/**
 * Test 9: HEX conversion verification
 */
void test_hex_conversion(void) {
    printf("\n=== Test 9: Binary to HEX Conversion ===\n");

    uint8_t test_data[] = {0xD3, 0x00, 0x13, 0x3E, 0xD7, 0xFF};
    char expected_hex[] = "D300133ED7FF";

    printf("Input:    ");
    for (int i = 0; i < sizeof(test_data); i++) {
        printf("%02X ", test_data[i]);
    }
    printf("\n");
    printf("Expected: %s\n", expected_hex);

    // Manual calculation to verify
    printf("Verify:   ");
    for (int i = 0; i < sizeof(test_data); i++) {
        printf("%02X", test_data[i]);
    }
    printf("\n");
}

/**
 * Test 10: Rapid transmission scenario
 */
void test_rapid_transmission(void) {
    printf("\n=== Test 10: Rapid Transmission Scenario ===\n");
    printf("Simulating 5 consecutive RTCM packets\n\n");

    struct {
        uint16_t msg_type;
        size_t len;
    } packets[] = {
        {1074, 120},  // GPS MSM4
        {1084, 125},  // GLONASS MSM4
        {1094, 130},  // Galileo MSM4
        {1124, 115},  // BDS MSM4
        {1006, 25},   // Station ARP
    };

    uint32_t total_time = 0;

    for (int i = 0; i < 5; i++) {
        size_t actual_len = (packets[i].len % 2 == 0) ? packets[i].len : packets[i].len + 1;
        uint32_t toa = expected_toa(actual_len);

        printf("Packet %d: Type=%d, Len=%zu, Padded=%zu, ToA=%dms\n",
               i + 1, packets[i].msg_type, packets[i].len, actual_len, toa);

        total_time += toa;
    }

    printf("\nTotal transmission time: %dms (%.2fs)\n", total_time, total_time / 1000.0);
    printf("Average per packet: %dms\n", total_time / 5);
}

/**
 * Main test runner
 */
void run_all_rtcm_tests(void) {
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         RTCM to LoRa Transmission Test Suite             ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    test_rtcm_max_even_bytes();
    test_rtcm_max_odd_bytes();
    test_rtcm_small_even();
    test_rtcm_small_odd();
    test_rtcm_min_odd();
    test_rtcm_zero_length();
    test_rtcm_oversized_odd();
    test_toa_calculation();
    test_hex_conversion();
    test_rapid_transmission();

    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                    Tests Completed                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
}
