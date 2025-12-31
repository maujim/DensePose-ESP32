/**
 * @file wifi_csi.h
 * @brief WiFi Channel State Information (CSI) collection module
 *
 * CSI is the "secret sauce" for WiFi-based sensing. When WiFi signals travel
 * from transmitter to receiver, they interact with the environment - bouncing
 * off walls, getting absorbed by human bodies, etc. CSI captures these
 * interactions as complex numbers (amplitude + phase) for each subcarrier.
 *
 * Think of it like this:
 * - WiFi uses many frequencies (subcarriers) within its channel
 * - Each subcarrier is affected differently by obstacles
 * - CSI tells us exactly how each subcarrier was affected
 * - Human bodies create distinctive patterns in CSI data
 */

#ifndef WIFI_CSI_H
#define WIFI_CSI_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CSI data structure for processed data
 *
 * Raw CSI comes as interleaved I/Q values. This structure holds
 * the processed amplitude and phase for each subcarrier.
 */
typedef struct {
    float amplitude[64];    // Amplitude for each subcarrier
    float phase[64];        // Phase (radians) for each subcarrier
    uint8_t num_subcarriers; // Actual number of valid subcarriers
    int8_t rssi;            // Received Signal Strength Indicator
    uint32_t timestamp;     // Timestamp in milliseconds
} csi_data_t;

/**
 * @brief Callback function type for processed CSI data
 *
 * Register a callback to receive processed CSI data.
 * Called from the CSI processing task, not from interrupt context.
 *
 * @param data Pointer to processed CSI data (valid only during callback)
 * @param user_ctx User context passed during registration
 */
typedef void (*csi_callback_t)(const csi_data_t *data, void *user_ctx);

/**
 * @brief Initialize WiFi CSI collection
 *
 * Sets up CSI configuration and registers the receive callback.
 * WiFi must be initialized and connected before calling this.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_csi_init(void);

/**
 * @brief Deinitialize WiFi CSI collection
 *
 * Stops CSI collection and frees resources.
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_csi_deinit(void);

/**
 * @brief Register a callback for processed CSI data
 *
 * @param callback Function to call with processed data
 * @param user_ctx User context passed to callback
 * @return ESP_OK on success
 */
esp_err_t wifi_csi_register_callback(csi_callback_t callback, void *user_ctx);

/**
 * @brief Get the latest CSI data
 *
 * Copies the most recent CSI data into the provided buffer.
 * Use this for polling instead of callbacks.
 *
 * @param data Buffer to store CSI data
 * @return ESP_OK if data available, ESP_ERR_NOT_FOUND if no data yet
 */
esp_err_t wifi_csi_get_latest(csi_data_t *data);

/**
 * @brief Check if CSI collection is active
 *
 * @return true if collecting CSI data
 */
bool wifi_csi_is_active(void);

/**
 * @brief Get CSI statistics
 *
 * @param packets_received Total packets received
 * @param packets_processed Total packets successfully processed
 * @param packets_dropped Total packets dropped due to queue full
 */
void wifi_csi_get_stats(uint32_t *packets_received, uint32_t *packets_processed,
                        uint32_t *packets_dropped);

#ifdef __cplusplus
}
#endif

#endif // WIFI_CSI_H
