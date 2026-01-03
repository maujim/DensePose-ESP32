/**
 * @file pose_inference.c
 * @brief Implementation of pose estimation from WiFi CSI
 *
 * This implementation follows a simplified version of the DensePose from WiFi paper,
 * adapted for ESP32-S3 hardware constraints.
 *
 * Paper: "DensePose From WiFi" (arXiv:2301.00250)
 * Key concepts:
 * - Use temporal CSI windows for feature extraction
 * - Human bodies affect WiFi signals through absorption and reflection
 * - ML models can map CSI patterns to pose information
 *
 * ESP32 Adaptations:
 * - Reduced model size (knowledge distillation)
 * - INT8 quantization for efficiency
 * - Simplified output (presence + basic pose classes)
 * - PSRAM-based buffering for temporal windows
 */

#include "pose_inference.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *TAG = "pose_inference";

// Default configuration
#define DEFAULT_WINDOW_SIZE_MS 500
#define DEFAULT_SAMPLING_RATE_HZ 100
#define DEFAULT_NUM_SUBCARRIERS 52
#define TEMPORAL_BUFFER_SIZE (DEFAULT_WINDOW_SIZE_MS * DEFAULT_SAMPLING_RATE_HZ / 1000)

// State variables
static pose_config_t s_config;
static bool s_initialized = false;
static SemaphoreHandle_t s_mutex = NULL;
static pose_callback_t s_user_callback = NULL;
static void *s_user_ctx = NULL;

// Temporal CSI buffer (stored in PSRAM if available)
static float *s_amplitude_buffer = NULL;
static float *s_phase_buffer = NULL;
static int8_t *s_rssi_buffer = NULL;
static int s_buffer_index = 0;
static bool s_buffer_ready = false;

// Latest result
static pose_result_t s_latest_result;

// Statistics
static uint32_t s_inferences_count = 0;
static uint64_t s_total_inference_time_us = 0;

/**
 * @brief Allocate buffers in PSRAM or internal RAM
 *
 * PSRAM is slower but abundant (2MB). Internal SRAM is fast but limited (~300KB usable).
 * For CSI temporal windows, we need PSRAM due to size requirements.
 */
static esp_err_t allocate_buffers(void)
{
    size_t buffer_size = TEMPORAL_BUFFER_SIZE * DEFAULT_NUM_SUBCARRIERS * sizeof(float);

    ESP_LOGI(TAG, "Allocating CSI buffers: %zu bytes each", buffer_size);

    // Try PSRAM first
    s_amplitude_buffer = (float *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    s_phase_buffer = (float *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);

    if (s_amplitude_buffer == NULL || s_phase_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffers");
        // Try internal RAM as fallback (may fail due to size)
        if (s_amplitude_buffer == NULL) {
            s_amplitude_buffer = (float *)malloc(buffer_size);
        }
        if (s_phase_buffer == NULL) {
            s_phase_buffer = (float *)malloc(buffer_size);
        }

        if (s_amplitude_buffer == NULL || s_phase_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate buffers in internal RAM too");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGW(TAG, "Using internal RAM (PSRAM not available)");
    } else {
        ESP_LOGI(TAG, "Successfully allocated PSRAM buffers");
    }

    // RSSI buffer (smaller, can fit in internal RAM)
    size_t rssi_size = TEMPORAL_BUFFER_SIZE * sizeof(int8_t);
    s_rssi_buffer = (int8_t *)malloc(rssi_size);
    if (s_rssi_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate RSSI buffer");
        return ESP_ERR_NO_MEM;
    }

    // Initialize buffers to zero
    memset(s_amplitude_buffer, 0, buffer_size);
    memset(s_phase_buffer, 0, buffer_size);
    memset(s_rssi_buffer, 0, rssi_size);

    return ESP_OK;
}

/**
 * @brief Calculate mean of an array
 */
static float calculate_mean(const float *data, int len)
{
    if (len == 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum / len;
}

/**
 * @brief Calculate standard deviation of an array
 */
static float calculate_std(const float *data, int len, float mean)
{
    if (len == 0) return 0.0f;
    float sum_sq_diff = 0.0f;
    for (int i = 0; i < len; i++) {
        float diff = data[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sqrtf(sum_sq_diff / len);
}

/**
 * @brief Calculate variance of phase array
 *
 * Phase variance is a good indicator of human activity.
 * Static environments have stable phase; humans cause fluctuations.
 */
static float calculate_phase_variance(const float *phase, int len)
{
    if (len < 2) return 0.0f;

    // Calculate differences between consecutive samples
    float sum_diff_sq = 0.0f;
    for (int i = 1; i < len; i++) {
        float diff = phase[i] - phase[i-1];
        // Wrap phase difference to [-π, π]
        while (diff > M_PI) diff -= 2 * M_PI;
        while (diff < -M_PI) diff += 2 * M_PI;
        sum_diff_sq += diff * diff;
    }

    return sum_diff_sq / (len - 1);
}

/**
 * @brief Detect human presence based on CSI statistics
 *
 * This is a simplified detection algorithm based on the paper's insights:
 * - Human presence increases CSI variance
 * - Amplitude_std and phase_variance are key indicators
 * - RSSI changes can also indicate presence
 *
 * TODO: Replace with ML model for better accuracy
 */
static void detect_human_presence(float amp_mean, float amp_std, float phase_var,
                                  int8_t rssi, pose_result_t *result)
{
    // Thresholds (tune these based on empirical data)
    const float AMP_STD_THRESHOLD_EMPTY = 2.0f;
    const float AMP_STD_THRESHOLD_PRESENT = 5.0f;
    const float PHASE_VAR_THRESHOLD_MOVING = 0.5f;
    const float MOTION_THRESHOLD = 0.3f;

    result->amplitude_mean = amp_mean;
    result->amplitude_std = amp_std;
    result->phase_variance = phase_var;

    // Basic presence detection
    if (amp_std < AMP_STD_THRESHOLD_EMPTY) {
        result->human_detected = false;
        result->pose_class = POSE_EMPTY;
        result->confidence = 0.9f;
        result->motion_level = 0.0f;
    } else {
        result->human_detected = true;

        // Calculate motion level from phase variance
        result->motion_level = fminf(phase_var / PHASE_VAR_THRESHOLD_MOVING, 1.0f);

        // Classify activity based on motion level and amplitude variance
        if (result->motion_level < MOTION_THRESHOLD) {
            result->pose_class = POSE_PRESENT;  // Static human
            result->confidence = 0.7f;
        } else {
            result->pose_class = POSE_MOVING;   // Moving human
            result->confidence = 0.6f;

            // TODO: More sophisticated classification for walking/sitting/standing
            // This would require a trained ML model
        }
    }

    result->timestamp = (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief Run inference on temporal CSI window
 *
 * This function implements the core inference pipeline:
 * 1. Extract features from temporal window
 * 2. Run ML model (simplified rule-based for now)
 * 3. Generate pose detection result
 *
 * TODO: Integrate TensorFlow Lite Micro model
 */
static void run_inference(void)
{
    uint64_t start_time = esp_timer_get_time();

    pose_result_t result = {0};

    // Aggregate statistics across temporal window
    float amp_mean = 0.0f;
    float amp_std = 0.0f;
    float phase_var = 0.0f;
    int8_t avg_rssi = 0;

    // Calculate statistics across all subcarriers and time steps
    int total_samples = TEMPORAL_BUFFER_SIZE * DEFAULT_NUM_SUBCARRIERS;

    // Amplitude statistics
    float amp_sum = 0.0f;
    for (int i = 0; i < total_samples; i++) {
        amp_sum += s_amplitude_buffer[i];
    }
    amp_mean = amp_sum / total_samples;

    float amp_var_sum = 0.0f;
    for (int i = 0; i < total_samples; i++) {
        float diff = s_amplitude_buffer[i] - amp_mean;
        amp_var_sum += diff * diff;
    }
    amp_std = sqrtf(amp_var_sum / total_samples);

    // Phase variance (sum across subcarriers first, then time)
    float total_phase_var = 0.0f;
    for (int s = 0; s < DEFAULT_NUM_SUBCARRIERS; s++) {
        // Extract time series for this subcarrier
        float subcarrier_phase[TEMPORAL_BUFFER_SIZE];
        for (int t = 0; t < TEMPORAL_BUFFER_SIZE; t++) {
            subcarrier_phase[t] = s_phase_buffer[t * DEFAULT_NUM_SUBCARRIERS + s];
        }
        float sub_mean = calculate_mean(subcarrier_phase, TEMPORAL_BUFFER_SIZE);
        float sub_var = calculate_std(subcarrier_phase, TEMPORAL_BUFFER_SIZE, sub_mean);
        total_phase_var += sub_var * sub_var;
    }
    phase_var = sqrtf(total_phase_var / DEFAULT_NUM_SUBCARRIERS);

    // Average RSSI
    int rssi_sum = 0;
    for (int i = 0; i < TEMPORAL_BUFFER_SIZE; i++) {
        rssi_sum += s_rssi_buffer[i];
    }
    avg_rssi = rssi_sum / TEMPORAL_BUFFER_SIZE;

    // Run detection
    detect_human_presence(amp_mean, amp_std, phase_var, avg_rssi, &result);

    // Calculate inference time
    uint64_t end_time = esp_timer_get_time();
    result.inference_time_ms = (uint32_t)((end_time - start_time) / 1000);

    // Update statistics
    s_inferences_count++;
    s_total_inference_time_us += (end_time - start_time);

    // Store result (thread-safe)
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memcpy(&s_latest_result, &result, sizeof(pose_result_t));
        xSemaphoreGive(s_mutex);
    }

    // Log inference results
    ESP_LOGI(TAG, "Inference #%lu: detected=%s, pose=%d, confidence=%.2f, "
                  "amp_std=%.2f, phase_var=%.4f, motion=%.2f, latency=%lums",
             s_inferences_count,
             result.human_detected ? "yes" : "no",
             result.pose_class,
             result.confidence,
             amp_std,
             phase_var,
             result.motion_level,
             result.inference_time_ms);

    // Call user callback if registered
    if (s_user_callback != NULL) {
        s_user_callback(&result, s_user_ctx);
    }
}

esp_err_t pose_init(const pose_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing pose estimation module...");

    // Load configuration or use defaults
    if (config != NULL) {
        memcpy(&s_config, config, sizeof(pose_config_t));
    } else {
        s_config.window_size_ms = DEFAULT_WINDOW_SIZE_MS;
        s_config.sampling_rate_hz = DEFAULT_SAMPLING_RATE_HZ;
        s_config.num_subcarriers = DEFAULT_NUM_SUBCARRIERS;
        s_config.use_amplitude = true;
        s_config.use_phase = true;
        s_config.enable_presence_detection = true;
        s_config.enable_pose_classification = false;  // Disable for now
    }

    // Create mutex
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Allocate CSI buffers
    esp_err_t ret = allocate_buffers();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ret;
    }

    // TODO: Load ML model from flash
    // TODO: Initialize TensorFlow Lite Micro interpreter

    s_initialized = true;
    ESP_LOGI(TAG, "Pose estimation initialized successfully");
    ESP_LOGI(TAG, "Config: window=%dms, rate=%dHz, subcarriers=%d",
             s_config.window_size_ms, s_config.sampling_rate_hz, s_config.num_subcarriers);

    return ESP_OK;
}

esp_err_t pose_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing pose estimation...");

    // Free buffers
    if (s_amplitude_buffer != NULL) {
        heap_caps_free(s_amplitude_buffer);
        s_amplitude_buffer = NULL;
    }
    if (s_phase_buffer != NULL) {
        heap_caps_free(s_phase_buffer);
        s_phase_buffer = NULL;
    }
    if (s_rssi_buffer != NULL) {
        free(s_rssi_buffer);
        s_rssi_buffer = NULL;
    }

    // Clean up mutex
    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_initialized = false;
    s_user_callback = NULL;
    s_user_ctx = NULL;

    return ESP_OK;
}

esp_err_t pose_register_callback(pose_callback_t callback, void *user_ctx)
{
    s_user_callback = callback;
    s_user_ctx = user_ctx;
    ESP_LOGI(TAG, "Callback registered");
    return ESP_OK;
}

esp_err_t pose_process_csi(const float *amplitude, const float *phase,
                           int num_subcarriers, int8_t rssi)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (amplitude == NULL || phase == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Store CSI data in temporal buffer
    int subs = fmin(num_subcarriers, s_config.num_subcarriers);

    for (int i = 0; i < subs; i++) {
        int idx = s_buffer_index * s_config.num_subcarriers + i;
        s_amplitude_buffer[idx] = amplitude[i];
        s_phase_buffer[idx] = phase[i];
    }

    s_rssi_buffer[s_buffer_index] = rssi;
    s_buffer_index++;

    // Check if buffer is full
    if (s_buffer_index >= TEMPORAL_BUFFER_SIZE) {
        s_buffer_index = 0;
        s_buffer_ready = true;
    }

    // Run inference when buffer is ready
    if (s_buffer_ready && s_buffer_index == 0) {
        run_inference();
    }

    return ESP_OK;
}

esp_err_t pose_get_latest_result(pose_result_t *result)
{
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_inferences_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(result, &s_latest_result, sizeof(pose_result_t));
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

bool pose_is_active(void)
{
    return s_initialized;
}

void pose_get_stats(uint32_t *num_inferences, float *avg_latency_ms)
{
    if (num_inferences != NULL) {
        *num_inferences = s_inferences_count;
    }
    if (avg_latency_ms != NULL) {
        if (s_inferences_count > 0) {
            *avg_latency_ms = (float)(s_total_inference_time_us / s_inferences_count) / 1000.0f;
        } else {
            *avg_latency_ms = 0.0f;
        }
    }
}
