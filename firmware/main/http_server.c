/**
 * @file http_server.c
 * @brief HTTP web server for CSI visualization implementation
 */

#include "http_server.h"
#include "wifi_csi.h"
#include "html_data.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "http_server";

// HTTP server handle
static httpd_handle_t s_server = NULL;

// Queue to pass CSI data from WiFi callback to HTTP server
// Store only the essential data to save memory
typedef struct {
    uint32_t timestamp;
    int8_t rssi;
    uint8_t num_subcarriers;
    float amplitude[64];  // Only amplitude, phase not needed for display
} csi_http_data_t;

static QueueHandle_t s_csi_queue = NULL;
#define CSI_QUEUE_SIZE 10

// Forward declarations
static esp_err_t index_handler(httpd_req_t *req);
static esp_err_t csi_stream_handler(httpd_req_t *req);
static esp_err_t stats_handler(httpd_req_t *req);

/**
 * @brief Callback from CSI module to queue data for HTTP streaming
 */
static void csi_http_callback(const csi_data_t *data, void *user_ctx)
{
    if (s_csi_queue == NULL) {
        return;
    }

    csi_http_data_t http_data = {
        .timestamp = data->timestamp,
        .rssi = data->rssi,
        .num_subcarriers = data->num_subcarriers,
    };

    // Copy only the amplitudes we need
    uint8_t copy_len = (data->num_subcarriers > 64) ? 64 : data->num_subcarriers;
    memcpy(http_data.amplitude, data->amplitude, copy_len * sizeof(float));

    // Send to queue (don't block if full, just drop)
    xQueueSend(s_csi_queue, &http_data, 0);
}

/**
 * @brief Handler for the index page - serves the HTML
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving index page");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_data, strlen(html_data));
    return ESP_OK;
}

/**
 * @brief Handler for SSE stream - sends CSI data in real-time
 */
static esp_err_t csi_stream_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "CSI stream client connected");

    // Setup SSE
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // Send initial message
    const char *start_msg = "event: connected\ndata: {\"status\":\"connected\"}\n\n";
    httpd_resp_sendstr_chunk(req, start_msg);

    // Stream CSI data
    csi_http_data_t csi_data;
    char buffer[512];

    while (1) {
        // Wait for CSI data (with timeout to check connection)
        if (xQueueReceive(s_csi_queue, &csi_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Format as SSE data
            // Simplified - send only every 5th subcarrier to reduce bandwidth
            int len = snprintf(buffer, sizeof(buffer),
                "data: {\"ts\":%lu,\"rssi\":%d,\"num\":%d,\"amp\":[",
                csi_data.timestamp, csi_data.rssi, csi_data.num_subcarriers);

            // Send every 4th subcarrier (16 values instead of 64)
            for (int i = 0; i < csi_data.num_subcarriers; i += 4) {
                len += snprintf(buffer + len, sizeof(buffer) - len, "%.1f%s",
                    csi_data.amplitude[i],
                    (i + 4 < csi_data.num_subcarriers) ? "," : "");
            }

            len += snprintf(buffer + len, sizeof(buffer) - len, "]}\n\n");

            httpd_resp_sendstr_chunk(req, buffer);
        } else {
            // Send keep-alive comment every second
            httpd_resp_sendstr_chunk(req, ": keep-alive\n\n");
        }
    }

    ESP_LOGI(TAG, "CSI stream client disconnected");
    return ESP_OK;
}

/**
 * @brief Handler for stats endpoint - returns system statistics
 */
static esp_err_t stats_handler(httpd_req_t *req)
{
    char buffer[512];
    uint32_t packets_rx = 0, packets_proc = 0;

    wifi_csi_get_stats(&packets_rx, &packets_proc);

    int len = snprintf(buffer, sizeof(buffer),
        "{"
        "\"free_heap\":%lu,"
        "\"min_free_heap\":%lu,"
        "\"packets_received\":%lu,"
        "\"packets_processed\":%lu,"
        "\"uptime\":%lu,"
        "\"model\":\"ESP32-S3\""
        "}",
        esp_get_free_heap_size(),
        esp_get_minimum_free_heap_size(),
        packets_rx,
        packets_proc,
        xTaskGetTickCount() * portTICK_PERIOD_MS / 1000
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, buffer, len);
    return ESP_OK;
}

esp_err_t http_server_init(void)
{
    ESP_LOGI(TAG, "Initializing HTTP server...");

    // Create CSI data queue
    s_csi_queue = xQueueCreate(CSI_QUEUE_SIZE, sizeof(csi_http_data_t));
    if (s_csi_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create CSI queue");
        return ESP_ERR_NO_MEM;
    }

    // Register callback to get CSI data
    wifi_csi_register_callback(csi_http_callback, NULL);

    // HTTP server configuration
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.task_priority = tskIDLE_PRIORITY + 3;
    config.stack_size = 4096;
    config.max_open_sockets = 3;  // We don't need many
    config.lru_purge_enable = true;  // Purge old connections

    // Start the server
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        vQueueDelete(s_csi_queue);
        s_csi_queue = NULL;
        return ret;
    }

    // Register URI handlers
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &index_uri);

    httpd_uri_t csi_uri = {
        .uri = "/csi",
        .method = HTTP_GET,
        .handler = csi_stream_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &csi_uri);

    httpd_uri_t stats_uri = {
        .uri = "/stats",
        .method = HTTP_GET,
        .handler = stats_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &stats_uri);

    ESP_LOGI(TAG, "HTTP server started on port 80");
    return ESP_OK;
}

bool http_server_is_running(void)
{
    return s_server != NULL;
}

esp_err_t http_server_stop(void)
{
    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    if (s_csi_queue != NULL) {
        vQueueDelete(s_csi_queue);
        s_csi_queue = NULL;
    }
    return ESP_OK;
}
