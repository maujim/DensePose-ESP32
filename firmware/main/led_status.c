/**
 * @file led_status.c
 * @brief WS2812 RGB LED status indicator implementation
 *
 * Uses RMT (Remote Control) peripheral with bytes encoder for WS2812.
 * Based on ESP-IDF v5+ RMT API.
 */

#include "led_status.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "led_status";

// Hardware configuration
#define LED_GPIO_PIN         21     // WS2812 data pin on ESP32-S3-Zero

// RMT configuration - 1MHz resolution = 1 tick per microsecond
#define RMT_RESOLUTION_HZ    1000000

// State
static rmt_channel_handle_t s_led_channel = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;
static led_status_t s_current_status = LED_STATUS_WIFI_DISCONNECTED;
static uint32_t s_csi_tick_count = 0;

// WS2812 timing configuration at 1MHz resolution (1 tick = 1us)
// WS2812: 800kHz protocol, T0H=0.35us, T0L=0.8us, T1H=0.7us, T1L=0.6us
static const rmt_bytes_encoder_config_t ws2812_encoder_config = {
    .bit0 = {
        .level0 = 1,
        .duration0 = 0.35 * RMT_RESOLUTION_HZ,  // T0H = 0.35us
        .level1 = 0,
        .duration1 = 0.80 * RMT_RESOLUTION_HZ,  // T0L = 0.80us
    },
    .bit1 = {
        .level0 = 1,
        .duration0 = 0.70 * RMT_RESOLUTION_HZ,  // T1H = 0.70us
        .level1 = 0,
        .duration1 = 0.60 * RMT_RESOLUTION_HZ,  // T1L = 0.60us
    },
    .flags.msb_first = 1,  // WS2812 wants MSB first
};

// LED colors (GRB format for WS2812)
#define COLOR_RED     ((uint8_t[]){0, 255, 0})    // G=0, R=255, B=0
#define COLOR_GREEN   ((uint8_t[]){255, 0, 0})    // G=255, R=0, B=0
#define COLOR_BLUE    ((uint8_t[]){0, 0, 255})    // G=0, R=0, B=255
#define COLOR_OFF     ((uint8_t[]){0, 0, 0})      // All off

// Set LED color
static esp_err_t led_set_color(const uint8_t *grb)
{
    if (s_led_channel == NULL || s_led_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // WS2812 expects 24 bits: GRB (8 bits each)
    uint8_t payload[3] = {grb[0], grb[1], grb[2]};

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,  // No loop
    };

    esp_err_t ret = rmt_transmit(s_led_channel, s_led_encoder, payload, sizeof(payload), &tx_config);
    if (ret != ESP_OK) {
        return ret;
    }

    // Wait for transmission to complete
    return rmt_tx_wait_all_done(s_led_channel, pdMS_TO_TICKS(100));
}

esp_err_t led_status_init(void)
{
    ESP_LOGI(TAG, "Initializing WS2812 RGB LED on GPIO %d...", LED_GPIO_PIN);

    esp_err_t ret;

    // Create RMT bytes encoder for WS2812 protocol
    ret = rmt_new_bytes_encoder(&ws2812_encoder_config, &s_led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create bytes encoder: %s", esp_err_to_name(ret));
        return ret;
    }

    // RMT TX channel configuration
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = LED_GPIO_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    ret = rmt_new_tx_channel(&tx_chan_config, &s_led_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Enable the channel
    ret = rmt_enable(s_led_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initial color test
    led_set_color(COLOR_RED);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_set_color(COLOR_GREEN);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_set_color(COLOR_BLUE);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_set_color(COLOR_OFF);

    ESP_LOGI(TAG, "RGB LED initialized");
    return ESP_OK;
}

esp_err_t led_status_set(led_status_t status)
{
    s_current_status = status;
    s_csi_tick_count = 0;
    return ESP_OK;
}

esp_err_t led_status_csi_tick(void)
{
    s_csi_tick_count++;
    return ESP_OK;
}

void led_status_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LED status task started");

    bool led_on = false;
    uint32_t last_state_change = xTaskGetTickCount();

    while (1) {
        uint32_t now = xTaskGetTickCount();
        uint32_t ms_since_last = (now - last_state_change) * portTICK_PERIOD_MS;

        switch (s_current_status) {
            case LED_STATUS_WIFI_DISCONNECTED:
                // Red slow blink (500ms on, 500ms off)
                if (ms_since_last >= 500) {
                    led_on = !led_on;
                    led_set_color(led_on ? COLOR_RED : COLOR_OFF);
                    last_state_change = now;
                }
                break;

            case LED_STATUS_WIFI_CONNECTED:
                // Blue slow pulse (200ms on, 800ms off)
                if (ms_since_last >= (led_on ? 200 : 800)) {
                    led_on = !led_on;
                    led_set_color(led_on ? COLOR_BLUE : COLOR_OFF);
                    last_state_change = now;
                }
                break;

            case LED_STATUS_CSI_ACTIVE:
                // Green blink on CSI activity
                // If we received CSI packets recently, blink briefly
                if (s_csi_tick_count > 0) {
                    led_set_color(COLOR_GREEN);
                    vTaskDelay(pdMS_TO_TICKS(20));  // Brief flash
                    led_set_color(COLOR_OFF);
                    s_csi_tick_count = 0;  // Reset counter
                }
                // Small idle delay
                vTaskDelay(pdMS_TO_TICKS(50));
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz update rate
    }
}
