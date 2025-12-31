/**
 * @file http_server.h
 * @brief HTTP web server for CSI visualization
 *
 * Provides a simple HTTP server with Server-Sent Events (SSE)
 * for streaming CSI data to a web browser.
 *
 * Routes:
 * - GET /         - Main HTML page with real-time visualization
 * - GET /csi      - SSE stream of CSI data
 * - GET /stats    - JSON system statistics
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the HTTP server
 *
 * Starts the web server on port 80.
 *
 * @return ESP_OK on success
 */
esp_err_t http_server_init(void);

/**
 * @brief Check if HTTP server is running
 *
 * @return true if server is active
 */
bool http_server_is_running(void);

/**
 * @brief Stop the HTTP server
 *
 * @return ESP_OK on success
 */
esp_err_t http_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_H
