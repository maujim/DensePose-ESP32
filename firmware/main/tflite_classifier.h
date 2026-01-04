/**
 * @file tflite_classifier.h
 * @brief TensorFlow Lite Micro integration for pose classification
 *
 * This module provides a simple interface to run TFLite models on ESP32-S3
 * for WiFi-based pose estimation.
 *
 * Features:
 * - Load quantized INT8 models from flash
 * - Efficient inference using ESP-NN optimizations
 * - Minimal memory footprint (~100-200KB tensor arena)
 *
 * Usage:
 *   tflite_classifier_init();
 *   tflite_classifier_run(input_data, output_data);
 *   tflite_classifier_deinit();
 */

#ifndef TFLITE_CLASSIFIER_H
#define TFLITE_CLASSIFIER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// Model input/output dimensions
#define TFLITE_INPUT_SAMPLES 50      // Temporal window size
#define TFLITE_INPUT_FEATURES 104    // 52 amplitude + 52 phase
#define TFLITE_NUM_CLASSES 6         // Number of pose classes

/**
 * @brief Initialize TFLite Micro classifier
 *
 * Loads the model and initializes the interpreter.
 * The model should be compiled into the firmware as a binary array.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t tflite_classifier_init(void);

/**
 * @brief Run inference on input data
 *
 * @param input Input tensor (int8量化数据)
 *              Shape: (TFLITE_INPUT_SAMPLES, TFLITE_INPUT_FEATURES)
 *              Should be preprocessed and quantized to int8 range [-128, 127]
 *
 * @param output Output array for class probabilities
 *               Shape: (TFLITE_NUM_CLASSES,)
 *               Values are int8 quantized scores
 *
 * @return ESP_OK on success
 */
esp_err_t tflite_classifier_run(const int8_t *input, int8_t *output);

/**
 * @brief Get input tensor details
 *
 * @param out_size  Output: total size of input tensor in bytes
 * @param out_scale Output: quantization scale
 * @param out_zero_point Output: quantization zero point
 *
 * @return ESP_OK on success
 */
esp_err_t tflite_classifier_get_input_details(size_t *out_size,
                                               float *out_scale,
                                               int *out_zero_point);

/**
 * @brief Get output tensor details
 *
 * @param out_size  Output: total size of output tensor in bytes
 * @param out_scale Output: quantization scale
 * @param out_zero_point Output: quantization zero point
 *
 * @return ESP_OK on success
 */
esp_err_t tflite_classifier_get_output_details(size_t *out_size,
                                                float *out_scale,
                                                int *out_zero_point);

/**
 * @brief Clean up TFLite resources
 *
 * @return ESP_OK on success
 */
esp_err_t tflite_classifier_deinit(void);

/**
 * @brief Check if classifier is initialized
 *
 * @return true if ready
 */
bool tflite_classifier_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif // TFLITE_CLASSIFIER_H
