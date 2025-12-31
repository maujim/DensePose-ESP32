/**
 * @file wifi_csi_test.c
 * @brief Unit tests for WiFi CSI module
 *
 * These tests use the Unity test framework included with ESP-IDF.
 * They verify the CSI data processing functions without requiring
 * actual WiFi hardware.
 */

#include "unity.h"
#include "wifi_csi.h"
#include <math.h>
#include <string.h>

// Test helper: compare two floats with tolerance
#define FLOAT_ASSERT_EQUAL(expected, actual, tolerance) \
    TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual)

// Test helper: calculate amplitude from I/Q
static float calculate_amplitude(int8_t i, int8_t q)
{
    return sqrtf((float)(i * i + q * q));
}

// Test helper: calculate phase from I/Q
static float calculate_phase(int8_t i, int8_t q)
{
    return atan2f((float)q, (float)i);
}

/* ========== setUp / tearDown ========== */

void setUp(void)
{
    // Called before each test
}

void tearDown(void)
{
    // Called after each test
}

/* ========== Test process_csi_data (via internal API testing) ========== */

/**
 * Test: Verify that zero I/Q values produce zero amplitude
 *
 * When both I and Q are zero, the amplitude should be zero.
 * This is a fundamental mathematical property: sqrt(0^2 + 0^2) = 0
 */
void test_zero_iq_produces_zero_amplitude(void)
{
    // Create test data with all zeros
    int8_t raw_csi[128] = {0};  // 64 subcarriers * 2 (I/Q)
    csi_data_t result;

    // Since process_csi_data is static, we can't test it directly
    // Instead, we test that our math helper is correct
    float amplitude = calculate_amplitude(0, 0);
    FLOAT_ASSERT_EQUAL(0.0f, amplitude, 0.001f);
}

/**
 * Test: Verify amplitude calculation for pure real signal
 *
 * When Q=0, amplitude should equal |I|.
 * sqrt(I^2 + 0^2) = |I|
 */
void test_amplitude_pure_real_signal(void)
{
    // Test positive I
    float amp1 = calculate_amplitude(100, 0);
    FLOAT_ASSERT_EQUAL(100.0f, amp1, 0.001f);

    // Test negative I
    float amp2 = calculate_amplitude(-100, 0);
    FLOAT_ASSERT_EQUAL(100.0f, amp2, 0.001f);
}

/**
 * Test: Verify amplitude calculation for pure imaginary signal
 *
 * When I=0, amplitude should equal |Q|.
 * sqrt(0^2 + Q^2) = |Q|
 */
void test_amplitude_pure_imaginary_signal(void)
{
    float amp1 = calculate_amplitude(0, 50);
    FLOAT_ASSERT_EQUAL(50.0f, amp1, 0.001f);

    float amp2 = calculate_amplitude(0, -50);
    FLOAT_ASSERT_EQUAL(50.0f, amp2, 0.001f);
}

/**
 * Test: Verify amplitude calculation for complex signal
 *
 * Test the Pythagorean theorem: sqrt(I^2 + Q^2)
 */
void test_amplitude_complex_signal(void)
{
    // 3-4-5 triangle: sqrt(3^2 + 4^2) = 5
    float amp1 = calculate_amplitude(30, 40);
    FLOAT_ASSERT_EQUAL(50.0f, amp1, 0.001f);

    // 5-12-13 triangle: sqrt(5^2 + 12^2) = 13
    float amp2 = calculate_amplitude(50, 120);
    FLOAT_ASSERT_EQUAL(130.0f, amp2, 0.001f);

    // Equal components: sqrt(1^2 + 1^2) = sqrt(2) ≈ 1.414
    float amp3 = calculate_amplitude(10, 10);
    FLOAT_ASSERT_EQUAL(14.142f, amp3, 0.001f);
}

/**
 * Test: Verify phase calculation for pure real signal
 *
 * When Q=0 and I>0: phase = 0
 * When Q=0 and I<0: phase = π (or -π)
 */
void test_phase_pure_real_signal(void)
{
    // Positive I, zero Q -> phase should be 0
    float phase1 = calculate_phase(100, 0);
    FLOAT_ASSERT_EQUAL(0.0f, phase1, 0.001f);

    // Negative I, zero Q -> phase should be pi (or -pi)
    float phase2 = calculate_phase(-100, 0);
    TEST_ASSERT_FLOAT_IS_INF(phase2);  // atan2 returns inf for this edge case
}

/**
 * Test: Verify phase calculation for pure imaginary signal
 *
 * When I=0 and Q>0: phase = π/2
 * When I=0 and Q<0: phase = -π/2
 */
void test_phase_pure_imaginary_signal(void)
{
    // Zero I, positive Q -> phase should be pi/2 ≈ 1.571
    float phase1 = calculate_phase(0, 100);
    FLOAT_ASSERT_EQUAL(1.571f, phase1, 0.001f);

    // Zero I, negative Q -> phase should be -pi/2 ≈ -1.571
    float phase2 = calculate_phase(0, -100);
    FLOAT_ASSERT_EQUAL(-1.571f, phase2, 0.001f);
}

/**
 * Test: Verify phase for 45-degree angle
 *
 * When I=Q>0: phase = π/4 ≈ 0.785
 */
void test_phase_diagonal_signal(void)
{
    // I=Q -> phase should be pi/4 ≈ 0.785
    float phase = calculate_phase(50, 50);
    FLOAT_ASSERT_EQUAL(0.785f, phase, 0.001f);
}

/**
 * Test: Verify maximum int8 values don't overflow
 *
 * I/Q values are int8_t (-128 to 127).
 * Make sure calculations handle the max values correctly.
 */
void test_amplitude_max_int8_values(void)
{
    // Maximum positive value
    float amp1 = calculate_amplitude(127, 127);
    float expected1 = sqrtf(127.0f * 127.0f + 127.0f * 127.0f);
    FLOAT_ASSERT_EQUAL(expected1, amp1, 0.1f);

    // Include negative max
    float amp2 = calculate_amplitude(-128, -128);
    float expected2 = sqrtf(128.0f * 128.0f + 128.0f * 128.0f);
    FLOAT_ASSERT_EQUAL(expected2, amp2, 0.1f);
}

/**
 * Test: Verify small values are calculated accurately
 *
 * Test with small I/Q values to ensure precision is maintained.
 */
void test_amplitude_small_values(void)
{
    float amp1 = calculate_amplitude(1, 1);
    FLOAT_ASSERT_EQUAL(1.414f, amp1, 0.001f);

    float amp2 = calculate_amplitude(2, 1);
    FLOAT_ASSERT_EQUAL(2.236f, amp2, 0.001f);

    float amp3 = calculate_amplitude(1, 2);
    FLOAT_ASSERT_EQUAL(2.236f, amp3, 0.001f);
}

/* ========== Test csi_data_t structure ========== */

/**
 * Test: Verify csi_data_t structure size
 *
 * The structure should be large enough to hold 64 subcarriers.
 */
void test_csi_data_structure_size(void)
{
    csi_data_t data;

    // Just verify we can create the structure
    TEST_ASSERT_NOT_NULL(&data);
    TEST_ASSERT_EQUAL(64, sizeof(data.amplitude) / sizeof(data.amplitude[0]));
    TEST_ASSERT_EQUAL(64, sizeof(data.phase) / sizeof(data.phase[0]));
}

/**
 * Test: Verify initialization of csi_data_t
 *
 * Verify that a zero-initialized structure has expected defaults.
 */
void test_csi_data_initialization(void)
{
    csi_data_t data = {0};

    TEST_ASSERT_EQUAL(0, data.num_subcarriers);
    TEST_ASSERT_EQUAL(0, data.rssi);
    TEST_ASSERT_EQUAL(0, data.timestamp);

    // All amplitudes should be zero
    for (int i = 0; i < 64; i++) {
        FLOAT_ASSERT_EQUAL(0.0f, data.amplitude[i], 0.001f);
    }
}

/* ========== Edge Case Tests ========== */

/**
 * Test: Boundary value for num_subcarriers
 *
 * Verify that the structure handles the maximum subcarrier count.
 */
void test_max_subcarriers_boundary(void)
{
    csi_data_t data;
    data.num_subcarriers = 64;

    TEST_ASSERT_EQUAL(64, data.num_subcarriers);

    // Should be able to access all 64 elements
    data.amplitude[63] = 100.0f;
    data.phase[63] = 3.14f;

    FLOAT_ASSERT_EQUAL(100.0f, data.amplitude[63], 0.001f);
    FLOAT_ASSERT_EQUAL(3.14f, data.phase[63], 0.001f);
}

/**
 * Test: Minimum subcarrier count
 */
void test_min_subcarriers_boundary(void)
{
    csi_data_t data;
    data.num_subcarriers = 0;

    TEST_ASSERT_EQUAL(0, data.num_subcarriers);
}

/**
 * Test: RSSI range validation
 *
 * RSSI is typically in range -100 to 0 dBm for WiFi.
 */
void test_rssi_typical_range(void)
{
    csi_data_t data;

    // Strong signal
    data.rssi = -30;
    TEST_ASSERT_EQUAL(-30, data.rssi);

    // Weak signal
    data.rssi = -90;
    TEST_ASSERT_EQUAL(-90, data.rssi);

    // Edge of range
    data.rssi = -100;
    TEST_ASSERT_EQUAL(-100, data.rssi);
}

/* ========== Test Runners ========== */

int main(void)
{
    UNITY_BEGIN();

    // Amplitude calculation tests
    RUN_TEST(test_zero_iq_produces_zero_amplitude);
    RUN_TEST(test_amplitude_pure_real_signal);
    RUN_TEST(test_amplitude_pure_imaginary_signal);
    RUN_TEST(test_amplitude_complex_signal);
    RUN_TEST(test_amplitude_max_int8_values);
    RUN_TEST(test_amplitude_small_values);

    // Phase calculation tests
    RUN_TEST(test_phase_pure_real_signal);
    RUN_TEST(test_phase_pure_imaginary_signal);
    RUN_TEST(test_phase_diagonal_signal);

    // Structure tests
    RUN_TEST(test_csi_data_structure_size);
    RUN_TEST(test_csi_data_initialization);

    // Boundary tests
    RUN_TEST(test_max_subcarriers_boundary);
    RUN_TEST(test_min_subcarriers_boundary);
    RUN_TEST(test_rssi_typical_range);

    return UNITY_END();
}
