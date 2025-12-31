/**
 * @file main.c
 * @brief DensePose-ESP32 Main Entry Point
 *
 * This is the entry point for the ESP32 firmware. The app_main() function
 * is called by the ESP-IDF framework after the system initializes.
 *
 * Key concepts for returning firmware developers:
 * - ESP-IDF uses FreeRTOS, so we work with tasks, not a main loop
 * - WiFi and most peripherals are event-driven
 * - Memory is precious: 512KB SRAM + 2MB PSRAM
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "wifi_csi.h"

// Logging tag - used to identify log messages from this file
static const char *TAG = "main";

// WiFi credentials - loaded from Kconfig (can be set via .env file)
#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASSWORD  CONFIG_WIFI_PASSWORD

// Event group to signal WiFi connection status
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Connection retry counter
static int s_retry_num = 0;
#define MAX_RETRY CONFIG_WIFI_MAXIMUM_RETRY

/**
 * @brief WiFi and IP event handler
 *
 * This callback is invoked by the ESP event loop when WiFi events occur.
 * Event-driven programming is the ESP-IDF way - no polling!
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                // WiFi station started, try to connect
                ESP_LOGI(TAG, "WiFi started, connecting to AP...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                // Disconnected - try to reconnect
                if (s_retry_num < MAX_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGW(TAG, "Retry connection (%d/%d)", s_retry_num, MAX_RETRY);
                } else {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    ESP_LOGE(TAG, "Failed to connect after %d attempts", MAX_RETRY);
                }
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Got IP address - we're connected!
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Initialize WiFi in station mode
 *
 * Station mode means we connect to an existing access point (router).
 * This is required for CSI collection - we need WiFi packets to analyze.
 */
static esp_err_t wifi_init_sta(void)
{
    esp_err_t ret;

    // Create event group for synchronization
    s_wifi_event_group = xEventGroupCreate();

    // Initialize the TCP/IP stack (required for WiFi)
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop (handles WiFi, IP, and other events)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station interface
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    // We want to know about WiFi events (connect/disconnect) and IP events (got IP)
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    // Configure WiFi connection parameters
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            // These settings help with connection stability
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    // Set mode to station and apply config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start WiFi - this triggers WIFI_EVENT_STA_START
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization complete, waiting for connection...");

    // Wait for connection or failure
    // xEventGroupWaitBits blocks until one of the bits is set
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,      // Don't clear bits on exit
        pdFALSE,      // Wait for ANY bit, not all
        portMAX_DELAY // Wait forever
    );

    // Check which bit was set
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Successfully connected to SSID: %s", WIFI_SSID);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", WIFI_SSID);
        return ESP_FAIL;
    }

    ESP_LOGE(TAG, "Unexpected event");
    return ESP_FAIL;
}

/**
 * @brief Print system information
 *
 * Useful for debugging - shows available memory, chip info, etc.
 */
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "=== System Information ===");
    ESP_LOGI(TAG, "Chip: ESP32-S3, %d cores, WiFi%s%s",
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    ESP_LOGI(TAG, "Silicon revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

    // Check PSRAM
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram_size > 0) {
        ESP_LOGI(TAG, "PSRAM: %zu bytes total, %zu bytes free",
                 psram_size, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    } else {
        ESP_LOGW(TAG, "PSRAM: Not available (enable in menuconfig)");
    }

    ESP_LOGI(TAG, "==========================");
}

/**
 * @brief Application entry point
 *
 * This is called by ESP-IDF after system initialization.
 * Unlike Arduino's setup()/loop(), we create tasks and return.
 * The FreeRTOS scheduler handles task execution.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "DensePose-ESP32 starting...");

    // Print system info for debugging
    print_system_info();

    // Initialize Non-Volatile Storage (NVS)
    // Required for WiFi to store calibration data
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was corrupted, erase and reinitialize
        ESP_LOGW(TAG, "NVS partition corrupted or version mismatch, erasing...");
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS flash: %s", esp_err_to_name(ret));
            return;
        }
        ret = nvs_flash_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize NVS after erase: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "NVS reinitialized successfully");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize WiFi in station mode
    ret = wifi_init_sta();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi initialization failed!");
        // In a real application, you might want to retry or enter a config mode
        return;
    }

    // Initialize CSI collection
    // This sets up callbacks to receive Channel State Information
    ret = wifi_csi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CSI initialization failed!");
        return;
    }

    ESP_LOGI(TAG, "Initialization complete. Collecting CSI data...");
    ESP_LOGI(TAG, "Streaming CSI data over serial (JSON format)...");

    // Main task can now do other work or just idle
    // CSI data is collected in callbacks, not in a loop
    while (1) {
        // Print memory stats periodically for debugging
        ESP_LOGI(TAG, "Free heap: %lu, min ever: %lu",
                 esp_get_free_heap_size(),
                 esp_get_minimum_free_heap_size());

        // Delay for 10 seconds
        // vTaskDelay is the FreeRTOS way to sleep - it yields to other tasks
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
