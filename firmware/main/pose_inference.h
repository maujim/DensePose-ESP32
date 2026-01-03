/**
 * @file pose_inference.h
 * @brief Pose estimation inference module for DensePose from WiFi
 *
 * This module implements the pose estimation pipeline inspired by the paper
 * "DensePose From WiFi" (arXiv:2301.00250).
 *
 * Architecture Overview:
 * ---------------------
 * The paper uses a temporal window of CSI data to predict DensePose UV coordinates.
 * For ESP32 constraints, we implement a simplified version:
 *
 * 1. CSI Input Buffer: Time-series of amplitude/phase features
 * 2. Feature Extraction: Statistical features across temporal window
 * 3. ML Model: Lightweight neural network for inference
 * 4. Output: Simplified pose representation (presence + basic pose classes)
 *
 * Memory Constraints:
 * -----------------
 * - Internal SRAM: 512KB (use for model weights and active buffers)
 * - PSRAM: 2MB (use for CSI history and temporary tensors)
 * - Flash: 4MB (store model weights)
 */

#ifndef POSE_INFERENCE_H
#define POSE_INFERENCE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for pose inference module
 */
typedef struct {
    int window_size_ms;        // Temporal window size (milliseconds)
    int sampling_rate_hz;      // Expected CSI sampling rate (Hz)
    int num_subcarriers;       // Number of WiFi subcarriers to use
    bool use_amplitude;        // Include amplitude features
    bool use_phase;            // Include phase features
    bool enable_presence_detection;  // Run human presence detection
    bool enable_pose_classification; // Run pose classification (advanced)
} pose_config_t;

/**
 * @brief Detection result: Human presence and basic activity
 */
typedef enum {
    POSE_EMPTY = 0,        // No human detected
    POSE_PRESENT = 1,      // Human present, static
    POSE_MOVING = 2,       // Human moving
    POSE_WALKING = 3,      // Walking pattern detected
    POSE_SITTING = 4,      // Sitting pattern detected
    POSE_STANDING = 5,     // Standing pattern detected
    POSE_UNKNOWN = 255     // Unable to classify
} pose_class_t;

/**
 * @brief Inference result structure
 */
typedef struct {
    bool human_detected;       // True if human presence detected
    pose_class_t pose_class;   // Classified pose/activity
    float confidence;          // Confidence score [0.0, 1.0]
    float motion_level;        // Motion intensity [0.0, 1.0]

    // Feature statistics (for debugging/analysis)
    float amplitude_mean;
    float amplitude_std;
    float phase_variance;

    uint32_t inference_time_ms;  // Time taken for inference
    uint32_t timestamp;          // Timestamp of result
} pose_result_t;

/**
 * @brief Callback function type for pose detection results
 *
 * @param result Pointer to inference result
 * @param user_ctx User context passed during registration
 */
typedef void (*pose_callback_t)(const pose_result_t *result, void *user_ctx);

/**
 * @brief Initialize pose estimation module
 *
 * Sets up the inference pipeline with default configuration optimized for ESP32-S3.
 * This includes:
 * - Allocating buffers in PSRAM for CSI history
 * - Loading model weights from flash
 * - Initializing the ML interpreter
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t pose_init(const pose_config_t *config);

/**
 * @brief Deinitialize pose estimation module
 *
 * Frees resources and stops inference.
 *
 * @return ESP_OK on success
 */
esp_err_t pose_deinit(void);

/**
 * @brief Register callback for pose detection results
 *
 * The callback will be invoked when a new inference result is available.
 *
 * @param callback Function to call with results
 * @param user_ctx User context passed to callback
 * @return ESP_OK on success
 */
esp_err_t pose_register_callback(pose_callback_t callback, void *user_ctx);

/**
 * @brief Process CSI data for pose estimation
 *
 * This function should be called from the WiFi CSI callback.
 * It maintains an internal buffer and runs inference when enough
 * data is collected.
 *
 * For the ESP32 implementation, we use a sliding window approach:
 * - Collect CSI samples over temporal window (e.g., 500ms)
 * - Extract features from the window
 * - Run ML inference
 * - Call registered callback with results
 *
 * @param csi_data CSI data (amplitude and phase arrays)
 * @param user_ctx User context (unused)
 * @return ESP_OK on success
 */
esp_err_t pose_process_csi(const float *amplitude, const float *phase,
                           int num_subcarriers, int8_t rssi);

/**
 * @brief Get the latest inference result
 *
 * Copies the most recent pose detection result into the provided buffer.
 * Use this for polling instead of callbacks.
 *
 * @param result Buffer to store result
 * @return ESP_OK if result available, ESP_ERR_NOT_FOUND if no inference yet
 */
esp_err_t pose_get_latest_result(pose_result_t *result);

/**
 * @brief Check if pose inference is active
 *
 * @return true if running
 */
bool pose_is_active(void);

/**
 * @brief Get inference statistics
 *
 * @param num_inferences Total number of inferences performed
 * @param avg_latency_ms Average inference latency in milliseconds
 */
void pose_get_stats(uint32_t *num_inferences, float *avg_latency_ms);

#ifdef __cplusplus
}
#endif

#endif // POSE_INFERENCE_H
