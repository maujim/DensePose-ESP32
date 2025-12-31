/**
 * @file led_status.h
 * @brief WS2812 RGB LED status indicator
 *
 * Uses the ESP32-S3 RMT peripheral to drive a WS2812 (NeoPixel) RGB LED.
 * The LED provides visual feedback about WiFi and CSI status:
 *
 * - RED slow blink:   WiFi disconnected / connecting
 * - BLUE pulse:       WiFi connected, waiting for CSI
 * - GREEN blink:      CSI data receiving (rate = packet rate)
 */

#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED status states
 */
typedef enum {
    LED_STATUS_WIFI_DISCONNECTED = 0,  // Red slow blink
    LED_STATUS_WIFI_CONNECTED,         // Blue slow pulse
    LED_STATUS_CSI_ACTIVE,             // Green blink (fast)
} led_status_t;

/**
 * @brief Initialize the RGB LED
 *
 * Sets up the RMT peripheral for WS2812 control.
 * Uses GPIO21 on the Waveshare ESP32-S3-Zero.
 *
 * @return ESP_OK on success
 */
esp_err_t led_status_init(void);

/**
 * @brief Set the LED status
 *
 * Changes the LED color and blink pattern based on status.
 *
 * @param status The new status to display
 * @return ESP_OK on success
 */
esp_err_t led_status_set(led_status_t status);

/**
 * @brief Update LED blink for CSI activity
 *
 * Call this for each CSI packet received. The LED will blink
 * based on the packet rate.
 *
 * @return ESP_OK on success
 */
esp_err_t led_status_csi_tick(void);

/**
 * @brief LED status task
 *
 * Handles LED animations. Runs continuously in background.
 */
void led_status_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // LED_STATUS_H
