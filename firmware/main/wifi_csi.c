/**
 * @file wifi_csi.c
 * @brief WiFi CSI collection implementation
 *
 * This module handles the low-level CSI data collection from the ESP32's
 * WiFi hardware. The ESP32-S3 can extract CSI from received WiFi packets,
 * giving us insight into how signals propagate through the environment.
 *
 * CSI Data Format:
 * ----------------
 * Raw CSI comes as pairs of signed 8-bit integers: [I0, Q0, I1, Q1, ...]
 * - I = In-phase component
 * - Q = Quadrature component
 * - Together they form a complex number representing that subcarrier
 *
 * From I/Q we calculate:
 * - Amplitude = sqrt(I² + Q²)  -- signal strength
 * - Phase = atan2(Q, I)        -- signal timing
 *
 * Human bodies affect both amplitude (absorption) and phase (reflection/delay).
 */

#include "wifi_csi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *TAG = "wifi_csi";

// CSI configuration
// These settings control what CSI data we receive
static wifi_csi_config_t csi_config = {
    .lltf_en = true,           // Legacy Long Training Field - good for CSI
    .htltf_en = true,          // HT Long Training Field - 802.11n
    .stbc_htltf2_en = true,    // Space-Time Block Coding HT-LTF2
    .ltf_merge_en = true,      // Merge LTF data for better accuracy
    .channel_filter_en = true, // Apply channel filter
    .manu_scale = false,       // Automatic scaling (recommended)
    .shift = 0,                // No bit shift (was incorrectly 'false')
    .dump_ack_en = false,      // Don't dump ACK frames
};

// State variables
static bool s_csi_active = false;
static csi_data_t s_latest_csi;
static SemaphoreHandle_t s_csi_mutex = NULL;
static csi_callback_t s_user_callback = NULL;
static void *s_user_ctx = NULL;

// Statistics
static uint32_t s_packets_received = 0;
static uint32_t s_packets_processed = 0;

/**
 * @brief Process raw CSI buffer into amplitude/phase
 *
 * This is where the magic happens! We convert raw I/Q data into
 * meaningful amplitude and phase values.
 *
 * @param raw_buf Raw CSI buffer (I/Q interleaved)
 * @param len Buffer length in bytes
 * @param out Output structure for processed data
 */
static void process_csi_data(const int8_t *raw_buf, uint16_t len, csi_data_t *out)
{
    // Each subcarrier has 2 bytes (I and Q)
    uint8_t num_subcarriers = len / 2;
    if (num_subcarriers > 64) {
        num_subcarriers = 64;  // Cap at our buffer size
    }

    out->num_subcarriers = num_subcarriers;

    for (int i = 0; i < num_subcarriers; i++) {
        // Extract I and Q components
        // Note: These are signed 8-bit values (-128 to 127)
        int8_t I = raw_buf[i * 2];
        int8_t Q = raw_buf[i * 2 + 1];

        // Calculate amplitude: |H| = sqrt(I² + Q²)
        // Using float for simplicity; could optimize with fixed-point later
        out->amplitude[i] = sqrtf((float)(I * I + Q * Q));

        // Calculate phase: angle = atan2(Q, I)
        // Result is in radians, range [-π, π]
        out->phase[i] = atan2f((float)Q, (float)I);
    }
}

/**
 * @brief WiFi CSI receive callback
 *
 * This function is called by the WiFi driver when CSI data is available.
 * IMPORTANT: This runs in WiFi task context, so keep it fast!
 *
 * @param ctx User context (unused)
 * @param info CSI information structure
 */
static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    s_packets_received++;

    // Sanity check
    if (info == NULL || info->buf == NULL || info->len == 0) {
        return;
    }

    // Process the CSI data
    csi_data_t processed;
    process_csi_data(info->buf, info->len, &processed);

    // Add metadata
    processed.rssi = info->rx_ctrl.rssi;
    processed.timestamp = (uint32_t)(esp_timer_get_time() / 1000);  // Convert to ms

    // Store as latest (thread-safe)
    if (xSemaphoreTake(s_csi_mutex, 0) == pdTRUE) {
        memcpy(&s_latest_csi, &processed, sizeof(csi_data_t));
        xSemaphoreGive(s_csi_mutex);
        s_packets_processed++;
    }

    // Call user callback if registered
    if (s_user_callback != NULL) {
        s_user_callback(&processed, s_user_ctx);
    }

    // Stream CSI data over serial in JSON format
    // This allows real-time visualization and analysis on the laptop
    // Format: {"ts":12345,"rssi":-45,"num":64,"amp":[...],"phase":[...]}
    printf("{\"ts\":%lu,\"rssi\":%d,\"num\":%d,\"amp\":[",
           processed.timestamp, processed.rssi, processed.num_subcarriers);

    for (int i = 0; i < processed.num_subcarriers; i++) {
        printf("%.2f%s", processed.amplitude[i],
               (i < processed.num_subcarriers - 1) ? "," : "");
    }

    printf("],\"phase\":[");
    for (int i = 0; i < processed.num_subcarriers; i++) {
        printf("%.4f%s", processed.phase[i],
               (i < processed.num_subcarriers - 1) ? "," : "");
    }
    printf("]}\n");

    // Log occasionally for debugging (every 100 packets)
    if (s_packets_received % 100 == 0) {
        ESP_LOGD(TAG, "CSI packet #%lu: %d subcarriers, RSSI=%d dBm",
                 s_packets_received, processed.num_subcarriers, processed.rssi);

        // Print first few amplitude values for debugging
        ESP_LOGD(TAG, "Amplitudes[0-4]: %.1f, %.1f, %.1f, %.1f, %.1f",
                 processed.amplitude[0], processed.amplitude[1],
                 processed.amplitude[2], processed.amplitude[3],
                 processed.amplitude[4]);
    }
}

esp_err_t wifi_csi_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing WiFi CSI collection...");

    // Create mutex for thread-safe access to latest CSI data
    s_csi_mutex = xSemaphoreCreateMutex();
    if (s_csi_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Configure CSI
    ret = esp_wifi_set_csi_config(&csi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set CSI config: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register our callback
    ret = esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register CSI callback: %s", esp_err_to_name(ret));
        return ret;
    }

    // Enable CSI collection
    ret = esp_wifi_set_csi(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable CSI: %s", esp_err_to_name(ret));
        return ret;
    }

    s_csi_active = true;
    ESP_LOGI(TAG, "CSI collection initialized successfully");
    ESP_LOGI(TAG, "Config: lltf=%d, htltf=%d, filter=%d",
             csi_config.lltf_en, csi_config.htltf_en, csi_config.channel_filter_en);

    return ESP_OK;
}

esp_err_t wifi_csi_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing CSI collection...");

    // Disable CSI
    esp_wifi_set_csi(false);
    esp_wifi_set_csi_rx_cb(NULL, NULL);

    // Clean up mutex
    if (s_csi_mutex != NULL) {
        vSemaphoreDelete(s_csi_mutex);
        s_csi_mutex = NULL;
    }

    s_csi_active = false;
    s_user_callback = NULL;
    s_user_ctx = NULL;

    return ESP_OK;
}

esp_err_t wifi_csi_register_callback(csi_callback_t callback, void *user_ctx)
{
    s_user_callback = callback;
    s_user_ctx = user_ctx;
    ESP_LOGI(TAG, "User callback registered");
    return ESP_OK;
}

esp_err_t wifi_csi_get_latest(csi_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_packets_processed == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    if (xSemaphoreTake(s_csi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(data, &s_latest_csi, sizeof(csi_data_t));
        xSemaphoreGive(s_csi_mutex);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

bool wifi_csi_is_active(void)
{
    return s_csi_active;
}

void wifi_csi_get_stats(uint32_t *packets_received, uint32_t *packets_processed)
{
    if (packets_received != NULL) {
        *packets_received = s_packets_received;
    }
    if (packets_processed != NULL) {
        *packets_processed = s_packets_processed;
    }
}
